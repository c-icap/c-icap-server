/*
 *  Copyright (C) 2011 Christos Tsantilas
 *  email: christos@chtsanti.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "common.h"
#include "array.h"
#include "cache.h"
#include "ci_threads.h"
#include "debug.h"
#include "md5.h"
#include "mem.h"
#include "module.h"

#include <libmemcached/memcached.h>

/*
  Set it to 1 if you want to use c-icap memory pools and
  custom c-icap memory allocators.
  Currently libmemcached looks that does not handle well
  foreign mem allocators, so for nos it is disabled.
*/
#define USE_CI_BUFFERS 0

#if defined(LIBMEMCACHED_VERSION_HEX)
#if LIBMEMCACHED_VERSION_HEX > 0x01000000
#include <libmemcached/util.h>
#else
#include <libmemcached/util/pool.h>
#endif /* LIBMEMCACHED_VERSION_HEX > 0x01000000*/
#else
/* And older version of libmemcached*/
#include <libmemcached/memcached_pool.h>
#endif

#include <crypt.h>

int USE_MD5_SUM_KEYS = 1;

int mc_cfg_servers_set(const char *directive, const char **argv, void *setdata);
/*Configuration Table .....*/
static struct ci_conf_entry mc_conf_variables[] = {
    {"servers", NULL, mc_cfg_servers_set, NULL},
    {"use_md5_keys", &USE_MD5_SUM_KEYS, ci_cfg_onoff, NULL},
    {NULL, NULL, NULL, NULL}
};

static int mc_module_init(struct ci_server_conf *server_conf);
static int mc_module_post_init(struct ci_server_conf *server_conf);
static void mc_module_release();
CI_DECLARE_MOD_DATA common_module_t module = {
    "memcached",
    mc_module_init,
    mc_module_post_init,
    mc_module_release,
    mc_conf_variables,
};

#define MC_DOMAINLEN 32
#define MC_MAXKEYLEN 250
#define HOSTNAME_LEN 256
typedef struct mc_server {
    char hostname[HOSTNAME_LEN];
    int port;
} mc_server_t;

/*Vector of mc_server_t elements*/
static ci_list_t *servers_list = NULL;
/*A general mutex used in various configuration steps*/
static ci_thread_mutex_t mc_mtx;

struct mc_cache_data {
    char domain[MC_DOMAINLEN + 1];
};
/*The list of mc caches. Objects of type mc_cache_data.*/
static ci_list_t *mc_caches_list = NULL;

static struct ci_cache_type mc_cache;

memcached_st *MC = NULL;
memcached_pool_st *MC_POOL = NULL;

#if USE_CI_BUFFERS
#if defined(LIBMEMCACHED_VERSION_HEX)
void *mc_mem_malloc(const memcached_st *ptr, const size_t size, void *context);
void mc_mem_free(const memcached_st *ptr, void *mem, void *context);
void *mc_mem_realloc(const memcached_st *ptr, void *mem, const size_t size, void *context);
void *mc_mem_calloc(const memcached_st *ptr, size_t nelem, const size_t elsize, void *context);
#else
void *mc_mem_malloc(memcached_st *ptr, const size_t size);
void mc_mem_free(memcached_st *ptr, void *mem);
void *mc_mem_realloc(memcached_st *ptr, void *mem, const size_t size);
void *mc_mem_calloc(memcached_st *ptr, size_t nelem, const size_t elsize);
#endif
#endif
static int computekey(char *mckey, const char *key, const char *search_domain);

int mc_module_init(struct ci_server_conf *server_conf)
{
    if (ci_thread_mutex_init(&mc_mtx) != 0) {
        ci_debug_printf(1, "Can not intialize mutex!\n");
        return 0;
    }

    /*mc_caches_list should store pointers to memcached caches data*/
    if ((mc_caches_list = ci_list_create(1024, 0)) == NULL) {
        ci_debug_printf(1, "Can not allocate memory for list storing mc domains!\n");
        return 0;
    }

    ci_cache_type_register(&mc_cache);
    ci_debug_printf(3, "Memcached cache sucessfully initialized!\n");
    return 1;
}

int mc_module_post_init(struct ci_server_conf *server_conf)
{
#if USE_CI_BUFFERS
    memcached_return rc;
#endif
    const mc_server_t *srv;
    const char *default_servers[] = {
        "127.0.0.1",
        NULL
    };

    if (servers_list == NULL) {
        mc_cfg_servers_set("server", default_servers, NULL);
        if (servers_list == NULL)
            return 0;
    }

#if USE_CI_BUFFERS
    MC = (memcached_st *)mc_mem_calloc(NULL, 1, sizeof(memcached_st)
#if defined(LIBMEMCACHED_VERSION_HEX)
                                       ,(void *)0x1
#endif
                                      );
#else
    MC = calloc(1, sizeof(memcached_st));
#endif

    MC = memcached_create(MC);
    if (MC == NULL) {
        ci_debug_printf(1,  "Failed to create memcached instance\n");
        return 0;
    }
    ci_debug_printf(1,  "memcached instance created\n");

#if USE_CI_BUFFERS
    rc = memcached_set_memory_allocators(MC,
                                         mc_mem_malloc,
                                         mc_mem_free,
                                         mc_mem_realloc,
                                         mc_mem_calloc
#if defined(LIBMEMCACHED_VERSION_HEX)
                                         , (void *)0x1
#endif
                                        );

    if (rc != MEMCACHED_SUCCESS) {
        ci_debug_printf(1, "Failed to set ci-icap membuf memory allocators\n");
        memcached_free(MC);
        MC = NULL;
        return 0;
    }
#endif

    memcached_behavior_set(MC, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);

    for (srv = (const mc_server_t *)ci_list_first(servers_list); srv != NULL ; srv = (const mc_server_t *)ci_list_next(servers_list)) {
        if (srv->hostname[0] == '/') {
            if (memcached_server_add_unix_socket(MC, srv->hostname) != MEMCACHED_SUCCESS) {
                ci_debug_printf(1, "Failed to add socket path to the server pool\n");
                memcached_free(MC);
                MC = NULL;
                return 0;
            }
        } else if (memcached_server_add(MC, srv->hostname, srv->port) !=
                   MEMCACHED_SUCCESS) {
            ci_debug_printf(1, "Failed to add localhost to the server pool\n");
            memcached_free(MC);
            MC = NULL;
            return 0;
        }
    }

    MC_POOL = memcached_pool_create(MC, 5, 500);
    if (MC_POOL == NULL) {
        ci_debug_printf(1, "Failed to create connection pool\n");
        memcached_free(MC);
        MC = NULL;
        return 0;
    }
    return 1;
}

void mc_module_release()
{
    memcached_pool_destroy(MC_POOL);
    memcached_free(MC);
    ci_list_destroy(servers_list);
    ci_list_destroy(mc_caches_list);
    servers_list = NULL;
}


/*******************************************/
/* memcached cache implementation          */
static int mc_cache_init(struct ci_cache *cache, const char *name);
static const void *mc_cache_search(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));
static int mc_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size));
static void mc_cache_destroy(struct ci_cache *cache);

static struct ci_cache_type mc_cache = {
    mc_cache_init,
    mc_cache_search,
    mc_cache_update,
    mc_cache_destroy,
    "memcached"
};

int mc_cache_cmp(const void *obj, const void *user_data, size_t user_data_size)
{
    struct mc_cache_data *mcObj = (struct mc_cache_data *)obj;
    const char *domain = (const char *)user_data;
    return strcmp(mcObj->domain, domain);
}

int mc_cache_init(struct ci_cache *cache, const char *domain)
{
    int i;
    char useDomain[MC_DOMAINLEN + 1];
    strncpy(useDomain, domain, MC_DOMAINLEN);
    useDomain[MC_DOMAINLEN] = '\0';
    i = 0;
    ci_thread_mutex_lock(&mc_mtx);
    while (i < 1000 && ci_list_search2(mc_caches_list, useDomain, mc_cache_cmp)) {
        snprintf(useDomain, MC_DOMAINLEN, "%.*s~%d",
                 MC_DOMAINLEN - 2 - (i < 10 ? 1 : (i < 100 ? 2 : 3)),
                 domain,
                 i);
        i++;
    }
    ci_thread_mutex_unlock(&mc_mtx);

    if (i > 999) /*????*/
        return 0;

    struct mc_cache_data *mc_data = malloc(sizeof(struct mc_cache_data));
    strncpy(mc_data->domain, useDomain, MC_DOMAINLEN);
    mc_data->domain[MC_DOMAINLEN] = '\0';
    cache->cache_data = mc_data;
    ci_thread_mutex_lock(&mc_mtx);
    ci_list_push_back(mc_caches_list, mc_data);
    ci_thread_mutex_unlock(&mc_mtx);
    ci_debug_printf(3, "memcached cache for domain: '%s' created\n", useDomain);
    return 1;
}

void mc_cache_destroy(struct ci_cache *cache)
{
    ci_thread_mutex_lock(&mc_mtx);
    ci_list_remove(mc_caches_list, cache->cache_data);
    ci_thread_mutex_unlock(&mc_mtx);
    free(cache->cache_data);
}

const void *mc_cache_search(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data))
{
    memcached_return rc;
    memcached_st *mlocal;
    uint32_t flags;
    char mckey[MC_MAXKEYLEN+1];
    int mckeylen = 0;
    void *value;
    size_t value_len;
    int found = 0;
    struct mc_cache_data *mc_data = (struct mc_cache_data *)cache->cache_data;

    mckeylen = computekey(mckey, key, mc_data->domain);
    if (mckeylen == 0)
        return NULL;

    mlocal = memcached_pool_pop(MC_POOL, true, &rc);
    if (!mlocal) {
        ci_debug_printf(1, "Error getting memcached_st object from pool: %s\n", memcached_strerror(MC, rc));
        return NULL;
    }

    value = memcached_get(mlocal, mckey, mckeylen, &value_len, &flags, &rc);

    if ( rc != MEMCACHED_SUCCESS) {
        ci_debug_printf(5, "Failed to retrieve %s object from cache: %s\n",
                        mckey,
                        memcached_strerror(mlocal, rc));
    } else {
        ci_debug_printf(5, "The %s object retrieved from cache has size %d\n",  mckey, (int)value_len);
        found = 1;
    }

    if ((rc = memcached_pool_push(MC_POOL, mlocal)) != MEMCACHED_SUCCESS) {
        ci_debug_printf(1, "Failed to release memcached_st object (%s)!\n", memcached_strerror(MC, rc));
    }

    if (!found)
        return NULL;

    if (dup_from_cache && value) {
        *val = dup_from_cache(value, value_len, data);
        ci_buffer_free(value);
        value = NULL;
    } else {
#if USE_CI_BUFFERS
        *val = value;
#else
        if (value_len) {
            *val = ci_buffer_alloc(value_len);
            if (!*val) {
                free(value);
                return NULL;
            }
            memcpy(*val, value, value_len);
            free(value);
        } else
            *val = NULL;
#endif
    }
    return key;
}

int mc_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size))
{
    void *value = NULL;
    memcached_return rc;
    char mckey[MC_MAXKEYLEN+1];
    int mckeylen = 0;
    struct mc_cache_data *mc_data = (struct mc_cache_data *)cache->cache_data;
    memcached_st *mlocal;
    mckeylen = computekey(mckey, key, mc_data->domain);
    if (mckeylen == 0)
        return 0;

    if (copy_to_cache && val_size) {
        if ((value = ci_buffer_alloc(val_size)) == NULL)
            return 0; /*debug message?*/

        if (!copy_to_cache(value, val, val_size))
            return 0;  /*debug message?*/
    }

    mlocal = memcached_pool_pop(MC_POOL, true, &rc);
    if (!mlocal) {
        ci_debug_printf(1, "Error getting memcached_st object from pool: %s\n",
                        memcached_strerror(MC, rc));
        return 0;
    }

    rc = memcached_set(mlocal, mckey, mckeylen, value != NULL ? (const char *)value : (const char *)val, val_size, cache->ttl, (uint32_t)0);

    if (value)
        ci_buffer_free(value);

    if (rc != MEMCACHED_SUCCESS)
        ci_debug_printf(5, "failed to set key: %s in memcached: %s\n",
                        mckey,
                        memcached_strerror(mlocal, rc));

    if (memcached_pool_push(MC_POOL, mlocal) != MEMCACHED_SUCCESS) {
        ci_debug_printf(1, "Failed to release memcached_st object:%s\n",
                        memcached_strerror(MC, rc));
    }

    ci_debug_printf(5, "mc_cache_update: successfully update key '%s'\n", mckey);
    return 1;
}

int mc_cache_delete(const char *key, const char *search_domain)
{
    memcached_return rc;
    memcached_st *mlocal = memcached_pool_pop(MC_POOL, true, &rc);

    if (!mlocal) {
        ci_debug_printf(1, "Error getting memcached_st object from pool: %s\n",
                        memcached_strerror(MC, rc));
        return 0;
    }

    char mckey[MC_MAXKEYLEN+1];
    int mckeylen = 0;
    mckeylen = computekey(mckey,key,search_domain);
    if (mckeylen == 0)
        return 0;

    rc = memcached_delete(mlocal, mckey, mckeylen, (time_t)0);
    if (rc != MEMCACHED_SUCCESS)
        ci_debug_printf(5, "failed to set key: %s in memcached: %s\n",
                        mckey,
                        memcached_strerror(mlocal, rc));

    return 1;
}

int mc_cfg_servers_set(const char *directive, const char **argv, void *setdata)
{
    int argc;
    char *s;
    mc_server_t srv;

    if (!servers_list) {
        servers_list = ci_list_create(4096, sizeof(mc_server_t));
        if (!servers_list) {
            ci_debug_printf(1, "Error allocating memory for mc_servers list!\n");
            return 0;
        }
    }

    for (argc = 0; argv[argc] != NULL; argc++) {

        strncpy(srv.hostname, argv[argc], HOSTNAME_LEN);
        srv.hostname[HOSTNAME_LEN - 1] = '\0';
        if (srv.hostname[0] != '/' && (s = strchr(srv.hostname, ':')) != NULL) {
            *s = '\0';
            s++;
            srv.port = atoi(s);
            if (!srv.port)
                srv.port = 11211;
        } else
            srv.port = 11211;
        ci_debug_printf(2, "Setup memcached server %s:%d\n", srv.hostname, srv.port);
    }
    ci_list_push_back(servers_list, &srv);

    return argc;
}

#if USE_CI_BUFFERS
/*Memory managment functions*/
#if defined(LIBMEMCACHED_VERSION_HEX)
void *mc_mem_malloc(const memcached_st *ptr, const size_t size, void *context)
#else
void *mc_mem_malloc(memcached_st *ptr, const size_t size)
#endif
{

    void *p = ci_buffer_alloc(size);
    ci_debug_printf(5, "mc_mem_malloc: %p of size %u\n", p, (unsigned int)size);
    return p;
}

#if defined(LIBMEMCACHED_VERSION_HEX)
void mc_mem_free(const memcached_st *ptr, void *mem, void *context)
#else
void mc_mem_free(memcached_st *ptr, void *mem)
#endif
{
#if defined(LIBMEMCACHED_VERSION_HEX)
    ci_debug_printf(5, "mc_mem_free: %p/%p\n", mem, context);
#else
    ci_debug_printf(5, "mc_mem_free: %p\n", mem);
#endif
    if (mem)
        ci_buffer_free(mem);
}

#if defined(LIBMEMCACHED_VERSION_HEX)
void *mc_mem_realloc(const memcached_st *ptr, void *mem, const size_t size, void *context)
#else
void *mc_mem_realloc(memcached_st *ptr, void *mem, const size_t size)
#endif
{
    void *p = ci_buffer_realloc(mem, size);
    ci_debug_printf(5, "mc_mem_realloc: %p of size %u\n", p, (unsigned int)size);
    return p;
}

#if defined(LIBMEMCACHED_VERSION_HEX)
void *mc_mem_calloc(const memcached_st *ptr, size_t nelem, const size_t elsize, void *context)
#else
void *mc_mem_calloc(memcached_st *ptr, size_t nelem, const size_t elsize)
#endif
{
    void *p;
    p = ci_buffer_alloc(nelem*elsize);
    if (!p)
        return NULL;
    memset(p, 0, nelem*elsize);
    ci_debug_printf(5, "mc_mem_calloc: %p of size %u\n", p, (unsigned int)(nelem*elsize));
    return p;
}
#endif

int computekey(char *mckey, const char *key, const char *search_domain)
{
    ci_MD5_CTX md5;
    unsigned char digest[16];
    int mckeylen;
    /*we need to use keys in the form "search_domain:key"
      We can not use keys bigger than MC_MAXKEYLEN
     */
    if (strlen(key)+strlen(search_domain)+1 < MC_MAXKEYLEN) {
        mckeylen = sprintf(mckey, "v%s:%s", search_domain, key);
    } else if (USE_MD5_SUM_KEYS) {
        ci_MD5Init(&md5);
        ci_MD5Update(&md5, (const unsigned char *)key, strlen(key));
        ci_MD5Final(digest, &md5);

        mckeylen = sprintf(mckey, "v%s:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                           search_domain,
                           digest[0], digest[1], digest[2], digest[3],
                           digest[4], digest[5], digest[6], digest[7],
                           digest[8], digest[9], digest[10], digest[11],
                           digest[12], digest[13], digest[14], digest[15]);
    } else {
        mckeylen = 0;
    }

    return mckeylen;
}
