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
#include "registry.h"
#include "proc_mutex.h"
#include "ci_threads.h"
#include <assert.h>

time_t ci_internal_time()
{
    return time(NULL);
}

void ci_cache_type_register(const struct ci_cache_type *type)
{
    ci_registry_add_item("c-icap::ci_cache_type", type->name, type);
}

static const ci_cache_type_t *ci_cache_type_get(const char *name)
{
    return (const ci_cache_type_t *)ci_registry_get_item("c-icap::ci_cache_type", name);
}

void ci_cache_destroy(ci_cache_t *cache) 
{
    cache->destroy(cache);
    free(cache);
}

const void *ci_cache_search(ci_cache_t *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data))
{
    return cache->search(cache, key, val, data, dup_from_cache);
}

int ci_cache_update(ci_cache_t *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size)) {
    return cache->update(cache, key, val, val_size, copy_to_cache);
}

/*****************************************/
/*Simple local cache implementation      */

int ci_local_cache_init(struct ci_cache *cache, const char *name);
const void *ci_local_cache_search(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));
int ci_local_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size));
void ci_local_cache_destroy(struct ci_cache *cache);

struct ci_cache_type ci_local_cache = {
    ci_local_cache_init,
    ci_local_cache_search,
    ci_local_cache_update,
    ci_local_cache_destroy,
    "local"
};

struct ci_cache_entry {
   unsigned int hash;
   time_t time;
   void *key;
   void *val;
   int val_size;
   struct ci_cache_entry *qnext;
   struct ci_cache_entry *hnext;
};

typedef struct common_mutex {
    int isproc;
    union {
	ci_proc_mutex_t proc_mutex;
	ci_thread_mutex_t thread_mutex;
    } mtx;
} common_mutex_t;

struct ci_local_cache_data {
    struct ci_cache_entry *first_queue_entry; 
    struct ci_cache_entry *last_queue_entry;
    struct ci_cache_entry **hash_table;
    unsigned int hash_table_size;
    ci_mem_allocator_t *allocator;
    common_mutex_t mtx;
};

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

int ci_local_cache_init(struct ci_cache *cache, const char *name)
{
    struct ci_local_cache_data *cache_data;
    int i;
    unsigned int new_hash_size;
    ci_mem_allocator_t *allocator;

    cache_data = malloc(sizeof(struct ci_local_cache_data));
    if (!cache_data)
        return 0;
    cache->cache_data = cache_data;

    /*until we are going to create an allocator which can allocate/release from 
     continues memory blocks like those we have in shared memory*/
    allocator = ci_create_os_allocator();
    if (!allocator) {
        free(cache_data);
        return 0;
    }

    cache_data->allocator = allocator;    
    cache_data->first_queue_entry = (struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry)); 
    if (!cache_data->first_queue_entry) {
        ci_mem_allocator_destroy(allocator);
        free(cache_data);
        return 0;
    }
    cache_data->last_queue_entry = cache_data->first_queue_entry;
    cache_data->last_queue_entry->hnext=NULL;
    cache_data->last_queue_entry->qnext = NULL;
    cache_data->last_queue_entry->key = NULL;
    cache_data->last_queue_entry->val = NULL;
    cache_data->last_queue_entry->time = 0;
    cache_data->last_queue_entry->hash = 0;

    unsigned int cache_items = cache->mem_size/(cache->max_object_size+sizeof(struct ci_cache_entry));
    if (cache_items == 0) {
        ci_mem_allocator_destroy(allocator);
        free(cache_data);
	return 0;
    }

    for (i=0; i < cache_items-1; i++) {
	cache_data->last_queue_entry->qnext=(struct ci_cache_entry *)allocator->alloc(allocator, sizeof(struct ci_cache_entry));
        if (!cache_data->last_queue_entry->qnext) {
            /*we are leaking here the cache->first_queue_entry elements. TODO...*/
            ci_mem_allocator_destroy(allocator);
            return 0;            
        }
	cache_data->last_queue_entry=cache_data->last_queue_entry->qnext;
	cache_data->last_queue_entry->hnext=NULL;
        cache_data->last_queue_entry->qnext = NULL;
	cache_data->last_queue_entry->key = NULL;
	cache_data->last_queue_entry->val = NULL;
	cache_data->last_queue_entry->time = 0;
	cache_data->last_queue_entry->hash = 0;
    }
    
    new_hash_size = 63;
    if(cache_items > 63) {
	while(new_hash_size<cache_items && new_hash_size < 0xFFFFFF){
	    new_hash_size++; 
	    new_hash_size = (new_hash_size << 1) -1;
	}
    }
    ci_debug_printf(7,"Hash size: %d\n",new_hash_size);
    cache_data->hash_table=(struct ci_cache_entry **)allocator->alloc(allocator, (new_hash_size+1)*sizeof(struct ci_cache_entry *));
    if (!cache_data->hash_table) {
        /*we are leaking here the cache->first_queue_entry elements. TODO...*/
        ci_mem_allocator_destroy(allocator);
        free(cache);
        free(cache_data);
        return 0;
    }
    memset(cache_data->hash_table,0,(new_hash_size+1)*sizeof(struct ci_cache_entry *));
    cache_data->hash_table_size = new_hash_size; 

    common_mutex_init(&cache_data->mtx, 0);

    return 1;
}

void ci_local_cache_destroy(struct ci_cache *cache)
{
   struct ci_cache_entry *e;
   struct ci_local_cache_data *cache_data;
   cache_data = (struct ci_local_cache_data *)cache->cache_data;
   e = cache_data->first_queue_entry;
   while(e) {
       cache_data->first_queue_entry = cache_data->first_queue_entry->qnext;
       if (e->key)
           cache->key_ops->free(e->key, cache_data->allocator);
       if (e->val && e->val_size > 0)
           cache_data->allocator->free(cache_data->allocator, e->val);
       cache_data->allocator->free(cache_data->allocator, e);
       e = cache_data->first_queue_entry;
   }
   cache_data->allocator->free(cache_data->allocator, cache_data->hash_table);
   common_mutex_destroy(&cache_data->mtx);
   ci_mem_allocator_destroy(cache_data->allocator);
   free(cache_data);
}

const void *ci_local_cache_search(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data))
{
    struct ci_cache_entry *e;
    struct ci_local_cache_data *cache_data;
    time_t current_time;
    cache_data = (struct ci_local_cache_data *)cache->cache_data;

    unsigned int hash=ci_hash_compute(cache_data->hash_table_size, key, cache->key_ops->size(key));
    
    assert(hash <= cache_data->hash_table_size);

    common_mutex_lock(&cache_data->mtx);
    e=cache_data->hash_table[hash];
    *val = NULL;
    while(e != NULL) {
	ci_debug_printf(10," \t\t->>>>Val %s\n",(char *)e->val);
	ci_debug_printf(10," \t\t->>>>compare %s ~ %s\n",(char *)e->key, (char *)key);
	if(cache->key_ops->compare(e->key, key) == 0) {
            current_time = ci_internal_time();
            if ((current_time - e->time) > cache->ttl) /*if expired*/
                key = NULL;
            else if (e->val_size) {
                if (dup_from_cache)
                    *val = dup_from_cache(e->val, e->val_size, data);
                else {
                    *val = ci_buffer_alloc(e->val_size);
                    memcpy(*val, e->val, e->val_size);
                }
            }
            common_mutex_unlock(&cache_data->mtx);
	    return key;
	}
        assert(e != e->hnext);
	e = e->hnext;
    }
    common_mutex_unlock(&cache_data->mtx);
    return NULL;
}

int ci_local_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size))
{
    struct ci_cache_entry *e,*tmp;
    int key_size;
    time_t current_time;
    struct ci_local_cache_data *cache_data;
    unsigned int hash;
    cache_data = (struct ci_local_cache_data *)cache->cache_data;
    hash = ci_hash_compute(cache_data->hash_table_size, key, cache->key_ops->size(key));

    assert(hash <= cache_data->hash_table_size);
    ci_debug_printf(10,"Adding :%s:%p\n",(char *)key, (char *)val);

    current_time = ci_internal_time();

    common_mutex_lock(&cache_data->mtx);
    /*Get the oldest entry*/
    e=cache_data->first_queue_entry;
    
    /*if the oldest entry does not expired do not store tke key/value pair*/
    if((current_time - e->time)< cache->ttl) {
	ci_debug_printf(6, "ci_cache_update: not available slot (%d-%d %d).\n",
			(unsigned int) current_time,
			(unsigned int) e->time, 
			(unsigned int) cache->ttl
	    );
	common_mutex_unlock(&cache_data->mtx);
	return 0;
    }
    
    /*If it has data release its data*/
    if (e->key) {
	cache->key_ops->free(e->key, cache_data->allocator);
	e->key = NULL;
    }
    if (e->val && e->val_size > 0) {
	cache_data->allocator->free(cache_data->allocator, e->val);
	e->val = NULL;
    }

    /*If it is in the hash table remove it...*/
    {
        assert(e->hash <= cache_data->hash_table_size);
	tmp = cache_data->hash_table[e->hash];
	if(tmp == e)
	    cache_data->hash_table[e->hash] = tmp->hnext;
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
    e->key = cache_data->allocator->alloc(cache_data->allocator, key_size);
    if(!e->key) {
	common_mutex_unlock(&cache_data->mtx);
	ci_debug_printf(6, "ci_cache_update: failed to allocate memory for key.\n");
	return 0;
    }
    memcpy(e->key, key, key_size);

    if (val != NULL && val_size > 0) {
        e->val = cache_data->allocator->alloc(cache_data->allocator, val_size);
        e->val_size = val_size;
        if (e->val) {
            if (copy_to_cache) {
                if (!copy_to_cache(e->val, val, e->val_size)) {
                    cache_data->allocator->free(cache_data->allocator, e->val);
                    e->val = NULL;
                }
            } else
                memcpy(e->val, val, e->val_size);
        }
	if(!e->val) {
	    cache_data->allocator->free(cache_data->allocator, e->key);
	    e->key = NULL;
	    common_mutex_unlock(&cache_data->mtx);
	    ci_debug_printf(6, "ci_cache_update: failed to allocate memory for cache data.\n");
	    return 0;
	}
    }
    else {
	e->val = NULL;
        e->val_size = 0;
    }

    e->hash = hash;
    e->time = current_time;
    cache_data->first_queue_entry = cache_data->first_queue_entry->qnext;
    /*Make it the newest entry (make it last entry in queue)*/
    cache_data->last_queue_entry->qnext = e;
    cache_data->last_queue_entry = e;
    e->qnext = NULL;

    if(cache_data->hash_table[hash])
	ci_debug_printf(10,"\t\t:::Found %s\n", (char *)cache_data->hash_table[hash]->val);
    /*Make it the first entry in the current hash entry*/
    e->hnext=cache_data->hash_table[hash];
    cache_data->hash_table[hash] = e;

    common_mutex_unlock(&cache_data->mtx);
    return 1;
}

struct ci_cache *ci_cache_build( const char *name,
                                 const char *cache_type, 
                                 unsigned int cache_size,
				 unsigned int max_object_size,
				 int ttl,
				 const ci_type_ops_t *key_ops
    ) 
{
    struct ci_cache *cache;
    const ci_cache_type_t *type;

    if (cache_size <= 0)
	return NULL;

    type = ci_cache_type_get(cache_type);
    if (type == NULL) {
        type = &ci_local_cache;
        if (strcasecmp(cache_type, ci_local_cache.name) != 0)
            ci_debug_printf(1, "WARNING: Cache type '%s' not found. Creating a local cache\n", cache_type);
    }

    cache = malloc(sizeof(struct ci_cache));
    if (!cache) {
        return NULL;
    }
    if (key_ops != NULL)
        cache->key_ops = key_ops;
    else
        cache->key_ops = &ci_str_ops;
    cache->mem_size = cache_size;
    cache->max_object_size = max_object_size;
    cache->ttl = ttl;
    cache->init = type->init;
    cache->destroy = type->destroy;
    cache->search = type->search;
    cache->update = type->update;
    cache->_cache_type = type;

    if (!cache->init(cache, name)) {
        free(cache);
        return NULL;
    }
    return cache;
}

size_t ci_cache_store_vector_size(ci_vector_t *v)
{
    int  vector_data_size, vector_indx_size;
    void *vector_data_start;
    void *vector_data_end;
    if (!v)
        return 0;
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
    return sizeof(size_t) + vector_indx_size + vector_data_size ;
}

void *ci_cache_store_vector_val(void *buf, const void *val, size_t buf_size)
{
    int  vector_data_size, vector_indx_size, i;
    const void *vector_data_start;
    const void *vector_data_end;
    void *data, **data_indx;
    ci_vector_t *v = (ci_vector_t *)val;

    if (!val || !buf) /*Maybe look for error?*/
        return NULL;

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
    assert(buf_size >= sizeof(size_t) + vector_indx_size + vector_data_size);
    
    data = buf;

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

void *ci_cache_read_vector_val(const void *val, size_t val_size, void *o)
{
    size_t vector_size, item_size;
    int i;
    ci_vector_t *v;
    const void **data_indx;

    if (!val)
        return NULL;

    data_indx = (const void **)(val + sizeof(size_t));
    vector_size = *((size_t *)val);
    v= ci_vector_create(vector_size);
    
    /*The items stores from bottom to top.
      Compute the size of first item, which stored at the end of *val*/
    item_size = val_size - sizeof(size_t) - (size_t)data_indx[0];
    for(i=0; data_indx[i] != NULL; i++) {
        ci_vector_add(v, (const void *)((const void *)data_indx+(size_t)data_indx[i]), item_size);
        /*compute the item size of the next item*/
        item_size = data_indx[i] - data_indx[i+1];
    }

    return v;
}

