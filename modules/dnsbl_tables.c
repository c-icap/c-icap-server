#include "common.h" 
#include "c-icap.h"
#include "module.h"
#include "lookup_table.h"
#include "net_io.h"
#include "cache.h"
#include "debug.h"
#include "common.h"


int init_dnsbl_tables(struct ci_server_conf *server_conf);
void release_dnsbl_tables();

CI_DECLARE_MOD_DATA common_module_t module = {
    "dnsbl_tables",
    init_dnsbl_tables,
    NULL,
    release_dnsbl_tables,
    NULL,
};



void *dnsbl_table_open(struct ci_lookup_table *table); 
void  dnsbl_table_close(struct ci_lookup_table *table);
void *dnsbl_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  dnsbl_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type dnsbl_table_type={
    dnsbl_table_open,
    dnsbl_table_close,
    dnsbl_table_search,
    dnsbl_table_release_result,
    NULL,
    "dnsbl"
};


int init_dnsbl_tables(struct ci_server_conf *server_conf)
{
    return (ci_lookup_table_type_register(&dnsbl_table_type) != NULL);
}

void release_dnsbl_tables()
{
    ci_lookup_table_type_unregister(&dnsbl_table_type);
}

/***********************************************************/
/*  bdb_table_type inmplementation                         */

struct dnsbl_data {
    char check_domain[CI_MAXHOSTNAMELEN+1];
    ci_cache_t *cache;
};

void *dnsbl_table_open(struct ci_lookup_table *table)
{
    struct dnsbl_data *dnsbl_data;
    if (strlen(table->path) >= CI_MAXHOSTNAMELEN ) {
         ci_debug_printf(1, "dnsbl_table_open: too long domain name: %s\n",
                        table->path);
        return NULL;
    }

    if (table->key_ops != &ci_str_ops || table->val_ops!= &ci_str_ops) {
        ci_debug_printf(1, "dnsbl_table_open:  Only searching with strings and returning strings supported\n");
        return NULL;
    }

    dnsbl_data = malloc(sizeof(struct dnsbl_data));
    if (!dnsbl_data) {
        ci_debug_printf(1, "dnsbl_table_open: error allocating memory (dnsbl_data)!\n");
        return NULL;
    }
    strcpy(dnsbl_data->check_domain, table->path);
    dnsbl_data->cache = ci_cache_build(65536, CI_MAXHOSTNAMELEN+1, 0, 60, &ci_str_ops,
                                       &ci_cache_store_vector_val, /*copy_to*/
                                       &ci_cache_read_vector_val /*copy_from*/
        );

    table->data = dnsbl_data; 
    return table->data;
}

void  dnsbl_table_close(struct ci_lookup_table *table)
{
     struct dnsbl_data *dnsbl_data = table->data;
     table->data = NULL;
     if (dnsbl_data->cache)
         ci_cache_destroy(dnsbl_data->cache);
     free(dnsbl_data);
}

static ci_vector_t  *resolv_hostname(char *hostname);
void *dnsbl_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    char dnsname[CI_MAXHOSTNAMELEN + 1];
    char *server;
    ci_str_vector_t  *v;
    struct dnsbl_data *dnsbl_data = table->data;

    if(table->key_ops != &ci_str_ops) {
	ci_debug_printf(1,"Only keys of type string allowed in this type of table:\n");
	return NULL;
    }
    server = (char *)key;

    if (dnsbl_data->cache && ci_cache_search(dnsbl_data->cache, server, (void **)&v, table->allocator)) {
	ci_debug_printf(6,"dnsbl_table_search: cache hit for %s value %p\n", server,  v);
        if (!v) {
            *vals = NULL;
            return NULL;
        }
        *vals = (void **)ci_vector_cast_to_voidvoid(v);
        return key;
    }

    snprintf(dnsname, CI_MAXHOSTNAMELEN, "%s.%s", server, dnsbl_data->check_domain);
    dnsname[CI_MAXHOSTNAMELEN] = '\0';
    v = resolv_hostname(dnsname);
    if (dnsbl_data->cache)
        ci_cache_update(dnsbl_data->cache, server, v);
    
    if (!v)
        return NULL;

    *vals = (void **)ci_vector_cast_to_voidvoid(v);
    return key;
}

void  dnsbl_table_release_result(struct ci_lookup_table *table,void **val)
{
    ci_str_vector_t  *v = ci_vector_cast_from_voidvoid((const void **)val);
    ci_str_vector_destroy(v);
}

/**************************/
/* Utility functions               */

/*Return the list of ip address for a given  hostname*/
static ci_vector_t  *resolv_hostname(char *hostname)
{
    ci_str_vector_t  *vect = NULL;
    int ret;
    struct addrinfo hints, *res, *cur;
    ci_sockaddr_t addr;
    char buf[256];

     memset(&hints, 0, sizeof(hints));
     hints.ai_family = AF_UNSPEC;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_protocol = 0;
     if ((ret = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
	 ci_debug_printf(5, "Error geting addrinfo:%s\n", gai_strerror(ret));
	 return NULL;
     }

     if (res)
         vect = ci_str_vector_create(1024);

     if (vect) {
         for(cur = res; cur != NULL; cur = cur->ai_next){
             memcpy(&(addr.sockaddr), cur->ai_addr, CI_SOCKADDR_SIZE);
             ci_fill_sockaddr(&addr);
             if (ci_sockaddr_t_to_ip(&addr, buf, sizeof(buf)))
                 (void)ci_str_vector_add(vect, buf);
         }
         freeaddrinfo(res);
     }

     return vect;
}
