#include "common.h"
#include "c-icap.h"
#include "module.h"
#include "lookup_table.h"
#include "commands.h"
#include "debug.h"
#include "util.h"
#include BDB_HEADER_PATH(db.h)


int init_bdb_tables(struct ci_server_conf *server_conf);
void release_bdb_tables();

static common_module_t bdb_module = {
    "bdb_tables",
    init_bdb_tables,
    NULL,
    release_bdb_tables,
    NULL,
};
_CI_DECLARE_COMMON_MODULE(bdb_module)


void *bdb_table_open(struct ci_lookup_table *table);
void  bdb_table_close(struct ci_lookup_table *table);
void *bdb_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  bdb_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type bdb_table_type = {
    bdb_table_open,
    bdb_table_close,
    bdb_table_search,
    bdb_table_release_result,
    NULL,
    "bdb"
};


int init_bdb_tables(struct ci_server_conf *server_conf)
{
    return (ci_lookup_table_type_register(&bdb_table_type) != NULL);
}

void release_bdb_tables()
{
    ci_lookup_table_type_unregister(&bdb_table_type);
}

/***********************************************************/
/*  bdb_table_type inmplementation                         */

struct bdb_data {
    DB_ENV *env_db;
    DB *db;
};


int bdb_table_do_real_open(struct ci_lookup_table *table)
{
    int ret, i;
    char *s,home[CI_MAX_PATH];
    ci_dyn_array_t *args = NULL;
    const ci_array_item_t *arg = NULL;
    uint32_t cache_size = 0;
    int caches_num = 0;
    long int val;
    struct bdb_data *dbdata = table->data;

    if (!dbdata) {
        ci_debug_printf(1, "Db table %s is not initialized?\n", table->path);
        return 0;
    }
    if (dbdata->db || dbdata->env_db) {
        ci_debug_printf(1, "Db table %s already open?\n", table->path);
        return 0;
    }

    strncpy(home,table->path,CI_MAX_PATH);
    home[CI_MAX_PATH-1] = '\0';
    s=strrchr(home,'/');
    if (s)
        *s = '\0';
    else /*no path in filename?*/
        home[0] = '\0';

    if (table->args) {
        if ((args = ci_parse_key_value_list(table->args, ','))) {
            for (i = 0; (arg = ci_dyn_array_get_item(args, i)) != NULL; ++i) {
                if (strcasecmp(arg->name, "cache-size") == 0) {
                    val = ci_atol_ext((char *)arg->value, NULL);
                    if (val > 0 && val < 1*1024*1024*1024)
                        cache_size = (uint32_t)val;
                    else
                        ci_debug_printf(1, "WARNING: wrong cache-size value: %ld, will not set\n", val);
                }
                if (strcasecmp(arg->name, "cache-num") == 0) {
                    val = strtol(arg->value, NULL, 10);
                    if (val > 0 && val < 20)
                        caches_num = (uint32_t)val;
                    else
                        ci_debug_printf(1, "WARNING: wrong cache-num value: %ld, will not set\n", val);
                }

            }
        }
    }

    /* * Create an environment and initialize it for additional error * reporting. */
    if ((ret = db_env_create(&dbdata->env_db, 0)) != 0) {
        return 0;
    }
    ci_debug_printf(5, "bdb_table_open: Environment created OK.\n");


    dbdata->env_db->set_data_dir(dbdata->env_db, home);
    ci_debug_printf(5, "bdb_table_open: Data dir set to %s.\n", home);

    /* Open the environment  */
    if ((ret = dbdata->env_db->open(dbdata->env_db, home,
                                    DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL|DB_THREAD /*| DB_SYSTEM_MEM*/,
                                    0)) != 0) {
        ci_debug_printf(1, "bdb_table_open: Environment open failed: %s\n", db_strerror(ret));
        dbdata->env_db->close(dbdata->env_db, 0);
        dbdata->env_db = NULL;
        return 0;
    }
    ci_debug_printf(5, "bdb_table_open: DB environment setup OK.\n");


    if ((ret = db_create(&dbdata->db, dbdata->env_db, 0)) != 0) {
        ci_debug_printf(1, "db_create: %s\n", db_strerror(ret));
        dbdata->db = NULL;
        dbdata->env_db->close(dbdata->env_db, 0);
        dbdata->env_db = NULL;
        return 0;
    }

    if (cache_size > 0 &&
            (ret = dbdata->db->set_cachesize(dbdata->db, 0, cache_size, caches_num)) != 0) {
        ci_debug_printf(1, "db_create failed to set cache size: %s\n", db_strerror(ret));
    }

#if (DB_VERSION_MAJOR > 4) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    if ((ret = dbdata->db->open( dbdata->db, NULL, table->path, NULL,
                                 DB_BTREE, DB_RDONLY|DB_THREAD, 0)) != 0) {
#else
    if ((ret = dbdata->db->open( dbdata->db, table->path, NULL,
                                 DB_BTREE, DB_RDONLY, 0)) != 0) {
#endif
        ci_debug_printf(1, "open db %s: %s\n", table->path, db_strerror(ret));
        dbdata->db->close(dbdata->db, 0);
        dbdata->db = NULL;
        dbdata->env_db->close(dbdata->env_db, 0);
        dbdata->env_db = NULL;
        return 0;
    }

    return 1;
}


void command_real_open_table(const char *name, int type, void *data)
{
    struct ci_lookup_table *table = data;
    bdb_table_do_real_open(table);
}

void *bdb_table_open(struct ci_lookup_table *table)
{
    struct bdb_data *dbdata = malloc(sizeof(struct bdb_data));
    if (!dbdata)
        return NULL;
    dbdata->env_db = NULL;
    dbdata->db = NULL;
    table->data = dbdata;

    /*We can not fork a Berkeley DB table, so we have to
      open bdb tables for every child, on childs start-up procedure*/
    ci_command_register_action("openBDBtable", CHILD_START_CMD, table,
			       command_real_open_table);

    return table->data;
}

void  bdb_table_close(struct ci_lookup_table *table)
{
    struct bdb_data *dbdata;
    dbdata = table->data;
    if (dbdata && dbdata->db && dbdata->env_db) {

        dbdata->db->close(dbdata->db,0);
        dbdata->env_db->close(dbdata->env_db,0);
        free(table->data);
        table->data = NULL;
    } else {
        ci_debug_printf(3,"table %s is not open?\n", table->path);
    }
}

#define DATA_SIZE 32768
#define BDB_MAX_COLS 1024 /*Shound be: BDB_MAX_COLS*sizeof(void *) < DATA_SIZE */

void *bdb_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    void *store;
    void **store_index;
    void *endstore;
    DBT db_key, db_data;
    int ret, i, parse_error = 0;
    struct bdb_data *dbdata = (struct bdb_data *)table->data;

    if (!dbdata) {
        ci_debug_printf(1,"table %s is not initialized?\n", table->path);
        return NULL;
    }

    if (!dbdata->db) {
        ci_debug_printf(1,"table %s is not open?\n", table->path);
        return NULL;
    }

    *vals = NULL;
    memset(&db_data, 0, sizeof(db_data));
    memset(&db_key, 0, sizeof(db_key));
    db_key.data = key;
    db_key.size = table->key_ops->size(key);

    db_data.flags = DB_DBT_USERMEM;
    db_data.data = ci_buffer_alloc(DATA_SIZE);
    db_data.size = DATA_SIZE;

    if ((ret = dbdata->db->get(dbdata->db, NULL, &db_key, &db_data, 0)) != 0) {
        ci_debug_printf(5, "db_entry_exists does not exists: %s\n", db_strerror(ret));
        *vals = NULL;
        return NULL;
    }

    if (db_data.size) {
        store = db_data.data;
        store_index = store;
        endstore = store+db_data.size;
        for (i = 0; store_index[i] != NULL && i < BDB_MAX_COLS && !parse_error; i++) {
            store_index[i] = store+(unsigned long int)store_index[i];
            if (store_index[i] > endstore)
                parse_error = 1;
        }
        if (!parse_error)
            *vals = store;
        else {
            ci_debug_printf(1, "Error while parsing data in bdb_table_search.Is this a c-icap bdb table?\n");
        }
    }
    return key;
}

void  bdb_table_release_result(struct ci_lookup_table *table,void **val)
{
    ci_buffer_free(val);
}
