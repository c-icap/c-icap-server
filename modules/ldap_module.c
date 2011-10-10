#include "common.h"
#include <ldap.h>
#include "c-icap.h"
#include "module.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"

#define MAX_LDAP_FILTER_SIZE 1024
#define MAX_DATA_SIZE        32768
#define MAX_COLS             1024
#define DATA_START           (MAX_COLS*sizeof(void *))
#define DATA_SIZE            (MAX_DATA_SIZE-DATA_START)

int init_ldap_pools();
void release_ldap_pools();

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

struct ci_lookup_table_type ldap_table_type={
    ldap_table_open,
    ldap_table_close,
    ldap_table_search,
    ldap_table_release_result,
    "ldap"
};


int init_ldap_module(struct ci_server_conf *server_conf)
{
    init_ldap_pools();
    return (ci_lookup_table_type_register(&ldap_table_type) != NULL);
}

void release_ldap_module()
{
    release_ldap_pools();
    ci_lookup_table_type_unregister(&ldap_table_type);
}

/***********************************************************/
/*  ldap_connections_pool inmplementation                  */

struct ldap_connection {
    LDAP *ldap;
    int hits;
    struct ldap_connection *next;
};

struct ldap_connections_pool {
    char ldap_uri[1024];
    char server[CI_MAXHOSTNAMELEN+1];
    int port;
    int ldapversion;
    char user[256];
    char password[256];
    int connections;
#ifdef LDAP_MAX_CONNECTIONS
    int max_connections;
#endif
    ci_thread_mutex_t mutex;
#ifdef LDAP_MAX_CONNECTIONS
    ci_thread_cond_t pool_cond;
#endif
    struct ldap_connection *inactive;
    struct ldap_connection *used;

    struct ldap_connections_pool *next;
};

struct ldap_connections_pool *ldap_pools = NULL;
ci_thread_mutex_t ldap_connections_pool_mtx;

void ldap_pool_destroy(struct ldap_connections_pool *pool);

int init_ldap_pools()
{
    ldap_pools = NULL;
    ci_thread_mutex_init(&ldap_connections_pool_mtx);
    return 1;
}

void release_ldap_pools()
{
    struct ldap_connections_pool *pool;
    while((pool=ldap_pools) != NULL) {
	ldap_pools = ldap_pools->next;
	ldap_pool_destroy(pool);
    }
    ci_thread_mutex_destroy(&ldap_connections_pool_mtx);
}

/*The folowing two functions are not thread safe! It is only used in ldap_pool_create which locks
  the required mutexes*/
void add_ldap_pool(struct ldap_connections_pool *pool)
{/*NOT thread safe!*/
    struct ldap_connections_pool *p;
    pool->next = NULL;
    if (!ldap_pools){
	ldap_pools = pool;
	return;
    }
    p = ldap_pools;
    while(p->next != NULL) p = p->next;
    p->next = pool;
}

struct ldap_connections_pool * search_ldap_pools(char *server, int port, char *user, char *password)
{/*NOT thread safe!*/
    struct ldap_connections_pool *p;
    p = ldap_pools;
    while(p) {
	if(strcmp(p->server,server) == 0  &&
	   p->port == port &&
	   strcmp(p->user, user) == 0 &&
	   strcmp(p->password, password) == 0
	    )
	    return p;

	p = p->next;
    }
    return NULL;
}

struct ldap_connections_pool *ldap_pool_create(char *server, int port, char *user, char *password)
{
    struct ldap_connections_pool *pool;
    ci_thread_mutex_lock(&ldap_connections_pool_mtx);
    
    pool = search_ldap_pools(server, port, 
			     (user != NULL? user : ""), 
			     (password != NULL? password : ""));
    if(pool) {
	ci_thread_mutex_unlock(&ldap_connections_pool_mtx);
	return pool;
    }

    pool = malloc(sizeof(struct ldap_connections_pool)); 
    if(!pool) {
	ci_thread_mutex_unlock(&ldap_connections_pool_mtx);
	return NULL;
    }
   strncpy(pool->server, server, CI_MAXHOSTNAMELEN);
   pool->server[CI_MAXHOSTNAMELEN]='\0';
   pool->port = port;
   pool->ldapversion = LDAP_VERSION3;
   pool->next = NULL;

   if(user) {
       strncpy(pool->user,user,256);
       pool->user[255] = '\0';
   }
   else
       pool->user[0] = '\0';

   if(password) {
       strncpy(pool->password,password,256);
       pool->password[255] = '\0';
   }
   else
       pool->password[0] = '\0';

   pool->connections = 0;
   pool->inactive = NULL;
   pool->used = NULL;

   snprintf(pool->ldap_uri,1024,"%s://%s:%d","ldap",pool->server,pool->port);
   pool->ldap_uri[1023] = '\0';
   ci_thread_mutex_init(&pool->mutex);
#ifdef LDAP_MAX_CONNECTIONS
   pool->max_connections = 0;
   ci_thread_cond_init(&pool->pool_cond); 
#endif
   add_ldap_pool(pool);
   ci_thread_mutex_unlock(&ldap_connections_pool_mtx);
   return pool;
}

/*The following function is not thread safe! Should called only when c-icap shutdown*/
void ldap_pool_destroy(struct ldap_connections_pool *pool)
{
    struct ldap_connection *conn,*prev;
    if(pool->used) {
	ci_debug_printf(1,"Not released ldap connections for pool %s.This is BUG!\n",
			pool->ldap_uri);
    }
    conn = pool->inactive;

    while(conn) {
	ldap_unbind_ext_s(conn->ldap, NULL, NULL);
	prev = conn;
	conn = conn->next;
	free(prev);
    }
    pool->inactive = NULL;

    ci_thread_mutex_destroy(&pool->mutex);
#ifdef LDAP_MAX_CONNECTIONS
    ci_thread_cond_destroy(&pool->pool_cond); 
#endif
    free(pool);
}

LDAP *ldap_connection_open(struct ldap_connections_pool *pool)
{
  struct ldap_connection *conn;
  struct berval ldap_passwd, *servercred;
  int ret;
  char *ldap_user;
  if (ci_thread_mutex_lock(&pool->mutex)!=0)
      return NULL;
#ifdef LDAP_MAX_CONNECTIONS
  do {
#endif
      if (pool->inactive) {
	  conn = pool->inactive;
	  pool->inactive = pool->inactive->next;
	  
	  conn->next = pool->used;
	  pool->used = conn;
      conn->hits++;
      ci_thread_mutex_unlock(&pool->mutex);
      return conn->ldap;
      }
#ifdef LDAP_MAX_CONNECTIONS
      if (pool->connections >= pool->max_connections) {
	  /*wait for an ldap connection to be released. The condwait will unlock 
	    pool->mutex */
	  if (ci_thread_cond_wait(&(pool->pool_cond), &(pool->mutex)) != 0) {
	      ci_thread_mutex_unlock(&(pool->mutex));
	      return NULL;
	  }
      }
  } while(pool->connections >= pool->max_connections);
#else
  ci_thread_mutex_unlock(&pool->mutex);
#endif


  conn=malloc(sizeof(struct ldap_connection));
  if (!conn) {
#ifdef LDAP_MAX_CONNECTIONS
     ci_thread_mutex_unlock(&pool->mutex);
#endif
     return NULL;
  }
  conn->hits = 1;

  ret = ldap_initialize(&conn->ldap, pool->ldap_uri);
  if (!conn->ldap){
#ifdef LDAP_MAX_CONNECTIONS
      ci_thread_mutex_unlock(&pool->mutex);
#endif
     ci_debug_printf(1, "Error allocating memory for ldap connection: %s!\n",
		     ldap_err2string(ret));
     free(conn);
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
  }
  else {
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

  if (ret != LDAP_SUCCESS){
     ci_debug_printf(1, "Error bind to ldap server: %s!\n",ldap_err2string(ret));
#ifdef LDAP_MAX_CONNECTIONS
     ci_thread_mutex_unlock(&pool->mutex);
#endif
     ldap_unbind_ext_s(conn->ldap, NULL, NULL);
     free(conn);
     return NULL;
  }
  if(servercred) {
      ber_bvfree(servercred);
  }

#ifdef LDAP_MAX_CONNECTIONS
  /*we are already locked*/
#else
  if (ci_thread_mutex_lock(&pool->mutex)!= 0) {
     ci_debug_printf(1, "Error locking mutex while opening ldap connection!\n");
     ldap_unbind_ext_s(conn->ldap, NULL, NULL); 
     free(conn);
     return NULL;
  }
#endif
  pool->connections++;
  conn->next = pool->used;
  pool->used = conn;
  ci_thread_mutex_unlock(&pool->mutex);
  return conn->ldap;
}

int ldap_connection_release(struct ldap_connections_pool *pool, LDAP *ldap, int close_connection)
{
   struct ldap_connection *cur,*prev;
   if (ci_thread_mutex_lock(&pool->mutex)!=0)
      return 0;
 
   for (prev = NULL, cur = pool->used; cur != NULL; 
	                              prev = cur, cur = cur->next) {
       if (cur->ldap == ldap) {
          if(cur == pool->used)
	      pool->used = pool->used->next;
	  else 
	      prev->next = cur->next;
	  break;
       }
   }
   if (!cur) {
       ci_debug_printf(0, "Not ldap connection in used list! THIS IS  A BUG! please contact authors\n!");
      close_connection = 1;
   }
   if (close_connection) {
       pool->connections--;
       ldap_unbind_ext_s(ldap, NULL, NULL);
       if (cur)
          free(cur);
   }
   else {
      cur->next = pool->inactive;
      pool->inactive = cur; 
   }
   ci_thread_mutex_unlock(&pool->mutex);
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
    ci_cache_t *cache;
};

void str_trim(char *str)
{
    char *s, *e;

    if (!str)
        return;

    s = str;
    e = NULL;
    while (*s == ' ' && s != '\0'){
        e = s;
        while (*e != '\0'){
            *e = *(e+1);
            e++;
        }
    }

    /*if (e) e--;  else */
    e = str+strlen(str);
    while(*(--e) == ' ' && e >= str) *e = '\0';
}

int parse_ldap_str(struct ldap_table_data *fields)
{
    char *s, *e;
    int array_size, i;

    /*we are expecting a path in the form //[username:password@]ldapserver?base?attr1,attr2?filter*/

    if(!fields->str)
	return 0;
    i = 0;
    s = fields->str;
    while(*s == '/'){    /*Ignore "//" at the beginning*/
	s++;			
	i++;
    }
    if (i != 2) 
	return 0;

    /*Extract username/password if exists*/
    if ((e=strrchr(s, '@')) != NULL) {
        fields->user = s;
	*e = '\0';
	s = e + 1;
	if ((e=strchr(fields->user, ':')) != NULL) {
	  *e = '\0';
	  fields->password = e + 1;
          str_trim(fields->password);
	}
        str_trim(fields->user); /* here we have parsed the user*/
    }
    
    fields->server = s;	 /*The s points to the "server" field now*/
    while(*s != '?' && *s != '/'  && *s != '\0') s++;
    if (*s == '\0') 
	return 0;
    *s = '\0';
    str_trim(fields->server);

    s++;
    fields->base = s;	 /*The s points to the "base" field now*/
    while(*s != '?' && *s != '\0') s++;
    if (*s == '\0') 
	return 0;
    *s = '\0';
    str_trim(fields->base);

    s++;
    e = s;	/*Count the args*/
    array_size = 1;	
    while (*e != '?' && *e != '\0'){
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
    while (i < array_size-1){										
	while (*s != ',') s++;	
	*s = '\0';
	s++;
	fields->attrs[i] = s;  /*Every pointer of the array points to an "arg", the last points NULL*/
	i++;
    }
    while (*s != '?') s++;											
    *s = '\0';

    fields->attrs[i] = NULL;
    for(i=0; fields->attrs[i] != NULL; i++)
        str_trim(fields->attrs[i]);

    s++;
    fields->filter = s;   /*The s points to the "filter" field now*/
    str_trim(fields->filter);
    return 1;
}

struct cache_val {
    int keys;
    int size;
    void *data;
};

void *store_val(void *val,int *val_size, ci_mem_allocator_t *allocator)
{
    int i, indx_size;
    void *data, *data_start;
    void **val_data, **data_indx;
    struct cache_val *cache_val = (struct cache_val *)val;

    indx_size = (cache_val->keys+1)*sizeof(char *);
    *val_size = indx_size + cache_val->size;

    data = allocator->alloc(allocator, *val_size);
    if (!data) {
        ci_debug_printf(1, "Memory allocation failed inside ldap_module.c:store_val() \n");
        return NULL;
    }
    data_start = data + indx_size;
    data_indx = data;
    val_data = cache_val->data;

    memcpy(data_start, val_data[0], cache_val->size);
    i=0;
    while(val_data[i]!=NULL) {
	data_indx[i]=(char *)(val_data[i] - val_data[0] + indx_size);
	i++;
    }
    data_indx[i] = NULL;
    return data;
}

void *read_val(void *val, int val_size, ci_mem_allocator_t *allocator)
{
    char *data = (char *)malloc(MAX_DATA_SIZE);
    char **indx;
    memcpy(data, val, val_size);
    indx=(char **)data;
    // reindex index of data. Currently it contains the relative position of values
    // inside the data.
    while(*indx){
#if SIZEOF_VOID_P == 8
	*indx = (char *) ((uint64_t)*indx+(uint64_t)data);
#else
	*indx = (char *) ((uint32_t)*indx+(uint32_t)data);
#endif
	indx++;
    }
    return (void *)data;
}


void *ldap_table_open(struct ci_lookup_table *table)
{
    char *path;
    struct ldap_table_data *ldapdata;
    
    path = strdup(table->path);
    if (!path) {
       ci_debug_printf(1, "ldap_table_open: error allocating memory!\n");
       return NULL;
    }

    ldapdata = malloc(sizeof(struct ldap_table_data));
    if(!ldapdata) {
	free(path);
	ci_debug_printf(1, "ldap_table_open: error allocating memory (ldapdata)!\n");
	return NULL;
    }
    ldapdata->str = path;
    ldapdata->pool = NULL;
    ldapdata->base = NULL;
    ldapdata->server = NULL;
    ldapdata->port = 389;
    ldapdata->user = NULL;
    ldapdata->password = NULL;
    ldapdata->attrs = NULL;
    ldapdata->filter = NULL;

    if(!parse_ldap_str(ldapdata)) {
	free(ldapdata->str);
	free(ldapdata);
	ci_debug_printf(1, "ldap_table_open: parse path string error!\n");
	return NULL;
    }
    ldapdata->pool = ldap_pool_create(ldapdata->server, ldapdata->port, 
				      ldapdata->user, ldapdata->password);
    ldapdata->cache = ci_cache_build(65536, 512, 1024, 60, 
				     &ci_str_ops,
				     store_val,
				     read_val
	);
    if(!ldapdata->cache) {
	ci_debug_printf(1, "ldap_table_open: can not create cache! cache is disabled");
    }

    table->data = ldapdata;
    return table->data;
}

void  ldap_table_close(struct ci_lookup_table *table)
{
    struct ldap_table_data *ldapdata;
    ldapdata = (struct ldap_table_data *)table->data;
    table->data = NULL;
    
    //release ldapdata 
    if(ldapdata) {
	free(ldapdata->str);
	if(ldapdata->cache)
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
		*o=*k;
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
    ci_debug_printf(5,"Table ldap search filter is \"%s\"\n", filter);
    return 1;
}

void *ldap_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    //struct ci_mem_allocator *allocator = table->allocator;
    struct ldap_table_data *data = (struct ldap_table_data *)table->data;
    LDAPMessage *msg, *entry;
    BerElement *aber;
    LDAP *ld;
    struct berval **attrs, **a;
    void *return_value=NULL;
    char *attrname;
    int ret = 0,memory_exhausted, failures;
    int value_size, keys_num = 0;
    ci_mem_allocator_t *packer;
    char *indx_values, *data_values, *s;
    char filter[MAX_LDAP_FILTER_SIZE];

    *vals = NULL;
    failures = 0;
    memory_exhausted = 0;
    return_value = NULL;

    if(data->cache && ci_cache_search(data->cache, key, (void *)vals, NULL)) {
	ci_debug_printf(4, "Retrieving from cache....\n");
	return key;
    }

    create_filter(filter, MAX_LDAP_FILTER_SIZE, data->filter,key);

    while ((ld = ldap_connection_open(data->pool)) && failures < 5) {

	ret = ldap_search_ext_s(ld,
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

	ci_debug_printf(4, "Contacting LDAP server: %s\n", ldap_err2string(ret));
	if(ret == LDAP_SUCCESS) {
	    indx_values = (char *)malloc(MAX_DATA_SIZE);
	    data_values = indx_values + DATA_START;
	    packer = ci_create_pack_allocator(data_values,DATA_SIZE);
	    *vals=(void * *)indx_values;
	    (*vals)[0]=NULL;

	    entry = ldap_first_entry(ld, msg);
	    keys_num = 0;
	    while(entry != NULL) {
		aber = NULL;
		attrname = ldap_first_attribute(ld, entry, &aber);
		while(attrname != NULL) {
		    ci_debug_printf(8, "Retrieve attribute:%s. Values: ", attrname);
		    attrs = ldap_get_values_len(ld, entry, attrname);
		    for(a=attrs; *a!=NULL; a++) {
			if(keys_num < (MAX_COLS-1) && 
			   ((*vals)[keys_num] = packer->alloc(packer,(*a)->bv_len+1)) != NULL) {
			    memcpy((*vals)[keys_num], (*a)->bv_val, (*a)->bv_len);
			    /*if the (*a) attribute contains binary data maybe the 
			      (*vals)[keys_num] is not NULL terminated. We are interested 
			      only for normal string data here so:*/
			    s = (char *)(*vals)[keys_num];
			    s[(*a)->bv_len] = '\0';
			    keys_num++;
			}
			else
			    memory_exhausted = 1;
			
			//ci_debug_printf(8, "%s (%d),", (*a)->bv_val, (*a)->bv_len);
		    }
		    ci_debug_printf(8, "\n");
		    ldap_value_free_len(attrs);
		    attrname = ldap_next_attribute(ld, entry, aber);
		}
		(*vals)[keys_num] = NULL;
		if(aber)
		    ber_free(aber, 0);

		if(!return_value)
		    return_value = key;
		
		entry = ldap_next_entry(ld, entry);
	    }
	    value_size = ci_pack_allocator_data_size(packer);
	    ci_mem_allocator_destroy(packer);
	    ldap_msgfree(msg);
	    ldap_connection_release(data->pool, ld, 0);

	    if(data->cache) {
		struct cache_val cache_val;
		cache_val.keys = keys_num;
		cache_val.size = value_size;
		cache_val.data = *vals;
		if (!ci_cache_update(data->cache, key, (void *)&cache_val))
		    ci_debug_printf(4, "adding to cache failed!\n");
	    }
	    return return_value;
	}

	ldap_connection_release(data->pool, ld, 1);

	if (ret != LDAP_SERVER_DOWN) {
	    ci_debug_printf(1, "Error contacting LDAP server: %s\n", ldap_err2string(ret));
	    return NULL;
	}

	failures++;
    }

    ci_debug_printf(1, "Error LDAP server is down: %s\n", ldap_err2string(ret));
    return NULL;
}

void  ldap_table_release_result(struct ci_lookup_table *table,void **val)
{
//    struct ci_mem_allocator *allocator = table->allocator;
//    allocator->free(allocator, val);
    free(val);
}
