#include "common.h"
#include <ldap.h>
#include "c-icap.h"
#include "module.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"
#include "util.h"
#include <assert.h>

#define MAX_LDAP_FILTER_SIZE 1024
#define MAX_DATA_SIZE        32768
#define MAX_COLS             1024
#define DATA_START           (MAX_COLS*sizeof(void *))
#define DATA_SIZE            (MAX_DATA_SIZE-DATA_START)

static int init_ldap_pools();
static void release_ldap_pools();

int init_ldap_module(struct ci_server_conf *server_conf);
void release_ldap_module();

CI_DECLARE_MOD_DATA common_module_t module = {
    "ldap_module",
    init_ldap_module,
    NULL,
    release_ldap_module,
    NULL,
};



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

int init_ldap_module(struct ci_server_conf *server_conf)
{
    init_ldap_pools();
    if (ci_lookup_table_type_register(&ldap_table_type) == NULL)
        return 0;
    if (ci_lookup_table_type_register(&ldaps_table_type) == NULL)
        return 0;
    if (ci_lookup_table_type_register(&ldapi_table_type) == NULL)
        return 0;
    return 1;
}

void release_ldap_module()
{
    release_ldap_pools();
    ci_lookup_table_type_unregister(&ldap_table_type);
    ci_lookup_table_type_unregister(&ldaps_table_type);
    ci_lookup_table_type_unregister(&ldapi_table_type);
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
    struct ldap_connection *next;
};

#define LDURISZ 1024
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
    int connections_used;
    int64_t connections_opened;
    int max_connections;
    int ttl;
    const char *scheme;
    ci_thread_mutex_t mutex;
    ci_thread_cond_t pool_cond;
    struct ldap_connection *inactive;
    struct ldap_connection *used;

    struct ldap_connections_pool *next;
};

struct ldap_connections_pool *ldap_pools = NULL;
ci_thread_mutex_t ldap_connections_pool_mtx;

static void ldap_pool_destroy(struct ldap_connections_pool *pool);

static int init_ldap_pools()
{
    ldap_pools = NULL;
    ci_thread_mutex_init(&ldap_connections_pool_mtx);
    return 1;
}

static void release_ldap_pools()
{
    struct ldap_connections_pool *pool;
    while ((pool = ldap_pools) != NULL) {
        ldap_pools = ldap_pools->next;
        ldap_pool_destroy(pool);
    }
    ci_thread_mutex_destroy(&ldap_connections_pool_mtx);
}

/*The folowing two functions are not thread safe! It is only used in ldap_pool_create which locks
  the required mutexes*/
static void add_ldap_pool(struct ldap_connections_pool *pool)
{
    /*NOT thread safe!*/
    struct ldap_connections_pool *p;
    pool->next = NULL;
    if (!ldap_pools) {
        ldap_pools = pool;
        return;
    }
    p = ldap_pools;
    while (p->next != NULL) p = p->next;
    p->next = pool;
}

static struct ldap_connections_pool * search_ldap_pools(char *server, int port, char *user, char *password)
{
    /*NOT thread safe!*/
    struct ldap_connections_pool *p;
    p = ldap_pools;
    while (p) {
        if (strcmp(p->server,server) == 0  &&
                p->port == port &&
                strcmp(p->user, user) == 0 &&
                strcmp(p->password, password) == 0
           )
            return p;

        p = p->next;
    }
    return NULL;
}

static struct ldap_connections_pool *ldap_pool_create(char *server, int port, char *user, char *password, const char *scheme, int max_connections, int ttl)
{
    struct ldap_connections_pool *pool;
    ci_thread_mutex_lock(&ldap_connections_pool_mtx);

    pool = search_ldap_pools(server, port,
                             (user != NULL? user : ""),
                             (password != NULL? password : ""));
    ci_thread_mutex_unlock(&ldap_connections_pool_mtx);
    if (pool)
        return pool;

    pool = malloc(sizeof(struct ldap_connections_pool));
    if (!pool) {
        return NULL;
    }
    strncpy(pool->server, server, CI_MAXHOSTNAMELEN);
    pool->server[CI_MAXHOSTNAMELEN]='\0';
    pool->port = port;
    pool->ldapversion = LDAP_VERSION3;
    pool->scheme = scheme;
    pool->ttl = ttl;
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
    pool->connections_used = 0;
    pool->connections_opened = 0;
    pool->inactive = NULL;
    pool->used = NULL;

    if (pool->port > 0)
        snprintf(pool->ldap_uri, LDURISZ, "%s://%s:%d", pool->scheme, pool->server, pool->port);
    else
        snprintf(pool->ldap_uri, LDURISZ, "%s://%s", pool->scheme, pool->server);
    ci_thread_mutex_init(&pool->mutex);
    ci_thread_cond_init(&pool->pool_cond);

    ci_thread_mutex_lock(&ldap_connections_pool_mtx);
    add_ldap_pool(pool);
    ci_thread_mutex_unlock(&ldap_connections_pool_mtx);

    return pool;
}

static void free_ldap_connection_list(struct ldap_connection *list)
{
    struct ldap_connection *curr;
    while (list) {
        ldap_unbind_ext_s(list->ldap, NULL, NULL);
        curr = list;
        list = list->next;
        free(curr);
    }
}

/*The following function is not thread safe! Should called only when c-icap shutdown*/
static void ldap_pool_destroy(struct ldap_connections_pool *pool)
{
    if (pool->used) {
        ci_debug_printf(1,"WARNING: Still used ldap connections for pool %s\n",
                        pool->ldap_uri);
    }

    free_ldap_connection_list(pool->inactive);
    pool->inactive = NULL;

    ci_thread_mutex_destroy(&pool->mutex);
    ci_thread_cond_destroy(&pool->pool_cond);
    free(pool);
}

static LDAP *ldap_connection_open(struct ldap_connections_pool *pool, int force_new, ldap_connection_pool_error_t *pool_error, int *ld_error)
{
    struct ldap_connection *conn = NULL;
    /*Use this to save expired ldap connection objects and free them just before exist.
     The reason is that the ldap_unbind_ext_s locks/unlocks mutexs. It is better to
     avoid nested mutex locks to avoid possible deadlocks. */
    struct ldap_connection *expired = NULL;;
    struct berval ldap_passwd, *servercred;
    int ret;
    char *ldap_user;
    *pool_error = LDP_ERR_NONE;
    *ld_error = LDAP_SUCCESS;
    time_t current_time;
    time(&current_time);
    if (!force_new) {
        if (ci_thread_mutex_lock(&pool->mutex) != 0) {
            *pool_error = LDP_ERR_LOCKING;
            return NULL;
        }
        do {
            if (pool->inactive) {
                conn = pool->inactive;
                pool->inactive = pool->inactive->next;

                /* TODO: maybe schedule a job (ci_command_schedule)
                   to remove expired LDAP connections.
                   However current implementation looks enough for now.
                 */
                if ((conn->last_use + pool->ttl) < current_time) {
                    /*expired delete it*/
                    conn->next = expired;
                    expired = conn;
                    conn = NULL;
                    pool->connections--;
                    continue; /* retry */
                }
                conn->next = pool->used;
                pool->used = conn;
                pool->connections_used++;
                conn->hits++;
                ci_thread_mutex_unlock(&pool->mutex);
                if (expired) {
                    free_ldap_connection_list(expired);
                    expired = NULL;
                }
                return conn->ldap;
            }

            if (pool->max_connections > 0 && ((pool->connections + pool->connections_ontheway) >= pool->max_connections)) {
                /*wait for an ldap connection to be released. The condwait will unlock
                  pool->mutex */
                if (ci_thread_cond_wait(&(pool->pool_cond), &(pool->mutex)) != 0) {
                    ci_thread_mutex_unlock(&(pool->mutex));
                    *pool_error = LDP_ERR_LOCKING;
                    return NULL;
                }
            }
        } while (pool->max_connections > 0 && (pool->connections + pool->connections_ontheway) >= pool->max_connections);

        pool->connections_ontheway++;
        ci_thread_mutex_unlock(&pool->mutex);
        if (expired) {
            free_ldap_connection_list(expired);
            expired = NULL;
        }
    } else {
        ci_thread_mutex_lock(&pool->mutex);
        pool->connections_ontheway++;
        ci_thread_mutex_unlock(&pool->mutex);
    }

    if (!(conn = malloc(sizeof(struct ldap_connection)))) {
        ci_debug_printf(1, "Error allocating ldap_connection structure\n");
        *pool_error = LDP_ERR_MEM_ALLOC;
        return NULL;
    }
    conn->hits = 1;
    conn->last_use = 0;

    ret = ldap_initialize(&conn->ldap, pool->ldap_uri);
    if (!conn->ldap) {
        ci_debug_printf(1, "Error allocating memory for ldap connection: %s!\n",
                        ldap_err2string(ret));
        free(conn);
        *pool_error = LDP_ERR_LDAP;
        *ld_error = ret;
        return NULL;
    }

    ldap_set_option(conn->ldap, LDAP_OPT_PROTOCOL_VERSION, &(pool->ldapversion));
    if (pool->user[0] != '\0')
        ldap_user = pool->user;
    else
        ldap_user = NULL;

    if (pool->password[0] != '\0') {
        ldap_passwd.bv_val = pool->password;
        ldap_passwd.bv_len = strlen(pool->password);
    } else {
        ldap_passwd.bv_val = NULL;
        ldap_passwd.bv_len = 0;
    }

    ret = ldap_sasl_bind_s( conn->ldap,
                            ldap_user,
                            LDAP_SASL_SIMPLE,
                            &ldap_passwd,
                            NULL,
                            NULL,
                            &servercred );

    if (ret != LDAP_SUCCESS) {
        ci_debug_printf(1, "Error bind to ldap server: %s!\n",ldap_err2string(ret));
        ldap_unbind_ext_s(conn->ldap, NULL, NULL);
        free(conn);
        *pool_error = LDP_ERR_LDAP;
        *ld_error = ret;
        return NULL;
    }
    if (servercred) {
        ber_bvfree(servercred);
    }

    if (ci_thread_mutex_lock(&pool->mutex)!= 0) {
        ci_debug_printf(1, "Error locking mutex while opening ldap connection!\n");
        ldap_unbind_ext_s(conn->ldap, NULL, NULL);
        free(conn);
        *pool_error = LDP_ERR_LOCKING;
        return NULL;
    }
    pool->connections++;
    pool->connections_used++;
    pool->connections_ontheway--;
    pool->connections_opened++;
    conn->next = pool->used;
    pool->used = conn;
    ci_thread_mutex_unlock(&pool->mutex);
    return conn->ldap;
}

static int ldap_connection_release(struct ldap_connections_pool *pool, LDAP *ldap, int close_connection)
{
    struct ldap_connection *cur,*prev;
    if (ci_thread_mutex_lock(&pool->mutex) != 0)
        return 0;

    ci_debug_printf(7, "ldap_connection_release, pool current state: overal connections opened %" PRId64 ", current connections %d, new connections on the way %d, used %d in pool %d\n", pool->connections_opened, pool->connections, pool->connections_ontheway,  pool->connections_used, pool->connections - pool->connections_used);

    for (prev = NULL, cur = pool->used; cur != NULL;
            prev = cur, cur = cur->next) {
        if (cur->ldap == ldap) {
            if (cur == pool->used)
                pool->used = pool->used->next;
            else
                prev->next = cur->next;
            break;
        }
    }
    if (!cur) {
        ci_debug_printf(0, "Not ldap connection in used list! THIS IS  A BUG! please contact authors\n!");
        close_connection = 1;
    } else
        pool->connections_used--;
    if (close_connection) {
        pool->connections--;
        ldap_unbind_ext_s(ldap, NULL, NULL);
        if (cur)
            free(cur);
    } else {
        time(&(cur->last_use));
        cur->next = pool->inactive;
        pool->inactive = cur;
    }
    ci_thread_mutex_unlock(&pool->mutex);

    if (pool->max_connections > 0)
        ci_thread_cond_signal(&pool->pool_cond);

    return 1;
}

/******************************************************/
/* ldap table implementation                          */

struct ldap_table_data {
    struct ldap_connections_pool *pool;
    char *str;
    char *base;
    char *server;
    int port;
    char *user;
    char *password;
    char **attrs;
    char *filter;
    char *name;
    const char *scheme;
    ci_cache_t *cache;
};

static int parse_ldap_str(struct ldap_table_data *fields)
{
    char *s, *e, *p;
    char c;
    int array_size, i;

    /*we are expecting a path in the form //[username:password@]ldapserver[:port][/|?]base?attr1,attr2?filter*/

    if (!fields->str)
        return 0;
    i = 0;
    s = fields->str;
    while (*s == '/') {  /*Ignore "//" at the beginning*/
        s++;
        i++;
    }
    if (i != 2)
        return 0;

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
    while (*s != ':' && *s != '?' && *s != '/'  && *s != '\0') s++;
    if (*s == '\0')
        return 0;
    c = *s;
    *s = '\0';
    ci_str_trim(fields->server);

    if (c == ':') { /*The s points to the port specification*/
        s++;
        p = s;
        while (*s != '?' && *s != '/'  && *s != '\0') s++;
        if (*s == '\0')
            return 0;
        *s = '\0';
        fields->port = strtol(p, NULL, 10);
    }
    s++;
    fields->base = s;    /*The s points to the "base" field now*/
    while (*s != '?' && *s != '\0') s++;
    if (*s == '\0')
        return 0;
    *s = '\0';
    ci_str_trim(fields->base);

    s++;
    e = s;  /*Count the args*/
    array_size = 1;
    while (*e != '?' && *e != '\0') {
        if (*e == ',')
            array_size = array_size+1;
        e++;
    }
    if (*e == '\0')
        return 0;
    array_size = array_size+1;
    fields->attrs = (char **) malloc(array_size*sizeof(char *));
    if (fields->attrs == NULL)
        return 0;
    fields->attrs[0] = s;
    i = 1;
    while (i < array_size-1) {
        while (*s != ',') s++;
        *s = '\0';
        s++;
        fields->attrs[i] = s;  /*Every pointer of the array points to an "arg", the last points NULL*/
        i++;
    }
    while (*s != '?') s++;
    *s = '\0';

    fields->attrs[i] = NULL;
    for (i = 0; fields->attrs[i] != NULL; i++)
        ci_str_trim(fields->attrs[i]);

    s++;
    fields->filter = s;   /*The s points to the "filter" field now*/
    ci_str_trim(fields->filter);
    return 1;
}

static void *ldap_open(struct ci_lookup_table *table, const char *scheme)
{
    int i;
    char *path;
    char tname[1024];
    struct ldap_table_data *ldapdata;
    ci_dyn_array_t *args = NULL;
    ci_array_item_t *arg = NULL;
    char *use_cache = "local";
    int cache_ttl = 60;
    size_t cache_size = 1*1024*1024;
    size_t cache_item_size = 2048;
    int max_connections = 0;
    int idle_ttl = 60;
    long int val;

    path = strdup(table->path);
    if (!path) {
        ci_debug_printf(1, "ldap_table_open: error allocating memory!\n");
        return NULL;
    }

    ldapdata = malloc(sizeof(struct ldap_table_data));
    if (!ldapdata) {
        free(path);
        ci_debug_printf(1, "ldap_table_open: error allocating memory (ldapdata)!\n");
        return NULL;
    }
    ldapdata->str = path;
    ldapdata->pool = NULL;
    ldapdata->base = NULL;
    ldapdata->server = NULL;
    if (strcasecmp(scheme, "ldap") == 0)
        ldapdata->port = 389;
    else if (strcasecmp(scheme, "ldaps") == 0)
        ldapdata->port = 636;
    else
        ldapdata->port = 0;
    ldapdata->user = NULL;
    ldapdata->password = NULL;
    ldapdata->attrs = NULL;
    ldapdata->filter = NULL;
    ldapdata->name = NULL;
    ldapdata->scheme = scheme;

    if (!parse_ldap_str(ldapdata)) {
        free(ldapdata->str);
        free(ldapdata);
        ci_debug_printf(1, "ldap_table_open: parse path string error!\n");
        return NULL;
    }

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
                } else if (strcasecmp(arg->name, "max-connections") == 0) {
                    val = strtol((char *)arg->value, NULL, 10);
                    if (val > 0)
                        max_connections = val;
                    else
                        ci_debug_printf(1, "WARNING: wrong max-connections value: %ld, using default\n", val);
                } else if (strcasecmp(arg->name, "idle-ttl") == 0) {
                    val = strtol((char *)arg->value, NULL, 10);
                    if (val > 0)
                        idle_ttl = val;
                    else
                        ci_debug_printf(1, "WARNING: wrong idle-ttl value: %ld, using default\n", val);
                }
            }
        }
    }

    ldapdata->pool = ldap_pool_create(ldapdata->server, ldapdata->port,
                                      ldapdata->user, ldapdata->password, ldapdata->scheme, max_connections, idle_ttl);
    if (use_cache) {
        snprintf(tname, sizeof(tname), "ldap:%s", ldapdata->name ? ldapdata->name : ldapdata->str);
        ldapdata->cache = ci_cache_build(tname, use_cache,
                                         cache_size, cache_item_size, cache_ttl,
                                         &ci_str_ops);
        if (!ldapdata->cache) {
            ci_debug_printf(1, "ldap_table_open: can not create cache! cache is disabled");
        }
    } else
        ldapdata->cache = NULL;

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
    char buf[2048];

    *vals = NULL;
    failures = 0;
    return_value = NULL;

    if (data->cache && ci_cache_search(data->cache, key, (void **)&vect, NULL, &ci_cache_read_vector_val)) {
        ci_debug_printf(4, "Retrieving from cache....\n");
        if (!vect) /*Negative hit*/
            return NULL;
        *vals = (void **)ci_vector_cast_to_voidvoid(vect);
        return key;
    }

    create_filter(filter, MAX_LDAP_FILTER_SIZE, data->filter,key);

    ld = NULL;
    for (failures = 0; ld == NULL && failures < 5; failures++) {
        ldap_connection_pool_error_t err = LDP_ERR_NONE;
        int ld_err = LDAP_SUCCESS;
        ld = ldap_connection_open(data->pool, failures > 1, &err, &ld_err);

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
            ci_debug_printf(4, "Querying LDAP server result: %d %s\n", err, ldap_err2string(ld_err));
            err = (ld_err != LDAP_SUCCESS || !msg) ? LDP_ERR_LDAP : LDP_ERR_NONE;
        }

        if (err) {
            if (ld)
                ldap_connection_release(data->pool, ld, 1);
            switch(ld_err) {
            case LDAP_SERVER_DOWN:
            case LDAP_TIMEOUT:
                usleep(10000 * failures);
                continue;
            }
            ci_debug_printf(1, "Error contacting LDAP server: %s\n", ldap_err2string(ld_err));
            return NULL;
        }
    }

    if (!ld) {
        ci_debug_printf(1, "Stop trying to connect to %s:%d after %d tries\n", data->server, data->port, failures);
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

            ci_debug_printf(8, "Retrieve attribute:%s. Values: ", attrname);
            if ((attrs = ldap_get_values_len(ld, entry, attrname))) {
                for (i = 0; attrs[i] != NULL ; ++i) {
                    //OpenLdap nowhere documents that the result is NULL terminated.
                    // copy to an intermediate buffer and terminate it before store to vector
                    v_size = sizeof(buf) <= attrs[i]->bv_len + 1 ? sizeof(buf) : attrs[i]->bv_len;
                    memcpy(buf, attrs[i]->bv_val, v_size);
                    buf[v_size] = '\0';
                    (void)ci_str_vector_add(vect, buf);
                    ci_debug_printf(8, "%s,", buf);
                }
                ci_debug_printf(8, "\n");
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

    if (data->cache) {
        v_size =  vect != NULL ? ci_cache_store_vector_size(vect) : 0;
        ci_debug_printf(4, "adding to cache\n");
        if (!ci_cache_update(data->cache, key, vect, v_size, ci_cache_store_vector_val))
            ci_debug_printf(4, "adding to cache failed!\n");
    }

    if (!vect)
        return NULL;

    *vals = (void **)ci_vector_cast_to_voidvoid(vect);
    return return_value;
}

void  ldap_table_release_result(struct ci_lookup_table *table,void **val)
{
    ci_str_vector_t  *v = ci_vector_cast_from_voidvoid((const void **)val);
    ci_str_vector_destroy(v);
}
