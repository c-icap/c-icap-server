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

int FATAL = 0;
char *INFILE = NULL;
char *DBNAME = NULL;
char *DBPATH = NULL;
int ERASE_MODE = 0;
int DUMP_MODE = 0;
int INFO_MODE = 0;
int VERSION_MODE = 0;
int CONTINUE_ON_ERROR = 0;

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
        "-C", NULL, &CONTINUE_ON_ERROR, ci_cfg_enable,
        "Do not abort on error"
    },
    {
        "--dump", NULL, &DUMP_MODE, ci_cfg_enable,
        "Do not update the database just dump it to the screen"
    },
    {
        "--erase", NULL, &ERASE_MODE, ci_cfg_enable,
        "Erase the keys/items listed in input file"
    },
    {
        "--info", NULL, &INFO_MODE, ci_cfg_enable,
        "Print information about database"
    },
    {NULL, NULL, NULL, NULL}
};

int open_db()
{
    int ret;
    int rdOnly = DUMP_MODE || INFO_MODE;
    /* * Create an environment and initialize it for additional error * reporting. */
    if ((ret = mdb_env_create(&env_db)) != 0) {
        ci_debug_printf(1, "mb_env_create  failed\n");
        return 0;
    }

    if (!rdOnly && DB_MAX_SIZE) {
        ret = mdb_env_set_mapsize(env_db, (size_t)DB_MAX_SIZE);
        if (ret != 0) {
            ci_debug_printf(1, "mb_env_set_mapsize  failed to set mapsize/maximum-size to %lld\n", (long long)DB_MAX_SIZE);
        }
    }

    ci_debug_printf(5, "mdb_env_create: Environment created OK.\n");
    mdb_env_set_maxdbs(env_db, 10);
    /* Open the environment  */
    if ((ret = mdb_env_open(env_db, DBPATH, (rdOnly ? MDB_RDONLY : 0), S_IRUSR | S_IWUSR | S_IRGRP)) != 0) {
        ci_debug_printf(1, "mdb_env_open: Environment open failed: %s\n", mdb_strerror(ret));
        mdb_env_close(env_db);
        return 0;
    }
    ci_debug_printf(5, "mdb_env_open: DB environment setup OK.\n");

    MDB_txn *txn;
    if ((ret = mdb_txn_begin(env_db, NULL, (rdOnly ? MDB_RDONLY : 0), &txn)) != 0) {
        ci_debug_printf(1, "Can not create transaction to open database\n");
        return 0;
    }
    unsigned int flags = rdOnly ? 0 : MDB_CREATE;
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

void printStat(MDB_stat *stat, char *label)
{
    if (!stat)
        return;
    printf("%s\n"
           "   Page size %u\n"
           "   Btree depth: %u\n"
           "   Branch pages: %lld\n"
           "   Leaf pages: %lld\n"
           "   Overflow pages: %lld\n"
           "   Entries: %lld\n",
           label,
           stat->ms_psize, stat->ms_depth,
           (long long)stat->ms_branch_pages, (long long)stat->ms_leaf_pages, (long long)stat->ms_overflow_pages, (long long)stat->ms_entries
        );
}

int info_db()
{
    MDB_envinfo info;
    mdb_env_info(env_db, &info);
    printf("Database environment info\n"
           "   Mapsize: %lld\n"
           "   Max readers: %d\n"
           "   Readers: %d\n", (long long) info.me_mapsize, info.me_maxreaders, info.me_numreaders);
    MDB_stat envstat;
    mdb_env_stat(env_db, &envstat);
    printStat(&envstat, "Database environment statistics");
    MDB_stat dbstat;
    MDB_txn *txn;
    int ret;
    if ((ret = mdb_txn_begin(env_db, NULL, MDB_RDONLY, &txn)) != 0) {
        ci_debug_printf(1, "error creating cursor\n");
        mdb_txn_abort(txn);
        return 0;
    }
    mdb_stat(txn, db, &dbstat);
    printStat(&dbstat, "Database statistics");
    mdb_txn_abort(txn);
    return 1;
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
        /*Empty DB?*/
        ci_debug_printf(3, "error getting first element of DB : %s\n", mdb_strerror(ret));
        mdb_cursor_close(dbc);
        mdb_txn_abort(txn);
        return 0;
    }

    int records = 0;
    int errors = 0;
    do {
        records++;
        printf("%s :", (char *)db_key.mv_data);
        if (db_data.mv_size && db_data.mv_data) {
            flat = db_data.mv_data;
            if (!ci_flat_array_check(flat)) {
                errors++;
                printf(" unknown_data_of_size_%d", (int)db_data.mv_size);
            } else {
                size_t item_size;
                const void *item;
                for (i = 0; (item = ci_flat_array_item(flat, i, &item_size)) != NULL; i++) {
                    const char *val = (char *)(item);
                    printf("%s'%s'", (i > 0 ? "| " : ""), val);
                }
            }
        }
        printf("\n");
        ret = mdb_cursor_get(dbc, &db_key, &db_data, MDB_NEXT);
        if (ret != 0 && ret != MDB_NOTFOUND) {
            ci_debug_printf(1, "Abort dump with the error: %s\n", mdb_strerror(ret));
        }
    } while (ret == 0);

    if (errors) {
        if (!DBNAME && errors == records) {
            /*Probably unnamed database which include records with named databases, do not report anything*/
        } else {
            ci_debug_printf(1, "Not valid c-icap lookup table records %d from %d. Is the DB corrupted? \n", errors, records);
        }
    }

    mdb_cursor_close(dbc);
    mdb_txn_abort(txn);
    return 1;
}

int store_db(MDB_txn *useTxn, void *key, int keysize, void *val, int  valsize)
{
    MDB_val db_key, db_data;
    int ret;
    MDB_txn *txn;
    if (useTxn) {
        txn = useTxn;
    } else if ((ret = mdb_txn_begin(env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "Can not create a transaction to store data\n");
        FATAL = 1;
        return 0;
    }

    memset(&db_key, 0, sizeof(db_key));
    memset(&db_data, 0, sizeof(db_data));
    db_key.mv_data = key;
    db_key.mv_size = keysize;

    db_data.mv_data = val;
    db_data.mv_size = valsize;

    ret = mdb_put(txn, db, &db_key, &db_data, MDB_NODUPDATA );
    if (ret != 0) {
        ci_debug_printf(1, "mdb_put: %s (key %.*s key size:%d, val size:%d)\n",
                        mdb_strerror(ret), keysize, (key_type == &ci_str_ops ? (const char *) key : "-"), keysize, valsize);
        if (ret != MDB_KEYEXIST && !CONTINUE_ON_ERROR) {
            FATAL = 1;
        }
    }
    if (!useTxn) {
        if (ret == 0)
            mdb_txn_commit(txn);
        else
            mdb_txn_abort(txn);
    }
    return (ret == 0 ? 1 : 0);
}

int erase_from_db(MDB_txn *useTxn, void *key, int keysize)
{
    MDB_val db_key;
    int ret;
    MDB_txn *txn;
    if (useTxn) {
        txn = useTxn;
    } else if ((ret = mdb_txn_begin(env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "Can not create transaction for deleting data\n");
        FATAL = 1;
        return 0;
    }

    memset(&db_key, 0, sizeof(db_key));
    db_key.mv_data = key;
    db_key.mv_size = keysize;
    ret = mdb_del(txn, db, &db_key, NULL);
    if (ret != 0) {
        ci_debug_printf(1, "erase_from_db/mdb_del: %s (key %.*s key size:%d)\n",
                        mdb_strerror(ret), keysize, (key_type == &ci_str_ops ? (const char *) key : "-"), keysize);
        if (ret != MDB_NOTFOUND && !CONTINUE_ON_ERROR) {
            FATAL = 1;
        }
    }
    if (!useTxn) {
        if (ret == 0)
            mdb_txn_commit(txn);
        else
            mdb_txn_abort(txn);
    }
    return (ret == 0 ? 1 : 0);
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
    size_t keysize, valsize;

    CI_DEBUG_LEVEL = 1;
    ci_mem_init();
    ci_cfg_lib_init();

    if (!ci_args_apply(argc, argv, options) || (!DBPATH && !DUMP_MODE && !VERSION_MODE && !INFO_MODE)) {
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

    if ((INFO_MODE || DUMP_MODE) && !DBPATH) {
        ci_debug_printf(1, "\nError: You need to specify the database ('-p file.db')\n\n");
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (!open_db()) {
        ci_debug_printf(1, "Error opening lmdb database %s\n", DBPATH);
        if (f)
            fclose(f);
        return -1;
    }

    if (INFO_MODE) {
        info_db();
    } else if (DUMP_MODE) {
        dump_db();
    } else {
        if ((f = fopen(INFILE, "r+")) == NULL) {
            ci_debug_printf(1, "Error opening file: %s\n", INFILE);
            return -1;
        }

        MDB_txn *txn = NULL;
        int ret = mdb_txn_begin(env_db, NULL, 0, &txn);
        if (ret != 0) {
            ci_debug_printf(1, "Can not create transaction to %s data\n", ERASE_MODE ? "erase" : "add");
            return -1;
        }
        unsigned lines = 0, stored = 0, parse_fails = 0, store_fails = 0, removed = 0, removed_fails = 0;
        while (!FATAL && fgets(line,MAXLINE,f)) {
            lines++;
            line[MAXLINE-1]='\0';
            ci_vector_t *values = NULL;
            if (ci_parse_key_mvalues(line, ':', ',', key_type, val_type, &key, &keysize,  &values) < 0) {
                ci_debug_printf(1, "Error parsing line : %s\n", line);
                parse_fails++;
                break;
            } else if (key) {
                if (ERASE_MODE) {
                    if ((ret = erase_from_db(txn, key, keysize)))
                        removed++;
                    else
                        removed_fails++;
                } else {
                    val = values ? ci_flat_array_build_from_vector(values) : NULL;
                    valsize = val ? ci_flat_array_size(val) : 0;
                    if ((ret = store_db(txn, key, keysize, val, valsize)))
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
        if (FATAL)
            mdb_txn_abort(txn);
        else
            mdb_txn_commit(txn);
        ci_debug_printf(1, "Lines processed %u\n", lines);
        ci_debug_printf(1, "Lines ignored (comments, blank lines, parse errors etc) %u\n", parse_fails);
        ci_debug_printf(1, "Stored keys %u\n", stored);
        ci_debug_printf(1, "Removed keys %u\n", removed);
        ci_debug_printf(1, "Failed to store keys %u\n", store_fails);
        ci_debug_printf(1, "Failed to removed keys %u\n", removed_fails);
    }
    close_db();
    ci_mem_allocator_destroy(allocator);
    return 0;
}
