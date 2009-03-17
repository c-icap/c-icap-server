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

void *store_val(void *val,int *val_size, ci_mem_allocator_t *allocator)
{
    return val;
}

void *read_val(void *val, int val_size, ci_mem_allocator_t *allocator)
{
    return val;
}

void *dnsbl_table_open(struct ci_lookup_table *table)
{
    struct dnsbl_data *dnsbl_data;
    if (strlen(table->path) >= CI_MAXHOSTNAMELEN ) {
         ci_debug_printf(1, "dnsbl_table_open: too long domain name: %s\n",
                        table->path);
        return NULL;
    }

    dnsbl_data = malloc(sizeof(struct dnsbl_data));
    if (!dnsbl_data) {
        ci_debug_printf(1, "dnsbl_table_open: error allocating memory (dnsbl_data)!\n");
        return NULL;
    }
    strcpy(dnsbl_data->check_domain, table->path);
    dnsbl_data->cache = ci_cache_build(65536, CI_MAXHOSTNAMELEN+1, 0, 60, &ci_str_ops,
				       store_val,
				       read_val);

    table->data = dnsbl_data; 
    return table->data;
}

void  dnsbl_table_close(struct ci_lookup_table *table)
{
     struct dnsbl_data *dnsbl_data = table->data;
     table->data = NULL;
     free(dnsbl_data);
}

void *dnsbl_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    ci_sockaddr_t  addr;
    char dnsname[CI_MAXHOSTNAMELEN];
    char *server;
    void *val;
    struct dnsbl_data *dnsbl_data = table->data;

    if(table->key_ops != &ci_str_ops) {
	ci_debug_printf(1,"Only keys of type string allowed in this type of table:\n");
	return NULL;
    }
    server = (char *)key;

    if (dnsbl_data->cache && ci_cache_search(dnsbl_data->cache, server, &val, table->allocator)) {
	ci_debug_printf(6,"dnsbl_table_search: cache hit for %s value %p\n", server,  val);
	return (val == (void *)0x0)? NULL: key;
    }

    snprintf(dnsname, CI_MAXHOSTNAMELEN, "%s.%s", server, dnsbl_data->check_domain);
    if(! ci_host_to_sockaddr_t(dnsname, &addr, AF_INET)) {
	if (dnsbl_data->cache)
	    ci_cache_update(dnsbl_data->cache, server, (void *)0x0);
	return NULL;
    }
    /*Are we interested for returning any data?
      Well, some of the dns lists uses different ip addresses to categorize
      hosts in various categories. TODO.
     */
    *vals = NULL;
    if (dnsbl_data->cache)
	ci_cache_update(dnsbl_data->cache, server, (void *)0x1);

    return key;
}

void  dnsbl_table_release_result(struct ci_lookup_table *table,void **val)
{
    /*   struct ci_mem_allocator *allocator = table->allocator;
	 allocator->free(allocator, val);
    */
    val = NULL;
}
