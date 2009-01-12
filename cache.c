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
				       unsigned int max_object_size,
				       int ttl,
				       ci_type_ops_t *key_ops,
				       void *(copy_to)(void *val, ci_mem_allocator_t *allocator),
				       void *(copy_from)(void *val, ci_mem_allocator_t *allocator)
    ) 
{
    struct ci_cache_table *cache;
    int i;
    unsigned int new_hash_size;
    ci_mem_allocator_t *allocator;
    
    if(cache_size <= 0 || cache_size > 65535)
	return NULL;

    /*until we are going to create an allocator which can allocate/release from 
     continues memory blocks like those we have in shared memory*/
    allocator = ci_create_os_allocator();

    
    cache = malloc(sizeof(struct ci_cache_table));
    cache->key_ops = key_ops;
    cache->allocator = allocator;
//    cache->data_release = data_release;
    cache->cache_size = cache_size;
    cache->max_object_size = max_object_size;
    cache->copy_to = copy_to;
    cache->copy_from = copy_from;
    
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

void ci_cache_destroy(struct ci_cache_table *cache) 
{
    struct ci_cache_entry *e;
    e = cache->first_queue_entry;
    while(e) {
	cache->first_queue_entry = cache->first_queue_entry->qnext;
	if (e->key)
	    cache->key_ops->free(e->key, cache->allocator);
	if (e->val)
	    cache->allocator->free(cache->allocator, e->val);
	cache->allocator->free(cache->allocator, e);
	e = cache->first_queue_entry;
    }
    cache->allocator->free(cache->allocator, cache->hash_table);
    free(cache);
}

void *ci_cache_search(struct ci_cache_table *cache,void *key, void **val, ci_mem_allocator_t *val_allocator) {
    struct ci_cache_entry *e;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));
    
    if(hash >= cache->hash_table_size) /*is it possible?*/
	return NULL;

    e=cache->hash_table[hash];
    while(e != NULL) {
	ci_debug_printf(10," \t\t->>>>Val %s\n",(char *)e->val);
	ci_debug_printf(10," \t\t->>>>compare %s ~ %s\n",(char *)e->key, (char *)key);
	if(cache->key_ops->compare(e->key, key) == 0) {
	    *val = cache->copy_from(e->val, val_allocator);
	    return key;
	}
	e = e->hnext;
    }
   return NULL;
}

int ci_cache_update(struct ci_cache_table *cache, void *key, void *val) {
    struct ci_cache_entry *e,*tmp;
    int key_size;
    time_t current_time;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));

    ci_debug_printf(10,"Adding :%s:%s\n",(char *)key, (char *)val);

    /*Get the oldest entry*/
    e=cache->first_queue_entry;
    current_time = ci_internal_time();
    
    /*if the oldest entry does not expired do not store tke key/value pair*/
    if((current_time - e->time)< cache->ttl)
	return 0;
    
    /*If it has data release its data*/
    if (e->key) {
	cache->key_ops->free(e->key, cache->allocator);
	e->key = NULL;
    }
    if (e->val) {
	cache->allocator->free(cache->allocator, e->val);
	e->val = NULL;
    }

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
	e->hash = 0;
    }
    
    e->hnext = NULL;
    e->time = 0;

    /*I should implement a ci_type_ops::clone method. Maybe the memcpy is not enough....*/
    key_size = cache->key_ops->size(key);
    e->key = cache->allocator->alloc(cache->allocator, key_size);
    if(!e->key)
	return 0;
    memcpy(e->key, key, key_size);

    e->val = cache->copy_to(val, cache->allocator);
    if(!e->val) {
	cache->allocator->free(cache->allocator, e->key);
	return 0;
    }

    e->hash = hash;
    e->time = current_time;
    cache->first_queue_entry = cache->first_queue_entry->qnext;
    /*Make it the newest entry (make it last entry in queue)*/
    cache->last_queue_entry->qnext = e;
    cache->last_queue_entry = e;
    e->qnext = NULL;

    if(cache->hash_table[hash])
	ci_debug_printf(10,"\t\t:::Found %s\n", (char *)cache->hash_table[hash]->val);
    /*Make it the first entry in the current hash entry*/
    e->hnext=cache->hash_table[hash];
    cache->hash_table[hash] = e;
    return 1;
}


