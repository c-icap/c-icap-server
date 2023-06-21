#include "common.h"
#include <ldap.h>
#include "c-icap.h"
#include "array.h"
#include "commands.h"
#include "module.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"
#include "stats.h"
#include "util.h"
#include <assert.h>

#define MAX_LDAP_FILTER_SIZE 1024
#define MAX_DATA_SIZE        32768
#define MAX_COLS             1024
#define DATA_START           (MAX_COLS*sizeof(void *))
#define DATA_SIZE            (MAX_DATA_SIZE-DATA_START)

static int USE_CI_MEMPOOLS = 1;
static ci_stat_memblock_t *LDAP_STATS = NULL;

static int init_ldap_pools();
static void release_ldap_pools();

static int ldap_connections_pool_configure(const char *directive, const char **argv, void *setdata);
static struct ci_conf_entry ldap_module_conf_table[] = {
    {"connections_pool", NULL, ldap_connections_pool_configure, NULL},
    {"disable_mempools", &USE_CI_MEMPOOLS, ci_cfg_disable, NULL},
    {NULL, NULL, NULL, NULL}
};

static int init_ldap_module(struct ci_server_conf *server_conf);
static void release_ldap_module();

static common_module_t ldap_module = {
    "ldap_module",
    init_ldap_module,
    NULL,
    release_ldap_module,
    ldap_module_conf_table,
};
_CI_DECLARE_COMMON_MODULE(ldap_module);


void *ldap_table_open(struct ci_lookup_table *table);
void  ldap_table_close(struct ci_lookup_table *table);
void *ldap_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  ldap_table_release_result(struct ci_lookup_table *table_data,void **val);
const void *ldap_table_get_row(struct ci_lookup_table *table, const void *key, const char *columns[], void ***vals);

struct ci_lookup_table_type ldap_table_type = {
    ldap_table_open,
    ldap_table_close,
    ldap_table_search,
    ldap_table_release_result,
    NULL,
    "ldap"
};

struct ci_lookup_table_type ldaps_table_type = {
    ldap_table_open,
    ldap_table_close,
    ldap_table_search,
    ldap_table_release_result,
    NULL,
    "ldaps"
};

struct ci_lookup_table_type ldapi_table_type = {
    ldap_table_open,
    ldap_table_close,
    ldap_table_search,
    ldap_table_release_result,
    NULL,
    "ldapi"
};

static void init_openldap_mem();
static void ldap_module_process_init_cmd(const char *name, int type, void *data);
int init_ldap_module(struct ci_server_conf *server_conf)
{
    init_ldap_pools();
    init_openldap_mem();
    if (ci_lookup_table_type_register(&ldap_table_type) == NULL)
        return 0;
    if (ci_lookup_table_type_register(&ldaps_table_type) == NULL)
        return 0;
    if (ci_lookup_table_type_register(&ldapi_table_type) == NULL)
        return 0;
    ci_command_register_action("ldap_module::child_process_init", CI_CMD_CHILD_START, NULL, ldap_module_process_init_cmd);
    return 1;
}

void release_ldap_module()
{
    release_ldap_pools();
    ci_lookup_table_type_unregister(&ldap_table_type);
    ci_lookup_table_type_unregister(&ldaps_table_type);
    ci_lookup_table_type_unregister(&ldapi_table_type);
}

static void ldap_module_process_init_cmd(const char *name, int type, void *data)
{
    LDAP_STATS =  ci_stat_memblock_get();
    _CI_ASSERT(LDAP_STATS);
}

/***********************************************************/
/*  ldap_connections_pool inmplementation                  */

typedef enum {
    LDP_ERR_NONE = 0,
    LDP_ERR_LOCKING,
    LDP_ERR_MEM_ALLOC,
    LDP_ERR_LDAP
} ldap_connection_pool_error_t;

struct ldap_connection {
    LDAP *ldap;
    int hits;
    time_t last_use;
};

#define LDURISZ 256
#define LDUSERSZ 256
#define LDPWDSZ 256
struct ldap_connections_pool {
    char ldap_uri[LDURISZ];
    char server[CI_MAXHOSTNAMELEN+1];
    int port;
    int ldapversion;
    char user[LDUSERSZ];
    char password[LDPWDSZ];
    int connections;
    int connections_ontheway;
    int max_connections;
    int ttl;
    char scheme[16];
    ci_thread_mutex_t mutex;
    ci_thread_cond_t pool_cond;
    ci_list_t *inactive;
    ci_list_t *used;

    // NOTE: The following statistics are updated using the STAT_INT64_*_NL
    // macros and require pthread locking before updated.
    int stat_connections;
    int stat_idleconnections;
    int stat_newconnections;

    struct ldap_connections_pool *next;
};

ci_list_t *ldap_pools = NULL;

static void ldap_pool_destroy(struct ldap_connections_pool *pool);
static void check_ldap_pools_cmd(const char *name, int type, void *data);
static int init_ldap_pools()
{
    ci_command_register_action("ldap_module::pools_check", CI_CMD_ONDEMAND, NULL,
                               check_ldap_pools_cmd);
    /* The following will be executed only on children startup */
    ci_command_schedule("ldap_module::pools_check", NULL, 0);
    return 1;
}

static void release_ldap_pools()
{
    if (!ldap_pools)
        return;
    struct ldap_connections_pool *pool;
    while((ci_list_pop(ldap_pools, &pool))) {
        ldap_pool_destroy(pool);
    }
    ci_list_destroy(ldap_pools);
    ldap_pools = NULL;
}

static void add_ldap_pool(struct ldap_connections_pool *pool)
{
    /*No need for any lock, pools are created on c-icap startup before
      threads are started*/
    if (!ldap_pools)
        ldap_pools = ci_list_create(512, 0);
    ci_list_push_back(ldap_pools, pool);
}

static struct ldap_connections_pool * search_ldap_pools(const char *server, int port, const char *user, const char *password, const char *scheme)
{
    struct ldap_connections_pool *p = NULL;;
    if (ldap_pools) {
        ci_list_iterator_t it;
        for (p = ci_list_iterator_first(ldap_pools, &it); p != NULL; p = ci_list_iterator_next(&it)) {
            if (strcmp(p->server,server) == 0  &&
                p->port == port &&
                strcmp(p->user, user) == 0 &&
                strcmp(p->password, password) == 0 &&
                strcasecmp(p->scheme, scheme) == 0
                )
                break; /* found */
        }
    }
    return p;
}

static struct ldap_connections_pool *ldap_pool_create(char *server, int port, char *user, char *password, const char *scheme, int max_connections, int ttl)
{
    struct ldap_connections_pool *pool;
    pool = search_ldap_pools(server, port,
                             (user != NULL? user : ""),
                             (password != NULL? password : ""), scheme);
    if (pool)
        return pool;

    pool = malloc(sizeof(struct ldap_connections_pool));
    if (!pool) {
        return NULL;
    }
    snprintf(pool->server, sizeof(pool->server), "%s", server);
    pool->port = port;
    pool->ldapversion = LDAP_VERSION3;
    snprintf(pool->scheme, sizeof(pool->scheme), "%s", scheme);
    pool->ttl = ttl > 0 ? ttl : 60;
    pool->next = NULL;

    if (user) {
        strncpy(pool->user, user, LDUSERSZ);
        pool->user[LDUSERSZ - 1] = '\0';
    } else
        pool->user[0] = '\0';

    if (password) {
        strncpy(pool->password, password, LDPWDSZ);
        pool->password[LDPWDSZ - 1] = '\0';
    } else
        pool->password[0] = '\0';

    pool->max_connections = max_connections;
    pool->connections = 0;
    pool->connections_ontheway = 0;
    pool->inactive = ci_list_create(1024, sizeof(struct ldap_connection));
    pool->used = ci_list_create(1024, sizeof(struct ldap_connection));

    if (pool->port > 0)
        snprintf(pool->ldap_uri, LDURISZ, "%.5s://%.*s:%d", pool->scheme, (int)(sizeof(pool->ldap_uri) - 20), pool->server, pool->port);
    else
        snprintf(pool->ldap_uri, LDURISZ, "%.5s://%.*s", pool->scheme, (int)(sizeof(pool->ldap_uri) - 9), pool->server);
    ci_thread_mutex_init(&pool->mutex);
    ci_thread_cond_init(&pool->pool_cond);

    char buf[sizeof(pool->ldap_uri) + 32];
    snprintf(buf, sizeof(buf), "%s_connections", pool->ldap_uri);
    pool->stat_connections = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_server");
    snprintf(buf, sizeof(buf), "%s_idle_connections", pool->ldap_uri);
    pool->stat_idleconnections = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_server");
    snprintf(buf, sizeof(buf), "%s_new_connections", pool->ldap_uri);
    pool->stat_connections = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_server");
    add_ldap_pool(pool);
    return pool;
}

static void ldap_connection_list_close_all(ci_list_t *list)
{
    struct ldap_connection c;
    while (ci_list_pop(list, &c)) {
        _CI_ASSERT(c.ldap);
        ldap_unbind_ext_s(c.ldap, NULL, NULL);
    }
}

static void free_ldap_connection_list(ci_list_t *list)
{
    if (!list)
        return;
    ldap_connection_list_close_all(list);
    ci_list_destroy(list);
}

static void check_ldap_pools_cmd(const char *name, int type, void *data)
{
    if (!ldap_pools)
        return; //nothing to do
    static ci_list_t *ldap_conn_to_free = NULL;
    if (!ldap_conn_to_free)
        ldap_conn_to_free = ci_list_create(1024, sizeof(struct ldap_connection));
    _CI_ASSERT(ldap_conn_to_free);
    /*No need to lock ldap_pools list*/
    time_t current_time;
    time(&current_time);
    ci_list_iterator_t it;
    struct ldap_connections_pool *p = NULL;
    for (p = ci_list_iterator_first(ldap_pools, &it); p != NULL; p = ci_list_iterator_next(&it)) {
        struct ldap_connection conn = {NULL, 0, 0};
        struct ldap_connection *c = NULL;
        int removed = 0;
        ci_thread_mutex_lock(&p->mutex);
        while((c = ci_list_head(p->inactive)) && (c->last_use + p->ttl) < current_time) {
            ci_list_pop(p->inactive, &conn);
            _CI_ASSERT(conn.ldap);
            ci_list_push(ldap_conn_to_free, &conn);
            memset(&conn, 0, sizeof(conn));
            p->connections--;
            removed++;
        }
        STAT_INT64_DEC_NL(LDAP_STATS, p->stat_connections, removed);
        STAT_INT64_DEC_NL(LDAP_STATS, p->stat_idleconnections, removed);
        ci_thread_mutex_unlock(&p->mutex);
        if (removed)
            ci_debug_printf(8, "Periodic check for ldap connections pool removed %d ldap connections after %d secs from pool %s\n", removed, p->ttl, p->ldap_uri);
    }
    ldap_connection_list_close_all(ldap_conn_to_free);
    ci_command_schedule("ldap_module::pools_check", NULL, 1);
}

static void ldap_pool_destroy(struct ldap_connections_pool *pool)
{
    if (pool->used && (ci_list_head(pool->used) != NULL)) {
        ci_debug_printf(1,"WARNING: Still used ldap connections for pool %s\n",
                        pool->ldap_uri);
    }

    free_ldap_connection_list(pool->inactive);
    pool->inactive = NULL;

    ci_thread_mutex_destroy(&pool->mutex);
    ci_thread_cond_destroy(&pool->pool_cond);
    free(pool);
}

static LDAP *ldap_connection_new(struct ldap_connections_pool *pool, ldap_connection_pool_error_t *pool_error, int *ld_error)
{
    LDAP *ldap = NULL;
    int ret;
    ret = ldap_initialize(&ldap, pool->ldap_uri);
    if (!ldap) {
        ci_debug_printf(1, "Error allocating memory for ldap connection: %s!\n",
                        ldap_err2string(ret));
        *pool_error = LDP_ERR_LDAP;
        *ld_error = ret;
        return NULL;
    }

    ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &(pool->ldapversion));
    char *ldap_user = NULL;
    if (pool->user[0] != '\0')
        ldap_user = pool->user;
    else
        ldap_user = NULL;

    struct berval ldap_passwd, *servercred = NULL;
    if (pool->password[0] != '\0') {
        ldap_passwd.bv_val = pool->password;
        ldap_passwd.bv_len = strlen(pool->password);
    } else {
        ldap_passwd.bv_val = NULL;
        ldap_passwd.bv_len = 0;
    }

    ret = ldap_sasl_bind_s( ldap,
                            ldap_user,
                            LDAP_SASL_SIMPLE,
                            &ldap_passwd,
                            NULL,
                            NULL,
                            &servercred );

    if (ret != LDAP_SUCCESS) {
        ci_debug_printf(1, "Error bind to ldap server: %s!\n",ldap_err2string(ret));
        ldap_unbind_ext_s(ldap, NULL, NULL);
        *pool_error = LDP_ERR_LDAP;
        *ld_error = ret;
        ldap = NULL;
    }
    if (servercred) {
        ber_bvfree(servercred);
    }
    return ldap;
}

static LDAP *ldap_connection_get(struct ldap_connections_pool *pool, int force_new, ldap_connection_pool_error_t *pool_error, int *ld_error)
{
    struct ldap_connection conn = {NULL, 0, 0};
    /*Use this to save expired ldap connection objects and free them just before exist.
     The reason is that the ldap_unbind_ext_s locks/unlocks mutexs. It is better to
     avoid nested mutex locks to avoid possible deadlocks. */
    *pool_error = LDP_ERR_NONE;
    *ld_error = LDAP_SUCCESS;
    time_t current_time;
    time(&current_time);

    ci_thread_mutex_lock(&pool->mutex);
    do {
        if (!force_new && ci_list_pop(pool->inactive, &conn)) {
            STAT_INT64_DEC_NL(LDAP_STATS, pool->stat_idleconnections, 1);
            conn.hits++;
            const struct ldap_connection *c = ci_list_push(pool->used, &conn);
            ci_thread_mutex_unlock(&pool->mutex);
            return c->ldap;
        }

        if (pool->max_connections > 0 && ((pool->connections + pool->connections_ontheway) >= pool->max_connections)) {
            /*wait for an ldap connection to be released. The condwait will unlock
              pool->mutex */
            ci_thread_cond_wait(&(pool->pool_cond), &(pool->mutex));
        }
    } while (pool->max_connections > 0 && (pool->connections + pool->connections_ontheway) >= pool->max_connections);
    pool->connections_ontheway++;
    ci_thread_mutex_unlock(&pool->mutex);

    memset(&conn, 0, sizeof(conn));
    conn.hits = 1;

    conn.ldap = ldap_connection_new(pool, pool_error, ld_error);
    if (!conn.ldap) {
        ci_debug_printf(1, "Error bind to ldap server: %s!\n",ldap_err2string(*ld_error));
    }

    ci_thread_mutex_lock(&pool->mutex);
    pool->connections_ontheway--;
    if (conn.ldap) {
        pool->connections++;
        /*const struct ldap_connection *c = */
        ci_list_push(pool->used, &conn);
        STAT_INT64_INC_NL(LDAP_STATS, pool->stat_connections, 1);
        STAT_INT64_INC_NL(LDAP_STATS, pool->stat_newconnections, 1);
    }
    ci_thread_mutex_unlock(&pool->mutex);
    return conn.ldap;
}

static int ldap_connection_cmp(const void *obj, const void *ldap, size_t obj_size)
{
    struct ldap_connection *c = (struct ldap_connection *)obj;
    if (c->ldap == ldap)
        return 0;
    return 1;
}

static int ldap_connection_release(struct ldap_connections_pool *pool, LDAP *ldap, int close_connection)
{
    struct ldap_connection conn = {NULL, 0, 0};
    time_t current_time;
    time(&current_time);
    ci_thread_mutex_lock(&pool->mutex);
    int ret = ci_list_remove3(pool->used, ldap, &conn, sizeof(conn), ldap_connection_cmp);
    if (!ret) {
        ci_debug_printf(0, "Not ldap connection in used list! THIS IS  A BUG! please contact authors\n!");
        close_connection = 1;
    }
    if (close_connection) {
        STAT_INT64_DEC_NL(LDAP_STATS, pool->stat_connections, 1);
        pool->connections--;
        /* Releasing ldap is probably time-cost operation to be done inside a
           pthread-lock block, do it later before exit this function.
        ldap_unbind_ext_s(ldap, NULL, NULL);
        */
    } else {
        conn.last_use = current_time;
        ci_list_push_back(pool->inactive, &conn);
        STAT_INT64_INC_NL(LDAP_STATS, pool->stat_idleconnections, 1);
    }
    ci_thread_mutex_unlock(&pool->mutex);

    if (pool->max_connections > 0)
        ci_thread_cond_signal(&pool->pool_cond);

    if (close_connection)
        ldap_unbind_ext_s(ldap, NULL, NULL);
    return 1;
}

/******************************************************/
/* ldap table implementation                          */

struct ldap_table_data {
    struct ldap_connections_pool *pool;
    char *str;
    char *base;
    char **attrs;
    char *filter;
    char *name;
    const char *scheme;
    ci_cache_t *cache;

    // NOTE: The following statistics are updated using the STAT_INT64_*
    // macros which support atomic operations
    int stat_failures;
    int stat_hit;
    int stat_miss;
    int stat_cached;
    int stat_retries;
};

struct ldap_uri_parse_data {
    char *base;
    char *server;
    int port;
    char *user;
    char *password;
    char *attrs[1024];
    int attrs_num;
    char *filter;
    char *name;
    char *scheme;
};

static int parse_ldap_uri(struct ldap_uri_parse_data *fields, char *str, int only_base_uri)
{
    char *s, *e, *p;
    char c;
    int i;

    if (!str || ! fields)
        return 0;
    memset(fields, 0, sizeof(struct ldap_uri_parse_data));
    s = str;
    if (!(e = strchr(str, ':')))
        return 0;
    *e = '\0';
    size_t len = e - s;
    if (len == 0 ||
        (strncasecmp(str, "ldap:", len) &&
         strncasecmp(str, "ldaps:", len) &&
         strncasecmp(str, "ldapi:", len))) {
        ci_debug_printf(2, "WARNING: ldap scheme is wrong: %s\n", s);
        return 0;
    }
    fields->scheme = s;
    s = e + 1;

    /*we are expectling a path in the form //[username:password@]ldapserver[:port][/|?]base?attr1,attr2?filter*/
    while (*s && *s == '/') s++; /*Ignore "//" after scheme*/

    /*Extract username/password if exists*/
    if ((e = strrchr(s, '@')) != NULL) {
        fields->user = s;
        *e = '\0';
        s = e + 1;
        if ((e = strchr(fields->user, ':')) != NULL) {
            *e = '\0';
            fields->password = e + 1;
            ci_str_trim(fields->password);
        }
        ci_str_trim(fields->user); /* here we have parsed the user*/
    }

    fields->server = s;  /*The s points to the "server" field now*/
    while (*s && *s != ':' && *s != '?' && *s != '/') s++;
    if (*s == '\0') {
        ci_debug_printf(2, "WARNING: ldap uri parse failue expected ?/: but got eos after %s\n", fields->server);
        return 0;
    }
    c = *s;
    *s = '\0';
    ci_str_trim(fields->server);
    if (c == ':') { /*The s points to the port specification*/
        s++;
        p = s;
        while (*s && *s != '?' && *s != '/') s++;
        if (*s == '\0' && !only_base_uri)
            return 0;
        *s = '\0';
        fields->port = strtol(p, NULL, 10);
    } else {
        if (strcasecmp(fields->scheme, "ldap") == 0)
            fields->port = 389;
        else if (strcasecmp(fields->scheme, "ldaps") == 0)
            fields->port = 636;
        // else ldapi does not have a port
    }
    if (only_base_uri)
        return 1; //Finish here.
    s++;
    fields->base = s;    /*The s points to the "base" field now*/
    while (*s && *s != '?') s++;
    if (*s == '\0') {
        ci_debug_printf(2, "WARNING: ldap uri parse failue expected ? but got eos after %s\n", fields->base);
        return 0;
    }
    *s = '\0';
    ci_str_trim(fields->base);

    s++;
    int attrs_max_len = sizeof(fields->attrs) / sizeof(char *);
    for (i = 0, c = '\0'; i < attrs_max_len && *s && c != '?'; i++) {
        fields->attrs[i] = s;
        e = s;
        while (*e && *e != ',' && *e != '?') e++;
        c = *e;
        if (*e) {
            *e = '\0';
            s = e + 1;
        } else
            s = e;
    }
    fields->attrs[i] = NULL;
    for (i = 0; fields->attrs[i] != NULL; i++)
        ci_str_trim(fields->attrs[i]);
    fields->attrs_num = i;
    if (*s) {
        fields->filter = s;   /*The s points to the "filter" field now*/
        ci_str_trim(fields->filter);
    }
    return 1;
}

static void *ldap_open(struct ci_lookup_table *table, const char *scheme)
{
    int i;
    char tname[512];
    struct ldap_table_data *ldapdata;
    ci_dyn_array_t *args = NULL;
    const ci_array_item_t *arg = NULL;
    char *use_cache = "local";
    int cache_ttl = 60;
    size_t cache_size = 1*1024*1024;
    size_t cache_item_size = 2048;
    long int val;

    size_t tmpSize = strlen(scheme) + 1 + strlen(table->path) + 1;
    char *tmp = malloc(tmpSize);
    _CI_ASSERT(tmp);
    snprintf(tmp, tmpSize, "%s:%s", scheme, table->path);
    struct ldap_uri_parse_data uri_data;
    if (!parse_ldap_uri(&uri_data, tmp, 0)) {
        ci_debug_printf(1, "ldap_table_open: parse uri '%s' is failed!\n", tmp);
        free(tmp);
        return 0;
    }
    struct ldap_connections_pool *pool = search_ldap_pools(uri_data.server, uri_data.port, uri_data.user, uri_data.password, uri_data.scheme);
    if (!pool) {
        pool = ldap_pool_create(uri_data.server, uri_data.port, uri_data.user, uri_data.password, scheme, 0, 0);
        ci_debug_printf(2, "Ldap table '%s', create the new ldap connections pool '%s'\n", table->path, pool->ldap_uri);
    } else {
        ci_debug_printf(2, "Ldap table '%s', use existing ldap connections pool '%s'\n", table->path, pool->ldap_uri);
    }
    if (!pool) {
        ci_debug_printf(1, "ldap_table_open: not able to build ldap pool for '%s'!\n", table->path);
        free(tmp);
        return 0;
    }
    ldapdata = malloc(sizeof(struct ldap_table_data));
    _CI_ASSERT(ldapdata);
    ldapdata->str = strdup(table->path);
    ldapdata->pool = pool;
    ldapdata->base = uri_data.base ? strdup(uri_data.base) : NULL;
    ldapdata->attrs = malloc((uri_data.attrs_num + 1) * sizeof(char *));
    for (i = 0; i < uri_data.attrs_num; i++) {
        ldapdata->attrs[i] = strdup(uri_data.attrs[i]);
    }
    ldapdata->attrs[uri_data.attrs_num] = NULL;
    ldapdata->filter = uri_data.filter ? strdup(uri_data.filter) : NULL;
    ldapdata->name = NULL;
    ldapdata->scheme = scheme;
    free(tmp);

    if (table->args) {
        if ((args = ci_parse_key_value_list(table->args, ','))) {
            for (i = 0; (arg = ci_dyn_array_get_item(args, i)) != NULL; ++i) {
                ci_debug_printf(5, "Table argument %s:%s\n", arg->name, (char *)arg->value);
                if (strcasecmp(arg->name, "name") == 0) {
                    ldapdata->name = strdup((char *)arg->value);
                } else if (strcasecmp(arg->name, "cache") == 0) {
                    if (strcasecmp((char *)arg->value, "no") == 0)
                        use_cache = NULL;
                    else
                        use_cache = (char *)arg->value;
                } else if (strcasecmp(arg->name, "cache-ttl") == 0) {
                    val = strtol((char *)arg->value, NULL, 10);
                    if (val > 0)
                        cache_ttl = val;
                    else
                        ci_debug_printf(1, "WARNING: wrong cache-ttl value: %ld, using default\n", val);
                } else if (strcasecmp(arg->name, "cache-size") == 0) {
                    val = ci_atol_ext((char *)arg->value, NULL);
                    if (val > 0)
                        cache_size = (size_t)val;
                    else
                        ci_debug_printf(1, "WARNING: wrong cache-size value: %ld, using default\n", val);
                } else if (strcasecmp(arg->name, "cache-item-size") == 0) {
                    val = ci_atol_ext((char *)arg->value, NULL);
                    if (val > 0)
                        cache_item_size = (size_t)val;
                    else
                        ci_debug_printf(1, "WARNING: wrong cache-item-size value: %ld, using default\n", val);
                }
            }
        }
    }

    snprintf(tname, sizeof(tname), "ldap:%s", ldapdata->name ? ldapdata->name : ldapdata->str);
    if (use_cache) {
        ldapdata->cache = ci_cache_build(tname, use_cache,
                                         cache_size, cache_item_size, cache_ttl,
                                         &ci_str_ops);
        if (!ldapdata->cache) {
            ci_debug_printf(1, "ldap_table_open: can not create cache! cache is disabled");
        }
    } else
        ldapdata->cache = NULL;

    char buf[1024];
    snprintf(buf, sizeof(buf), "%s_errors", tname);
    ldapdata->stat_failures = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_lookup_table");
    snprintf(buf, sizeof(buf), "%s_hits", tname);
    ldapdata->stat_hit = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_lookup_table");
    snprintf(buf, sizeof(buf), "%s_misses", tname);
    ldapdata->stat_miss = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_lookup_table");
    snprintf(buf, sizeof(buf), "%s_retries", tname);
    ldapdata->stat_retries = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_lookup_table");
    snprintf(buf, sizeof(buf), "%s_cached", tname);
    ldapdata->stat_cached = ci_stat_entry_register(buf, CI_STAT_INT64_T, "ldap_lookup_table");

    table->data = ldapdata;

    /*Must released before exit, we have pointes pointing on args array items*/
    if (args)
        ci_dyn_array_destroy(args);
    return table->data;
}

void *ldap_table_open(struct ci_lookup_table *table)
{
    return ldap_open(table, table->type);
}

void  ldap_table_close(struct ci_lookup_table *table)
{
    struct ldap_table_data *ldapdata;
    ldapdata = (struct ldap_table_data *)table->data;
    table->data = NULL;

    //release ldapdata
    if (ldapdata) {
        free(ldapdata->str);
        if (ldapdata->name)
            free(ldapdata->name);
        if (ldapdata->base)
            free(ldapdata->base);
        if (ldapdata->filter)
            free(ldapdata->filter);
        if (ldapdata->attrs) {
            int i;
            for(i = 0; ldapdata->attrs[i] != NULL; i++)
                free(ldapdata->attrs[i]);
            free(ldapdata->attrs);
        }
        if (ldapdata->cache)
            ci_cache_destroy(ldapdata->cache);
        free(ldapdata);
    }
}

int create_filter(char *filter,int size, char *frmt,char *key)
{
    char *s,*o, *k;
    int i;
    s = frmt;
    o = filter;
    i = 0;
    size --;
    while (i < size && *s != '\0') {
        if (*s == '%' && *(s+1) == 's') {
            k = key;
            while (i < size && *k != '\0' ) {
                *o = *k;
                o++;
                k++;
                i++;
            }
            s+=2;
            continue;
        }
        *o = *s;
        o++;
        s++;
        i++;
    }
    filter[i] = '\0';
    ci_debug_printf(5,"Table ldap search filter: \"%s\"\n", filter);
    return 1;
}

void *ldap_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    struct ldap_table_data *data = (struct ldap_table_data *)table->data;
    LDAPMessage *msg, *entry;
    BerElement *aber;
    LDAP *ld;
    struct berval **attrs;
    void *return_value = NULL;
    char *attrname;
    int failures, i;
    ci_str_vector_t  *vect = NULL;
    size_t v_size;
    char filter[MAX_LDAP_FILTER_SIZE];

    _CI_ASSERT(LDAP_STATS);
    *vals = NULL;
    failures = 0;
    return_value = NULL;

    if (data->cache && ci_cache_search(data->cache, key, (void **)&vect, NULL, &ci_cache_read_vector_val)) {
        STAT_INT64_INC(LDAP_STATS, data->stat_cached, 1);
        ci_debug_printf(6, "ldap_table_search: query ldap table '%s' for key '%s' retrieved from cache result:%p\n", table->path, (const char *)key, (void *)vect);
        if (!vect) { /*Negative hit*/
            STAT_INT64_INC(LDAP_STATS, data->stat_miss, 1);
            return NULL;
        }
        *vals = (void **)ci_vector_cast_to_voidvoid(vect);
        STAT_INT64_INC(LDAP_STATS, data->stat_hit, 1);
        return key;
    }

    create_filter(filter, MAX_LDAP_FILTER_SIZE, data->filter, key);

    int fatal_error = 0;
    for (failures = 0, ld = NULL; ld == NULL && !fatal_error && failures < 5; failures++) {
        if (failures > 0) {
            usleep(10000 * failures); // sleep a while before retry
            STAT_INT64_INC(LDAP_STATS, data->stat_retries, 1);
        }
        ldap_connection_pool_error_t err = LDP_ERR_NONE;
        int ld_err = LDAP_SUCCESS;
        ld = ldap_connection_get(data->pool, failures > 1, &err, &ld_err);

        if (ld) {
            ld_err = ldap_search_ext_s(ld,
                                    data->base, /*base*/
                                    LDAP_SCOPE_SUBTREE, /*scope*/
                                    filter, /*filter*/
                                    data->attrs,  /*attrs*/
                                    0,    /*attrsonly*/
                                    NULL, /*serverctrls*/
                                    NULL, /*clientctrls*/
                                    NULL, /*timeout*/
                                    -1,   /*sizelimit*/
                                    &msg /*res*/
                );
            ci_debug_printf(4, "Querying LDAP server %s result: %d %s\n", data->pool->ldap_uri, err, ldap_err2string(ld_err));
            err = (ld_err != LDAP_SUCCESS || !msg) ? LDP_ERR_LDAP : LDP_ERR_NONE;
        }

        if (err != LDP_ERR_NONE) {
            if (ld) {
                ldap_connection_release(data->pool, ld, 1);
                ld = NULL;
            }
            switch(ld_err) {
            case LDAP_SERVER_DOWN:
            case LDAP_TIMEOUT:
                ci_debug_printf(1, "LDAP server '%s', querying retry-able error %s\n", data->pool->ldap_uri, ldap_err2string(ld_err));
                break; /*will retry*/
            default:
                ci_debug_printf(1, "Error contacting LDAP server %s: %s\n", data->pool->ldap_uri, ldap_err2string(ld_err));
                fatal_error = 1;
                break;
            }
        }
    }

    if (!ld) {
        STAT_INT64_INC(LDAP_STATS, data->stat_failures, 1);
        ci_debug_printf(1, "Stop trying to connect to %s:%d after %d tries\n", data->pool->server, data->pool->port, failures);
        return NULL;
    }

    assert(msg);
    entry = ldap_first_entry(ld, msg);
    while (entry != NULL) {
        aber = NULL;
        attrname = ldap_first_attribute(ld, entry, &aber);
        while (attrname != NULL) {
            if (vect == NULL) {
                vect = ci_str_vector_create(MAX_DATA_SIZE);
                if (!vect)
                    return NULL;
            }

            if ((attrs = ldap_get_values_len(ld, entry, attrname))) {
                for (i = 0; attrs[i] != NULL ; ++i) {
                    const char *vadd = ci_str_vector_add2(vect, attrs[i]->bv_val, attrs[i]->bv_len);
                    if (!vadd) {
                        ci_debug_printf(0, "ldap_table_search: ldap table '%s': Error: not enough space, ldap response will be truncated, query: %s\n", table->path, filter);
                    }
                }
                ldap_value_free_len(attrs);
            }
            attrname = ldap_next_attribute(ld, entry, aber);
        }
        if (aber)
            ber_free(aber, 0);

        if (!return_value)
            return_value = key;

        entry = ldap_next_entry(ld, entry);
    }
    ldap_msgfree(msg);
    ldap_connection_release(data->pool, ld, 0);
    ci_debug_printf(6, "ldap_table_search: ldap table '%s' for key '%s' got %d cols\n", table->path, (const char *)key, ci_vector_size(vect));


    if (data->cache) {
        v_size =  vect != NULL ? ci_cache_store_vector_size(vect) : 0;
        ci_debug_printf(4, "ldap_table_search: ldap table '%s' for key '%s', adding to cache %d bytes\n", table->path, (const char *)key, (int)v_size);
        if (!ci_cache_update(data->cache, key, vect, v_size, ci_cache_store_vector_val))
            ci_debug_printf(4, "ldap_table_search adding to cache failed!\n");
    }

    if (!vect) {
        STAT_INT64_INC(LDAP_STATS, data->stat_miss, 1);
        return NULL;
    }

    *vals = (void **)ci_vector_cast_to_voidvoid(vect);
    STAT_INT64_INC(LDAP_STATS, data->stat_hit, 1);
    return return_value;
}

void  ldap_table_release_result(struct ci_lookup_table *table,void **val)
{
    ci_str_vector_t  *v = ci_vector_cast_from_voidvoid((const void **)val);
    ci_str_vector_destroy(v);
}

/****************/
int ldap_connections_pool_configure(const char *directive, const char **argv, void *setdata)
{
    int max_connections = 0;
    int idle_ttl = 60;
    if (!argv[0]) {
        ci_debug_printf(1, "Missing argument in configuration parameter '%s'\n", directive);
        return 0;
    }
    const char *ldapUri= argv[0];
    int i;
    for(i = 1; argv[i] != NULL; i++) {
        if (strncasecmp(argv[i], "max-connections=", 16) == 0) {
            long int val = strtol(argv[i] + 16, NULL, 10);
            if (val > 0)
                max_connections = val;
            else
                ci_debug_printf(1, "WARNING: wrong max-connections value: %ld, using default\n", val);
        } else if (strncasecmp(argv[i], "idle-ttl=", 9) == 0) {
            long int val = strtol(argv[i] + 9, NULL, 10);
            if (val > 0)
                idle_ttl = val;
            else
                ci_debug_printf(1, "WARNING: wrong idle-ttl value: %ld, using default\n", val);
        }
    }

    char *tmp = strdup(ldapUri);
    _CI_ASSERT(tmp);
    struct ldap_uri_parse_data uri_data;
    if (!parse_ldap_uri(&uri_data, tmp, 1)) {
        ci_debug_printf(1, "Configuration parameter, wrong uri: %s", ldapUri);
        free(tmp);
        return 0;
    }
    if (uri_data.port == 0) { /* set default port*/
        if (strcasecmp(uri_data.scheme, "ldap") == 0)
            uri_data.port = 389;
        else if (strcasecmp(uri_data.scheme, "ldaps") == 0)
            uri_data.port = 636;
    }
    struct ldap_connections_pool *pool = search_ldap_pools(uri_data.server, uri_data.port, uri_data.user, uri_data.password, uri_data.scheme);
    if (pool) {
        pool->max_connections = max_connections;
        if (idle_ttl > 0)
            pool->ttl = idle_ttl;
        ci_debug_printf(2, "Configure existing ldap connections pool '%s', max-connections:%d, idle-ttl:%d\n", pool->ldap_uri, max_connections, idle_ttl);
    } else {
        pool = ldap_pool_create(uri_data.server, uri_data.port, uri_data.user, uri_data.password, uri_data.scheme, max_connections, idle_ttl);
        ci_debug_printf(2, "Build new ldap connections pool '%s', max-connections:%d, idle-ttl:%d\n", pool->ldap_uri, max_connections, idle_ttl);
        if (!pool) {
            ci_debug_printf(1, "ldap_connections_pool_configure: not able to build ldap pool for '%s'!\n", ldapUri);
            free(tmp);
            return 0;
        }
    }
    free(tmp);
    return 1;
}


void *ci_ldap_malloc( ber_len_t   size, void *ctx)
{
    return ci_buffer_alloc(size);
}

void *ci_ldap_calloc( ber_len_t n, ber_len_t size, void *ctx )
{
    size_t buffer_size = size * n;
    void *p = ci_buffer_alloc(buffer_size);
    memset(p, 0, buffer_size);
    return p;
}

void *ci_ldap_realloc(void *ptr, ber_len_t size, void *ctx)
{
    return ci_buffer_realloc(ptr, size);
}

void ci_ldap_free(void *ptr, void *ctx)
{
    ci_buffer_free(ptr);
}

BerMemoryFunctions cicap_mem_funcs =
{ ci_ldap_malloc, ci_ldap_calloc, ci_ldap_realloc, ci_ldap_free };

void init_openldap_mem()
{
    if (USE_CI_MEMPOOLS)
        ber_set_option( NULL, LBER_OPT_MEMORY_FNS, &cicap_mem_funcs );
}
