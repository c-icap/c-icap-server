#include "common.h"
#include "c-icap.h"
#include "array.h"
#include "module.h"
#include "lookup_table.h"
#include "commands.h"
#include "debug.h"
#include "util.h"

#include <lmdb.h>

int init_lmdb_tables(struct ci_server_conf *server_conf);
void release_lmdb_tables();

static common_module_t lmdb_module = {
    "lmdb_tables",
    init_lmdb_tables,
    NULL,
    release_lmdb_tables,
    NULL,
};
_CI_DECLARE_COMMON_MODULE(lmdb_module)

void *lmdb_table_open(struct ci_lookup_table *table);
void  lmdb_table_close(struct ci_lookup_table *table);
void *lmdb_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  lmdb_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type lmdb_table_type = {
    lmdb_table_open,
    lmdb_table_close,
    lmdb_table_search,
    lmdb_table_release_result,
    NULL,
    "lmdb"
};

int init_lmdb_tables(struct ci_server_conf *server_conf)
{
    return (ci_lookup_table_type_register(&lmdb_table_type) != NULL);
}

void release_lmdb_tables()
{
    ci_debug_printf(3, "Module lmdb going down\n");
    ci_lookup_table_type_unregister(&lmdb_table_type);
}

/***********************************************************/
/*  lmdb_table_type inmplementation                         */

typedef struct lmdb_txn_pool {
    ci_thread_mutex_t mtx;
    ci_thread_cond_t cnd;
    ci_list_t *pool;
    int stat_readers_full;
} lmdb_txn_pool_t;

struct lmdb_data {
    MDB_env *env_db;
    MDB_dbi db;
    char *name;
    int readers;
    lmdb_txn_pool_t pool;
    int stat_failures;
    int stat_hit;
    int stat_miss;
};

static  MDB_txn *lmdb_txn_pool_get_reader(MDB_env *env_db, lmdb_txn_pool_t *pool)
{
    int ret;
    if (!env_db)
        return NULL;
    if (!pool || !pool->pool)
        return NULL; /*Should assert?*/

    const char *dbpath = NULL;
    if (mdb_env_get_path(env_db, &dbpath) != 0)
        dbpath = "[unknown]";
    MDB_txn *txn = NULL;
    int wait_list = 0;
    int tries = 10;
    do {
        ci_thread_mutex_lock(&pool->mtx);
        if (wait_list)
            ci_thread_cond_wait(&pool->cnd, &pool->mtx);
        ci_list_pop(pool->pool, &txn);
        ci_thread_mutex_unlock(&pool->mtx);

        if (txn) {
            ci_debug_printf(8, "lmdb_tables/lmdb_txn_pool_get_reader: db '%s' git transaction from pool\n", dbpath);
            ret = mdb_txn_renew(txn);
            if (ret != 0) {
                ci_debug_printf(1, "lmdb_tables/lmdb_txn_pool_get_reader: db '%s', wrong transaction object in pool: %s\n", dbpath, mdb_strerror(ret));
                mdb_txn_abort(txn);
                txn = NULL;
            }
        }

        if (txn == NULL && !wait_list) {
            /*Pool is empty. Try only once to build a txn.
              If fails return error or if we reach maximum readers
              wait one to be available */
            if ((ret = mdb_txn_begin(env_db, NULL, MDB_RDONLY, &txn)) != 0) {
                if (ret == MDB_READERS_FULL) {
                    ci_stat_uint64_inc(pool->stat_readers_full, 1);
                    wait_list = 1;
                } else {
                    ci_debug_printf(1, "lmdb_tables/mdb_txn_begin: db '%s', can not create transaction object: %s\n", dbpath, mdb_strerror(ret));
                    return NULL;
                }
            }
        }
        /*If txn is nil probably condition aborted by a signal, retry*/
        tries--;
    } while (txn == NULL && tries > 0);

    if (!txn) {
        ci_debug_printf(1, "lmdb_tables/lmdb_txn_pool_get_reader: db '%s', can not create or retrieve from pool a transaction object\n", dbpath);
    }
    return txn;
}

static void lmdb_txn_pool_push_txn(lmdb_txn_pool_t *pool, MDB_txn *txn)
{
    mdb_txn_reset(txn);
    ci_thread_mutex_lock(&pool->mtx);
    if (ci_list_first(pool->pool) == NULL)
        ci_thread_cond_signal(&pool->cnd); /*pool is empty, maybe there are waiters*/
    ci_list_push(pool->pool, &txn);
    ci_thread_mutex_unlock(&pool->mtx);

}

static void lmdb_txn_pool_init(lmdb_txn_pool_t *pool)
{
    ci_thread_mutex_init(&pool->mtx);
    ci_thread_cond_init(&pool->cnd);
    pool->pool = ci_list_create(2048, sizeof(void *));
}

static void lmdb_txn_pool_mkempty(lmdb_txn_pool_t *pool)
{
    if (!pool || !pool->pool)
        return;

    MDB_txn *txn = NULL;
    const char *dbpath = NULL;
    int n = 0;
    while(ci_list_pop(pool->pool, &txn)) {
        if (dbpath == NULL)
            mdb_env_get_path(mdb_txn_env(txn), &dbpath);
        mdb_txn_abort(txn);
        n++;
    }
    if (n) {
        if (!dbpath) dbpath = "[unknwon]";
        ci_debug_printf(3, "lmdb_table txn pool db: %s released, %d transactions in pool\n", dbpath, n);
    }
}

static void lmdb_txn_pool_destroy(lmdb_txn_pool_t *pool)
{
    lmdb_txn_pool_mkempty(pool);
    ci_list_destroy(pool->pool);
}

int lmdb_table_do_real_open(struct ci_lookup_table *table)
{
    int ret;
    struct lmdb_data *dbdata = table->data;

    if (!dbdata) {
        ci_debug_printf(1, "Lmdb table %s is not initialized?\n", table->path);
        return 0;
    }
    if (dbdata->db || dbdata->env_db) {
        ci_debug_printf(1, "lmdb table %s already open?\n", table->path);
        return 0;
    }

    /*Create an environment and initialize it for additional error reporting. */
    if ((ret = mdb_env_create(&dbdata->env_db)) != 0) {
        ci_debug_printf(1, "mdb_env_create  failed: %s\n", mdb_strerror(ret));
        return 0;
    }
    mdb_env_set_maxdbs  (dbdata->env_db, 10);/*Make it configurable. In any case here we need 1 database?*/
    if (dbdata->readers >0 && (ret = mdb_env_set_maxreaders(dbdata->env_db, dbdata->readers)) != 0) {
        ci_debug_printf(1, "WARNING: mdb_env_set_maxreaders  failed: %s\n", mdb_strerror(ret));
    }

    ci_debug_printf(5, "lmdb_table_open: Environment created OK.\n");
    /* Open the environment  */
    if ((ret = mdb_env_open(dbdata->env_db, table->path, MDB_NOTLS, 0)) != 0) {
        ci_debug_printf(1, "lmdb_table_open: Environment open failed: %s\n", mdb_strerror(ret));
        mdb_env_close(dbdata->env_db);
        dbdata->env_db = NULL;
        return 0;
    }
    ci_debug_printf(5, "lmdb_table_open: DB environment setup OK.\n");

    MDB_txn *txn = NULL;
    if ((ret = mdb_txn_begin(dbdata->env_db, NULL, 0, &txn)) != 0) {
        ci_debug_printf(1, "lmdb_table_open: Can not create transaction: %s\n", mdb_strerror(ret));
        mdb_env_close(dbdata->env_db);
        dbdata->env_db = NULL;
        return 0;
    }
    if ((ret = mdb_dbi_open(txn, dbdata->name, 0, &dbdata->db)) != 0) {
        ci_debug_printf(1, "open db %s/%s: %s\n", table->path, dbdata->name, mdb_strerror(ret));
        mdb_dbi_close(dbdata->env_db, dbdata->db);
        mdb_env_close(dbdata->env_db);
        dbdata->env_db = 0;
        dbdata->db = 0;
        return 0;
    }
    mdb_txn_commit(txn);
    return 1;
}

void lmdb_data_reset(struct lmdb_data *dbdata)
{
    lmdb_txn_pool_mkempty(&dbdata->pool);
    mdb_dbi_close(dbdata->env_db, dbdata->db);
    mdb_env_close(dbdata->env_db);
    dbdata->env_db = NULL;
    dbdata->db = 0;
}

void command_real_open_table(const char *name, int type, void *data)
{
    struct ci_lookup_table *table = data;
    lmdb_table_do_real_open(table);
}

void *lmdb_table_open(struct ci_lookup_table *table)
{
    int i;
    ci_dyn_array_t *args = NULL;
    const ci_array_item_t *arg = NULL;
    struct lmdb_data *dbdata = malloc(sizeof(struct lmdb_data));
    if (!dbdata)
        return NULL;

    table->data = dbdata;
    dbdata->env_db = NULL;
    dbdata->db = 0;
    dbdata->name = NULL;
    dbdata->readers = 0;
    lmdb_txn_pool_init(&dbdata->pool);

    if (table->args) {
        if ((args = ci_parse_key_value_list(table->args, ','))) {
            for (i = 0; (arg = ci_dyn_array_get_item(args, i)) != NULL; ++i) {
                if (strcasecmp(arg->name, "name") == 0) {
                    dbdata->name = strdup(arg->value);
                } else if (strcasecmp(arg->name, "readers") == 0) {
                    const char *errStr = NULL;
                    dbdata->readers = ci_atol_ext(arg->value, &errStr);
                    if (dbdata->readers <= 0 || errStr) {
                        ci_debug_printf(1, "WARNING:lmdb_table_open, db '%s', wrong parameter '%s=%s', ignoring\n", table->path, arg->name, (char *)arg->value);
                        dbdata->readers = 0;
                    }
                } else {
                    ci_debug_printf(1, "WARNING:lmdb_table_open, db '%s', wrong parameter '%s=%s', ignoring\n", table->path, arg->name, (char *)arg->value);
                }
            }
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "lmdb(%s:%s)_error_readers_full",table->path, dbdata->name);
    dbdata->pool.stat_readers_full = ci_stat_entry_register(buf, CI_STAT_INT64_T, "lmdb_lookup_table");
    snprintf(buf, sizeof(buf), "lmdb(%s:%s)_errors",table->path, dbdata->name);
    dbdata->stat_failures = ci_stat_entry_register(buf, CI_STAT_INT64_T, "lmdb_lookup_table");
    snprintf(buf, sizeof(buf), "lmdb(%s:%s)_hits",table->path, dbdata->name);
    dbdata->stat_hit = ci_stat_entry_register(buf, CI_STAT_INT64_T, "lmdb_lookup_table");
    snprintf(buf, sizeof(buf), "lmdb(%s:%s)_miss",table->path, dbdata->name);
    dbdata->stat_miss = ci_stat_entry_register(buf, CI_STAT_INT64_T, "lmdb_lookup_table");

    /*We can not use an MDB_env after fork, we have to open lmdb tables
      separately for every child, on start-up procedure.
      To check the database state open and close it immediately.
    */
    int ret;
    ret = lmdb_table_do_real_open(table);
    if (!ret) {
        lmdb_table_close(table);
        return NULL;
    }
    lmdb_data_reset(dbdata);
    ci_command_register_action("openLMDBtable", CHILD_START_CMD, table, command_real_open_table);
    return table->data;
}

void  lmdb_table_close(struct ci_lookup_table *table)
{
    struct lmdb_data *dbdata;
    dbdata = table->data;
    if (dbdata) {
        ci_debug_printf(3, "lmdb_table_close %s/%s, will be closed\n", table->path, dbdata->name);
        if (dbdata->env_db)
            lmdb_data_reset(dbdata);
        free(dbdata->name);
        free(table->data);
        lmdb_txn_pool_destroy(&dbdata->pool);
        table->data = NULL;
    } else {
        ci_debug_printf(3,"lmdb_table_close, table %s is not open?\n", table->path);
    }
}

#define DATA_SIZE 32768
#define LMDB_MAX_COLS 1024 /*Shound be: LMDB_MAX_COLS*sizeof(void *) < DATA_SIZE */

void *lmdb_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    MDB_val db_key, db_data;
    int ret;
    struct lmdb_data *dbdata = (struct lmdb_data *)table->data;

    if (!dbdata) {
        ci_debug_printf(1,"table %s is not initialized?\n", table->path);
        ci_stat_uint64_inc(dbdata->stat_failures, 1);
        return NULL;
    }

    if (!dbdata->db) {
        ci_debug_printf(1,"table %s is not open?\n", table->path);
        ci_stat_uint64_inc(dbdata->stat_failures, 1);
        return NULL;
    }

    MDB_txn *txn = lmdb_txn_pool_get_reader(dbdata->env_db, &dbdata->pool);
    if (!txn)
        return NULL; /*Error messages generated inside lmdb_txn_pool_get_reader*/
    *vals = NULL;
    memset(&db_data, 0, sizeof(db_data));
    memset(&db_key, 0, sizeof(db_key));
    db_key.mv_data = key;
    db_key.mv_size = table->key_ops->size(key);
    if ((ret = mdb_get(txn, dbdata->db, &db_key, &db_data)) != 0) {
        ci_stat_uint64_inc(dbdata->stat_miss, 1);
        ci_debug_printf(5, "db_entry_exists does not exists: %s\n", mdb_strerror(ret));
        *vals = NULL;
        mdb_txn_abort(txn);
        return NULL;
    }

    if (db_data.mv_size) {
        void *store = NULL;
        if (ci_flat_array_size(db_data.mv_data) <= db_data.mv_size&& ci_flat_array_check(db_data.mv_data)) {
            store = ci_buffer_alloc(db_data.mv_size);
            _CI_ASSERT(store);
            memcpy(store, db_data.mv_data, db_data.mv_size);
            *vals = ci_flat_array_to_ppvoid(store, NULL);
        }
        if (!(*vals)){
            if (store)
                ci_buffer_free(store);
            ci_debug_printf(1, "Error while parsing data in lmdb_table_search.Is this a c-icap lmdb table?\n");
            ci_stat_uint64_inc(dbdata->stat_failures, 1);
        }
    }
    lmdb_txn_pool_push_txn(&dbdata->pool, txn);
    ci_stat_uint64_inc(dbdata->stat_hit, 1);
    return key;
}

void  lmdb_table_release_result(struct ci_lookup_table *table,void **val)
{
    ci_buffer_free(val);
}
