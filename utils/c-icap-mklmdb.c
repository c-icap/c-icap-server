#include "common.h"
#include "c-icap.h"
#include "lookup_table.h"
#include "cfg_param.h"
#include "debug.h"
#include "util.h"
#include <lmdb.h>

MDB_env *env_db = NULL;
MDB_dbi db = 0;
const ci_type_ops_t *key_type = &ci_str_ops;
const ci_type_ops_t *val_type = &ci_str_ops;

#define MAXLINE 65535

char *INFILE = NULL;
char *DBNAME = NULL;
char *DBPATH = NULL;
int ERASE_MODE = 0;
int DUMP_MODE = 0;
int VERSION_MODE = 0;

#if CI_SIZEOF_VOID_P < 8
/*For 32bit systems use 64MB maximum size*/
long int DB_MAX_SIZE = 64*1024*1024;
#else
/* 64 bit system use 1GB*/
long int DB_MAX_SIZE = 1*1024*1024*1024;
#endif



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
        "-i", "in_file", &INFILE, ci_cfg_set_str,
        "The input file to load key/value pairs"
    },
    {
        "-p", "db_directory", &DBPATH, ci_cfg_set_str,
        "The database directory path (required)"
    },
    {
        "-n", "db_name", &DBNAME, ci_cfg_set_str,
        "The lmdb database name to use (optional)"
    },
    {
        "-t", "string|int|ip",NULL, cfg_set_type,
        "The type of the key (default is string)"
    },
    {
        "-v", "string|int|ip", NULL, cfg_set_type,
        "The type of values (default is string)"
    },
    {
        "-M", "max_size", &DB_MAX_SIZE, ci_cfg_size_long,
        "The maximum database size to use for the database"
    },
    {
        "--dump", NULL, &DUMP_MODE, ci_cfg_enable,
        "Do not update the database just dump it to the screen"
    },
    {
        "--erase", NULL, &ERASE_MODE, ci_cfg_enable,
        "Erase the keys/items listed in input file"
    },
    {NULL, NULL, NULL, NULL}
};

int open_db()
{
    int ret;
    /* * Create an environment and initialize it for additional error * reporting. */
    if ((ret = mdb_env_create(&env_db)) != 0) {
        ci_debug_printf(1, "mb_env_create  failed\n");
        return 0;
    }

    if (DB_MAX_SIZE) {
        ret = mdb_env_set_mapsize(env_db, (size_t)DB_MAX_SIZE);
        if (ret != 0) {
            ci_debug_printf(1, "mb_env_set_mapsize  failed to set mapsize/maximum-size to %lld\n", (long long)DB_MAX_SIZE);
        }
    }

    ci_debug_printf(5, "mdb_env_create: Environment created OK.\n");
    mdb_env_set_maxdbs(env_db, 10);
    /* Open the environment  */
    if ((ret = mdb_env_open(env_db, DBPATH, 0, S_IRUSR | S_IWUSR | S_IRGRP)) != 0) {
        ci_debug_printf(1, "mdb_env_open: Environment open failed: %s\n", mdb_strerror(ret));
        mdb_env_close(env_db);
        return 0;
    }
    ci_debug_printf(5, "mdb_env_open: DB environment setup OK.\n");

    MDB_txn *txn;
    if ((ret = mdb_txn_begin(env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "Can not create transaction for dump data\n");
        return 0;
    }
    unsigned int flags = 0;
    if (!DUMP_MODE)
        flags = MDB_CREATE;
    if ((ret = mdb_dbi_open(txn, DBNAME, flags, &db)) != 0) {
        ci_debug_printf(1, "open lmdb database %s/%s: %s\n", DBPATH, DBNAME, mdb_strerror(ret));
        mdb_dbi_close(env_db, db);
        return 0;
    }

    mdb_txn_commit(txn);
    ci_debug_printf(5, "lmdb open: Database %s successfully %s.\n", DBPATH, (flags == MDB_CREATE ? "created" : "opened"));
    return 1;
}

void close_db()
{
    mdb_dbi_close(env_db, db);
    mdb_env_close(env_db);
}

int dump_db()
{
    MDB_cursor *dbc;
    MDB_val db_key, db_data;
    int ret, i;
    void *flat;
    MDB_txn *txn;

    ci_debug_printf(3, "Going to dump database!\n");

    if (key_type != &ci_str_ops ||val_type != &ci_str_ops) {
        ci_debug_printf(1, "can not dump not string databases\n");
        return 0;
    }

    if ((ret = mdb_txn_begin(env_db, NULL, MDB_RDONLY, &txn)) != 0) {
        ci_debug_printf(1, "Can not create transaction to dump data\n");
        return 0;
    }

    if ((ret = mdb_cursor_open(txn, db, &dbc)) != 0) {
        ci_debug_printf(1, "error creating cursor\n");
        mdb_txn_abort(txn);
        return 0;
    }

    memset(&db_data, 0, sizeof(db_data));
    memset(&db_key, 0, sizeof(db_key));

    if ((ret = mdb_cursor_get(dbc, &db_key, &db_data, MDB_FIRST)) != 0) {
        ci_debug_printf(1, "error getting first element of DB : %s\n", mdb_strerror(ret));
        mdb_cursor_close(dbc);
        mdb_txn_abort(txn);
        return 0;
    }

    do {
        printf("%s :", (char *)db_key.mv_data);
        if (db_data.mv_data) {
            flat = db_data.mv_data;
            size_t item_size;
            const void *item;
            for (i = 0; (item = ci_flat_array_item(flat, i, &item_size)) != NULL; i++) {
                const char *val = (char *)(item);
                printf("'%s' | ", val);
            }
        }
        printf("\n");
        ret = mdb_cursor_get(dbc, &db_key, &db_data, MDB_NEXT);
        if (ret != 0 && ret != MDB_NOTFOUND) {
            ci_debug_printf(1, "Abort dump with the error: %s\n", mdb_strerror(ret));
        }
    } while (ret == 0);

    mdb_cursor_close(dbc);
    mdb_txn_abort(txn);
    return 1;
}

int store_db(void *key, int keysize, void *val, int  valsize)
{
    MDB_val db_key, db_data;
    int ret;
    MDB_txn *txn;
    if ((ret = mdb_txn_begin(env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "Can not create a transaction to store data\n");
        return 0;
    }

    memset(&db_key, 0, sizeof(db_key));
    memset(&db_data, 0, sizeof(db_data));
    db_key.mv_data = key;
    db_key.mv_size = keysize;

    db_data.mv_data = val;
    db_data.mv_size = valsize;

    ret = mdb_put(txn, db, &db_key, &db_data, MDB_NODUPDATA );
    if (ret != 0)
        ci_debug_printf(1, "mdb_put: %s (key size:%d, val size:%d)\n",
                        mdb_strerror(ret), keysize, valsize);
    mdb_txn_commit(txn);
    return 1;
}

void erase_from_db(void *key, int keysize)
{
    MDB_val db_key;
    int ret;
    MDB_txn *txn;
    if ((ret = mdb_txn_begin(env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "Can not create transaction for deleting data\n");
        return;
    }

    memset(&db_key, 0, sizeof(db_key));
    db_key.mv_data = key;
    db_key.mv_size = keysize;
    ret = mdb_del(txn, db, &db_key, NULL);
    if (ret != 0)
        ci_debug_printf(1, "erase_from_db/mdb_del: %s (key size:%d)\n",
                        mdb_strerror(ret), keysize);
    mdb_txn_commit(txn);
}

int cfg_set_type(const char *directive, const char **argv, void *setdata)
{
    const ci_type_ops_t *type_ops = &ci_str_ops;

    if (argv[0] == NULL) {
        ci_debug_printf(1, "error not argument for %s argument\n", argv[0]);
        return 0;
    }

    if (0 == strcmp(argv[0], "string")) {
        type_ops = &ci_str_ops;
    } else if (0 == strcmp(argv[0], "int")) {
        ci_debug_printf(1, "%s: not implemented type %s\n", directive, argv[0]);
        return 0;
    } else if (0 == strcmp(argv[0], "ip")) {
        ci_debug_printf(1, "%s: not implemented type %s\n", directive, argv[0]);
        return 0;
    }

    if (0 == strcmp(directive, "-t")) {
        key_type = type_ops;
    } else if (0 == strcmp(directive, "-v")) {
        val_type = type_ops;
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
    char line[MAXLINE];
    void *key, *val;
    size_t keysize;

    CI_DEBUG_LEVEL = 1;
    ci_mem_init();
    ci_cfg_lib_init();

    if (!ci_args_apply(argc, argv, options) || (!DBPATH && !DUMP_MODE && !VERSION_MODE)) {
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

    if (DUMP_MODE && !DBPATH) {
        ci_debug_printf(1, "\nError: You need to specify the database to dump ('-o file.db')\n\n");
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (!open_db()) {
        ci_debug_printf(1, "Error opening lmdb database %s\n", DBPATH);
        if (f)
            fclose(f);
        return -1;
    }

    if (DUMP_MODE) {
        dump_db();
    } else {
        if ((f = fopen(INFILE, "r+")) == NULL) {
            ci_debug_printf(1, "Error opening file: %s\n", INFILE);
            return -1;
        }

        unsigned lines = 0, stored = 0, parse_fails = 0, store_fails = 0, removed = 0;
        while (fgets(line,MAXLINE,f)) {
            lines++;
            line[MAXLINE-1]='\0';
            ci_vector_t *values = NULL;
            if (ci_parse_key_mvalues(line, ':', ',', key_type, val_type, &key, &keysize,  &values) < 0) {
                ci_debug_printf(1, "Error parsing line : %s\n", line);
                parse_fails++;
                break;
            } else if (key) {
                if (ERASE_MODE) {
                    erase_from_db(key, keysize);
                    removed++;
                } else {
                    val = ci_flat_array_build_from_vector(values);
                    if (val && store_db(key, keysize, val, ci_flat_array_size(val)))
                        stored++;
                    else
                        store_fails++;
                }
                if (key) {
                    allocator->free(allocator, key);
                    key = NULL;
                }
                if (values) {
                    ci_vector_destroy(values);
                    values = NULL;
                }
            }
        }
        fclose(f);
        ci_debug_printf(1, "Lines processed %u\n", lines);
        ci_debug_printf(1, "Lines ignored (comments, blank lines, parse errors etc) %u\n", parse_fails);
        ci_debug_printf(1, "Stored keys %u\n", stored);
        ci_debug_printf(1, "Removed keys %u\n", removed);
        ci_debug_printf(1, "Failed to store keys %u\n", store_fails);
    }
    close_db();
    ci_mem_allocator_destroy(allocator);
    return 0;
}
