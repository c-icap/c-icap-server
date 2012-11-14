/*
 *  Copyright (C) 2004-2010 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */

#include "common.h"
#include <time.h>
#include "debug.h"
#include "ci_threads.h"
#include "lookup_table.h"
#include "array.h"
#include "cache.h"
#include <assert.h>

time_t ci_internal_time()
{
    return time(NULL);
}


int common_mutex_init(common_mutex_t *mtx, int proc_mtx)
{
    if(proc_mtx)
	return 0;
    
    mtx->isproc = 0;
    return ci_thread_mutex_init(&mtx->mtx.thread_mutex);    
}

int common_mutex_destroy(common_mutex_t *mtx)
{
    if(mtx->isproc)
	return 0;
    return ci_thread_mutex_destroy(&mtx->mtx.thread_mutex);
}

int common_mutex_lock(common_mutex_t *mtx)
{
    if(mtx->isproc)
	return 0;
    return ci_thread_mutex_lock(&mtx->mtx.thread_mutex);
}

int common_mutex_unlock(common_mutex_t *mtx)
{
    if(mtx->isproc)
	return 0;
    return ci_thread_mutex_unlock(&mtx->mtx.thread_mutex);
}


struct ci_cache *ci_cache_build( unsigned int cache_size,
				 unsigned int max_key_size,
				 unsigned int max_object_size,
				 int ttl,
				 ci_type_ops_t *key_ops,
				 void *(copy_to_cache)(void *val,int *val_size, ci_mem_allocator_t *allocator),
				 void *(copy_from_cache)(void *val, int val_size, ci_mem_allocator_t *allocator)
    ) 
{
    struct ci_cache *cache;
    int i;
    unsigned int new_hash_size, cache_items;
    ci_mem_allocator_t *allocator;
    
    if (cache_size <= 0)
	return NULL;

    cache_items = cache_size/(max_key_size+max_object_size+sizeof(struct ci_cache_entry));
    if (cache_items == 0)
	return NULL;

    /*until we are going to create an allocator which can allocate/release from 
     continues memory blocks like those we have in shared memory*/
    allocator = ci_create_os_allocator();
    if (!allocator)
        return NULL;

    cache = malloc(sizeof(struct ci_cache));
    if (!cache) {
        ci_mem_allocator_destroy(allocator);
        return NULL;
    }
    cache->key_ops = key_ops;
    cache->allocator = allocator;
//    cache->data_release = data_release;
    cache->cache_size = cache_items;
    cache->mem_size = cache_size;
    cache->max_key_size = max_key_size;
    cache->max_object_size = max_object_size;
    cache->copy_to = copy_to_cache;
    cache->copy_from = copy_from_cache;
    
    cache->first_queue_entry = (struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry)); 
    if (!cache->first_queue_entry) {
        ci_mem_allocator_destroy(allocator);
        free(cache);
        return NULL;
    }
    cache->last_queue_entry = cache->first_queue_entry;
    cache->last_queue_entry->hnext=NULL;
    cache->last_queue_entry->qnext = NULL;
    cache->last_queue_entry->key = NULL;
    cache->last_queue_entry->val = NULL;
    cache->last_queue_entry->time = 0;
    cache->last_queue_entry->hash = 0;
    for (i=0; i < cache_items-1; i++) {
	cache->last_queue_entry->qnext=(struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry));
        if (!cache->last_queue_entry->qnext) {
            /*we are leaking here the cache->first_queue_entry elements. TODO...*/
            ci_mem_allocator_destroy(allocator);
            free(cache);
            return NULL;            
        }
	cache->last_queue_entry=cache->last_queue_entry->qnext;
	cache->last_queue_entry->hnext=NULL;
        cache->last_queue_entry->qnext = NULL;
	cache->last_queue_entry->key = NULL;
	cache->last_queue_entry->val = NULL;
	cache->last_queue_entry->time = 0;
	cache->last_queue_entry->hash = 0;
    }
    
    new_hash_size = 63;
    if(cache_items > 63) {
	while(new_hash_size<cache_items && new_hash_size < 0xFFFFFF){
	    new_hash_size++; 
	    new_hash_size = (new_hash_size << 1) -1;
	}
    }
    ci_debug_printf(7,"Hash size: %d\n",new_hash_size);
    cache->hash_table=(struct ci_cache_entry **)allocator->alloc(allocator, (new_hash_size+1)*sizeof(struct ci_cache_entry *));
    if (!cache->hash_table) {
        /*we are leaking here the cache->first_queue_entry elements. TODO...*/
        ci_mem_allocator_destroy(allocator);
        free(cache);
        return NULL;
    }
    memset(cache->hash_table,0,(new_hash_size+1)*sizeof(struct ci_cache_entry *));
    cache->hash_table_size = new_hash_size; 
    cache->ttl = ttl;
    common_mutex_init(&cache->mtx, 0);

    return cache;
}

void ci_cache_destroy(struct ci_cache *cache) 
{
    struct ci_cache_entry *e;
    e = cache->first_queue_entry;
    while(e) {
	cache->first_queue_entry = cache->first_queue_entry->qnext;
	if (e->key)
	    cache->key_ops->free(e->key, cache->allocator);
	if (e->val && e->val_size > 0)
	    cache->allocator->free(cache->allocator, e->val);
	cache->allocator->free(cache->allocator, e);
	e = cache->first_queue_entry;
    }
    cache->allocator->free(cache->allocator, cache->hash_table);
    common_mutex_destroy(&cache->mtx);
    ci_mem_allocator_destroy(cache->allocator);
    free(cache);
}

void *ci_cache_search(struct ci_cache *cache,void *key, void **val, ci_mem_allocator_t *val_allocator) {
    struct ci_cache_entry *e;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));
    
    assert(hash <= cache->hash_table_size);

    common_mutex_lock(&cache->mtx);
    e=cache->hash_table[hash];
    while(e != NULL) {
	ci_debug_printf(10," \t\t->>>>Val %s\n",(char *)e->val);
	ci_debug_printf(10," \t\t->>>>compare %s ~ %s\n",(char *)e->key, (char *)key);
	if(cache->key_ops->compare(e->key, key) == 0) {
	    *val = cache->copy_from(e->val, e->val_size, val_allocator);
	    common_mutex_unlock(&cache->mtx);
	    return key;
	}
        assert(e != e->hnext);
	e = e->hnext;
    }
    common_mutex_unlock(&cache->mtx);
    return NULL;
}

int ci_cache_update(struct ci_cache *cache, void *key, void *val) {
    struct ci_cache_entry *e,*tmp;
    int key_size;
    time_t current_time;
    unsigned int hash=ci_hash_compute(cache->hash_table_size, key, cache->key_ops->size(key));
    assert(hash <= cache->hash_table_size);
    ci_debug_printf(10,"Adding :%s:%s\n",(char *)key, (char *)val);

    current_time = ci_internal_time();

    common_mutex_lock(&cache->mtx);
    /*Get the oldest entry*/
    e=cache->first_queue_entry;
    
    /*if the oldest entry does not expired do not store tke key/value pair*/
    if((current_time - e->time)< cache->ttl) {
	ci_debug_printf(6, "ci_cache_update: not available slot (%d-%d %d).\n",
			(unsigned int) current_time,
			(unsigned int) e->time, 
			(unsigned int) cache->ttl
	    );
	common_mutex_unlock(&cache->mtx);
	return 0;
    }
    
    /*If it has data release its data*/
    if (e->key) {
	cache->key_ops->free(e->key, cache->allocator);
	e->key = NULL;
    }
    if (e->val && e->val_size > 0) {
	cache->allocator->free(cache->allocator, e->val);
	e->val = NULL;
    }

    /*If it is in the hash table remove it...*/
    {
        assert(e->hash <= cache->hash_table_size);
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
    e->time = 0;
    e->hash = 0;

    /*I should implement a ci_type_ops::clone method. Maybe the memcpy is not enough....*/
    key_size = cache->key_ops->size(key);
    e->key = cache->allocator->alloc(cache->allocator, key_size);
    if(!e->key) {
	common_mutex_unlock(&cache->mtx);
	ci_debug_printf(6, "ci_cache_update: failed to allocate memory for key.\n");
	return 0;
    }
    memcpy(e->key, key, key_size);

    if (val != NULL) {
	e->val = cache->copy_to(val, &e->val_size, cache->allocator);
	if(!e->val) {
	    cache->allocator->free(cache->allocator, e->key);
	    e->key = NULL;
	    common_mutex_unlock(&cache->mtx);
	    ci_debug_printf(6, "ci_cache_update: failed to allocate memory for cache data.\n");
	    return 0;
	}
    }
    else
	e->val = NULL;

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

    common_mutex_unlock(&cache->mtx);
    return 1;
}


void *ci_cache_store_vector_val(void *val, int *val_size, ci_mem_allocator_t *allocator)
{
    int  vector_data_size, vector_indx_size, i;
    void *vector_data_start;
    void *vector_data_end;
    void *data, **data_indx;
    ci_vector_t *v = (ci_vector_t *)val;

    if (!val) {
        *val_size = 0;
        return NULL;
    }

    /*The vector data stored in a continue memory block which filled from 
      bottom to up. So the last elements stored at the beggining of the 
      memory block. */
    vector_data_start = (void *)(v->items[v->count -1]);
    vector_data_end = v->mem +v->max_size;
    /*Assert that the vector stored in one memory block (eg it is not a ci_ptr_vector_t object)*/
    assert(vector_data_start < vector_data_end && vector_data_start > (void *)v->mem);

    /*compute the required memory for storing the vector*/
    vector_data_size = vector_data_end - vector_data_start;
    vector_indx_size = (v->count+1) * sizeof(void *);
    *val_size = sizeof(size_t) + vector_indx_size + vector_data_size ;
    
    data = allocator->alloc(allocator, *val_size);
    if (!data) {
        ci_debug_printf(1, "store_str_vector_val: error allocation memory of size %d\n", *val_size);
        return NULL;
    }

    /*store the size of vector*/
    memcpy(data, &(v->max_size), sizeof(size_t));
    data_indx = data + sizeof(size_t);
    memcpy((void *)data_indx+vector_indx_size, vector_data_start, vector_data_size);

    /*Store the relative position of the vector item to the index part*/
    for(i = 0; v->items[i]!= NULL; i++)
        data_indx[i] = (void *)(v->items[i] - vector_data_start + vector_indx_size);
    data_indx[i] = NULL;

    return data;
}

void *ci_cache_read_vector_val(void *val, int val_size, ci_mem_allocator_t *allocator)
{
    size_t vector_size, item_size;
    int i;
    ci_vector_t *v;
    void **data_indx;

    if (!val)
        return NULL;

    data_indx = (void **)(val + sizeof(size_t));
    vector_size = *((size_t *)val);
    v= ci_vector_create(vector_size);
    
    /*The items stores from bottom to top.
      Compute the size of first item, which stored at the end of *val*/
    item_size = val_size - sizeof(size_t) - (size_t)data_indx[0];
    for(i=0; data_indx[i] != NULL; i++) {
        ci_vector_add(v, (void *)((void *)data_indx+(size_t)data_indx[i]), item_size);
        /*compute the item size of the next item*/
        item_size = data_indx[i] - data_indx[i+1];
    }

    return v;
}

