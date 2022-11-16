#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include "c-icap.h"
#include "lookup_table.h"
#include "cfg_param.h"
#include "debug.h"
#include BDB_HEADER_PATH(db.h)

DB_ENV *env_db = NULL;
DB *db = NULL;
const ci_type_ops_t *key_ops = &ci_str_ops;
const ci_type_ops_t *val_ops = &ci_str_ops;

#define MAXLINE 65535

char *txtfile = NULL;
char *dbfile = NULL;
int DUMP_MODE = 0;
int VERSION_MODE = 0;
int USE_DBTREE = 0;
long int PAGE_SIZE;

ci_mem_allocator_t *allocator = NULL;
int cfg_set_type(const char *directive, const char **argv, void *setdata);

static struct ci_options_entry options[] = {
    {"-V", NULL, &VERSION_MODE, ci_cfg_version, "Print version and exits"},
    {"-VV", NULL, &VERSION_MODE, ci_cfg_build_info, "Print version and build informations and exits"},
    {
        "-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-i", "file.txt", &txtfile, ci_cfg_set_str,
        "The file contains the data (required)"
    },
    {
        "-o", "file.db", &dbfile, ci_cfg_set_str,
        "The database to be created"
    },
    {
        "-t", "string|int|ip",NULL, cfg_set_type,
        "The type of the key"
    },
    {
        "-v", "string|int|ip", NULL, cfg_set_type,
        "The type of values"
    },
    {
        "-p", "page_size", &PAGE_SIZE, ci_cfg_size_long,
        "The page size to use for the database"
    },
    {
        "--btree", NULL, &USE_DBTREE, ci_cfg_enable,
        "Use B-Tree for indexes instead of Hash"
    },
    {
        "--dump", NULL, &DUMP_MODE, ci_cfg_enable,
        "Do not update the database just dump it to the screen"
    },
    {NULL, NULL, NULL, NULL}
};



int open_db(char *path)
{
    char *s,home[CI_MAX_PATH];
    int ret;
    strncpy(home, path,CI_MAX_PATH);
    home[CI_MAX_PATH-1] = '\0';
    s = strrchr(home,'/');
    if (s)
        *s = '\0';
    else /*no path in filename?*/
        home[0]='\0';

    /* * Create an environment and initialize it for additional error * reporting. */
    if ((ret = db_env_create(&env_db, 0)) != 0) {
        return 0;
    }

    ci_debug_printf(5, "bdb_table_open: Environment created OK.\n");


    env_db->set_data_dir(env_db, home);
    ci_debug_printf(5, "bdb_table_open: Data dir set to %s.\n", home);

    /* Open the environment  */
    if ((ret = env_db->open(env_db, home,
                            DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL /*| DB_SYSTEM_MEM*/,
                            0)) != 0) {
        ci_debug_printf(1, "bdb_table_open: Environment open failed: %s\n", db_strerror(ret));
        env_db->close(env_db, 0);
        return 0;
    }
    ci_debug_printf(5, "bdb_table_open: DB environment setup OK.\n");


    if ((ret = db_create(&db, env_db, 0)) != 0) {
        ci_debug_printf(1, "db_create: %s\n", db_strerror(ret));
        return 0;
    }

    if (PAGE_SIZE > 512 && PAGE_SIZE <= 64*1024)
        db->set_pagesize(db, (uint32_t)PAGE_SIZE);

    if ((ret = db->open(db, NULL, path, NULL,
                        (USE_DBTREE ? DB_BTREE : DB_HASH),
                        DB_CREATE /*| DB_TRUNCATE*/, 0664)) != 0) {
        ci_debug_printf(1, "open db %s: %s\n", path, db_strerror(ret));
        db->close(db, 0);
        return 0;
    }

    ci_debug_printf(5, "bdb_table_open: file %s created OK.\n",path);
    return 1;
}

void close_db()
{
    db->close(db,0);
    env_db->close(env_db,0);
}

int dump_db()
{
    DBC *dbc;
    DBT db_key, db_data;
    int ret, i;
    void *store;
    void **store_index;

    printf("Going to dump database!\n");

    if (key_ops != &ci_str_ops ||val_ops != &ci_str_ops) {
        ci_debug_printf(1, "can not dump not string databases\n");
        return 0;
    }

    if ((ret = db->cursor(db, NULL, &dbc, 0)) != 0) {
        ci_debug_printf(1, "error creating cursor\n");
        return 0;
    }

    memset(&db_data, 0, sizeof(db_data));
    memset(&db_key, 0, sizeof(db_key));

    if ((ret = dbc->c_get(dbc, &db_key, &db_data, DB_FIRST)) != 0) {
        ci_debug_printf(1, "error getting first element of DB : %s\n", db_strerror(ret));
        dbc->c_close(dbc);
        return 0;
    }

    do {
        printf("%s :", (char *)db_key.data);
        if (db_data.data) {
            store = db_data.data;
            store_index = store;
            for (i = 0; store_index[i] != 0; i++) {
                store_index[i]+=(long int)store;
            }
            for (i = 0; store_index[i] != 0; i++) {
                printf("%s |", (char *)store_index[i]);
            }
        }
        printf("\n");
        ret = dbc->c_get(dbc, &db_key, &db_data, DB_NEXT);
    } while (ret == 0);

    dbc->c_close(dbc);
    return 1;
}

int record_extract(char *line,
                   void **key, int *keysize,
                   void **val, int *valsize)
{
    char *s, *v, *e;
    void *avalue;
    void *store, *store_value;
    void **store_index;
    int i, row_cols = 0, avalue_size, store_value_size;

    *key = NULL;
    *val = NULL;
    *keysize = 0;
    *valsize = 0;

    if (!(s = index(line,':'))) {
        row_cols = 1;
    } else {
        row_cols = 2;
        while ((s = index(s,','))) row_cols++,s++;
    }

    /*eat spaces .....*/
    s = line;
    while (*s == ' ' || *s == '\t') s++;
    v = s;

    if (*s == '#') /*it is a comment*/
        return 1;

    if (*s == '\0') /*it is a blank line*/
        return 1;

    if (row_cols == 1)
        e = s + strlen(s);
    else
        e = index(s,':');

    s = e+1; /*Now points to the end (*s = '\0') or after the ':' */

    e--;
    while (*e == ' ' || *e == '\t' || *e == '\n') e--;
    *(e+1) = '\0';

    (*key) = key_ops->dup(v, allocator);
    (*keysize) = key_ops->size(*key);

    if (row_cols > 1) {

        if (row_cols > 128) /*More than 128 cols?*/
            return -1;

        /*We are going to store the data part of the db as folows:
          [indx1,indx2,indx3,NULL,val1.....,val2...]
          indx*: are of type void* and has size of sizeof(void*).
          val* are the values
         */

        /*Allocate a enough mem for storing the values*/
        (*val) = allocator->alloc(allocator, 65535);
        /*We need row_cols elements for storing pointers to values + 1 element for
          NULL termination ellement*/
        store = (*val);
        store_index = (*val);
        store_value = (*val) + row_cols*sizeof(void *);
        store_value_size = 65535 - row_cols*sizeof(void *);
        (*valsize) = row_cols*sizeof(void *);

        for (i = 0; *s != '\0' && i< row_cols-1; i++) { /*we have vals*/

            while (*s == ' ' || *s =='\t') s++; /*find the start of the string*/
            v = s;
            e = s;
            while (*e != ',' && *e != '\0') e++;
            if (*e == '\0')
                s = e;
            else
                s = e + 1;

            e--;
            while (*e == ' ' || *e == '\t' || *e == '\n') e--;
            *(e+1) = '\0';
            avalue = val_ops->dup(v, allocator);
            avalue_size = val_ops->size(avalue);

            if ((*valsize)+avalue_size >= store_value_size) {
                allocator->free(allocator,avalue);
                store_index[i] = 0;
                return -1;
            }
            memcpy(store_value, avalue, avalue_size);
            /*Put on the index the position of the current */
            store_index[i] = (void *)(store_value - store);

            void *apos = store + (long int)store_index[i];

            printf("\t\t- Storing val:%s at pos:%p(%s:%d)\n", (char *)avalue, store_index[i],
                   (char *)(apos),avalue_size);
            store_value += avalue_size;
            (*valsize) += avalue_size;
            allocator->free(allocator,avalue);
            avalue = NULL;
        }
        store_index[i]=0;
    } else {
        *val = NULL;;
        *valsize = 0;
    }
    return 1;
}

void store_db(void *key, int keysize, void *val, int  valsize)
{
    DBT db_key, db_data;
    int ret;
    memset(&db_key, 0, sizeof(db_key));
    memset(&db_data, 0, sizeof(db_data));
    db_key.data = key;
    db_key.size = keysize;

    db_data.data = val;
    db_data.size = valsize;

    ret = db->put(db, NULL, &db_key, &db_data, 0);
    if (ret!=0)
        ci_debug_printf(1, "db_create: %s (key size:%d, val size:%d)\n",
                        db_strerror(ret), keysize, valsize);
}


int cfg_set_type(const char *directive, const char **argv, void *setdata)
{
    const ci_type_ops_t *ops = &ci_str_ops;

    if (argv[0] == NULL) {
        ci_debug_printf(1, "error not argument for %s argument\n", argv[0]);
        return 0;
    }

    if (0 == strcmp(argv[0], "string")) {
        ops = &ci_str_ops;
    } else if (0 == strcmp(argv[0], "int")) {
        ci_debug_printf(1, "%s: not implemented type %s\n", directive, argv[0]);
        return 0;
    } else if (0 == strcmp(argv[0], "ip")) {
        ci_debug_printf(1, "%s: not implemented type %s\n", directive, argv[0]);
        return 0;
    }

    if (0 == strcmp(directive, "-t")) {
        key_ops = ops;
    } else if (0 == strcmp(directive, "-v")) {
        val_ops = ops;
    }
    return 1;
}

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

void vlog_errors(void *unused, const char *format, va_list ap)
{
    vfprintf(stderr, format, ap);
}




int main(int argc, char **argv)
{
    FILE *f = NULL;
    char outfile[CI_MAX_PATH];
    char line[MAXLINE];
    int len;
    void *key, *val;
    int keysize,valsize;

    CI_DEBUG_LEVEL = 1;
    ci_cfg_lib_init();

    if (!ci_args_apply(argc, argv, options) || (!txtfile && !DUMP_MODE && !VERSION_MODE)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    if (VERSION_MODE)
        exit(0);

#if ! defined(_WIN32)
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */
#else
    __vlog_error = vlog_errors;        /*set c-icap library  log function for win32..... */
#endif

    if (!(allocator = ci_create_os_allocator())) {
        ci_debug_printf(1, "Error allocating mem allocator!\n");
        return -1;
    }

    if (DUMP_MODE && !dbfile) {
        ci_debug_printf(1, "\nError: You need to specify the database to dump ('-o file.db')\n\n");
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (!dbfile) {
        strncpy(outfile, txtfile, CI_MAX_PATH);
        outfile[CI_MAX_PATH-1] = '\0';
        len=strlen(outfile);
        if (len > CI_MAX_PATH-5) {
            ci_debug_printf(1,"The filename  %s is too long\n", outfile);
            exit(0);
        }
        strcat(outfile,".db");
    } else {
        strncpy(outfile, dbfile, CI_MAX_PATH);
        outfile[CI_MAX_PATH-1] = '\0';
    }

    if (!open_db(outfile)) {
        ci_debug_printf(1, "Error opening bdb file %s\n", outfile);
        if (f)
            fclose(f);
        return -1;
    }

    if (DUMP_MODE) {
        dump_db();
    } else {
        if ((f = fopen(txtfile, "r+")) == NULL) {
            ci_debug_printf(1, "Error opening file: %s\n", txtfile);
            return -1;
        }

        while (fgets(line,MAXLINE,f)) {
            line[MAXLINE-1]='\0';
            if (!record_extract(line, &key, &keysize, &val, &valsize)) {
                ci_debug_printf(1, "Error parsing line : %s\n", line);
                break;
            } else if (key) /*if it is not comment or blank line */
                store_db(key, keysize, val, valsize);
        }
        fclose(f);
    }

    close_db();

    ci_mem_allocator_destroy(allocator);
    return 0;
}
