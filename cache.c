#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "debug.h"
#include "lookup_table.h"
#include "cache.h"


time_t ci_internal_time()
{
    return time(NULL);
}

struct ci_cache_table *ci_cache_build( unsigned int cache_size, 
				       int ttl,
				       unsigned int flags,
				       ci_type_ops_t *ops,
				       void (*data_release)(void *val, ci_mem_allocator_t *allocator),
				       ci_mem_allocator_t *allocator ) {
   struct ci_cache_table *cache;
   int i;
   unsigned int new_hash_size;

   if(cache_size <= 0 || cache_size>65535)
       return NULL;

   cache = malloc(sizeof(struct ci_cache_table));
   cache->flags = flags;
   cache->key_ops = ops;
   cache->allocator = allocator;
   cache->data_release = data_release;
   cache->cache_size = cache_size;

   cache->first_queue_entry = (struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry)); 
   cache->last_queue_entry = cache->first_queue_entry;
   for (i=0; i < cache_size-1; i++) {
       cache->last_queue_entry->hnext=NULL;
       cache->last_queue_entry->qnext=(struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry));
       cache->last_queue_entry=cache->last_queue_entry->qnext;
   }



   new_hash_size = 63;
   if(cache_size > 63) {
       while(new_hash_size<cache_size && new_hash_size < 0xFFFFFF){
	   new_hash_size++; 
	   new_hash_size = (new_hash_size << 1) -1;
       }
   }
   ci_debug_printf(7,"Hash size: %d\n",new_hash_size);
   cache->hash_table=(struct ci_cache_entry **)allocator->alloc(allocator, new_hash_size*sizeof(struct ci_cache_entry *));
   memset(cache->hash_table,0,new_hash_size*sizeof(struct ci_cache_entry *));
   cache->hash_table_size = new_hash_size; 
   cache->ttl = ttl;
   return cache;
}

void *ci_cache_search(struct ci_cache_table *cache,void *key) {
    struct ci_cache_entry *e;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));

    if(hash >= cache->hash_table_size) /*is it possible?*/
	return NULL;

    e=cache->hash_table[hash];
    while(e!=NULL) {
	ci_debug_printf(10," \t\t->>>>Val %s\n",(char *)e->val);
	ci_debug_printf(10," \t\t->>>>compare %s ~ %s\n",(char *)e->key, (char *)key);
	if(cache->key_ops->compare(e->key, key) == 0)
           return e->val;
       e=e->hnext;
    }
   return NULL;
}

void *ci_cache_update(struct ci_cache_table *cache, void *key, void *val) {
    struct ci_cache_entry *e,*tmp;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));

    ci_debug_printf(10,"Adding :%s:%s\n",(char *)key, (char *)val);

    /*Get the oldest entry (TODO:check the cache ttl value if exists)*/
    e=cache->first_queue_entry;
    cache->first_queue_entry=cache->first_queue_entry->qnext;
    /*Make it the newest entry (make it last entry in queue)*/
    cache->last_queue_entry->qnext = e;
    cache->last_queue_entry = e;
    e->qnext = NULL;
    
    /*If it has data release its data*/
    if (e->key)
	cache->key_ops->free(e->key, cache->allocator);
    if (cache->data_release)
	cache->data_release(e->val, cache->allocator);

    /*If it is in the hash table remove it...*/
    if(e->hash) {
	tmp = cache->hash_table[e->hash];
	if(tmp == e)
	    cache->hash_table[e->hash] = tmp->hnext;
	else if(tmp) {
	    while(tmp->hnext != NULL && e != tmp->hnext) tmp = tmp->hnext;
	    if(tmp->hnext)
		tmp->hnext = tmp->hnext->hnext;
	}
    }
    
    e->hnext = NULL;
    e->time = ci_internal_time();
    e->key=key;
    e->val=val;
    e->hash=hash;
   

    if(cache->hash_table[hash])
	ci_debug_printf(10,"\t\t:::Found %s\n", cache->hash_table[hash]->val);
    /*Make it the first entry in the current hash entry*/
    e->hnext=cache->hash_table[hash];
    cache->hash_table[hash] = e;
}


