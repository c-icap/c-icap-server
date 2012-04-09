/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
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

#ifndef __CACHE_H
#define __CACHE_H
#include "hash.h"
#include "proc_mutex.h"
#include "ci_threads.h"

#ifdef __cplusplus
extern "C"
{
#endif

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


typedef struct ci_cache{ 
    struct ci_cache_entry *first_queue_entry; 
    struct ci_cache_entry *last_queue_entry;
    struct ci_cache_entry **hash_table;
    time_t ttl;
    unsigned int cache_size;
    unsigned int mem_size;
    unsigned int max_key_size;
    unsigned int max_object_size;
    unsigned int hash_table_size;
    unsigned int flags;
    ci_type_ops_t *key_ops;
    ci_mem_allocator_t *allocator;
    common_mutex_t mtx;
    void *(*copy_to)(void *val,int *val_size, ci_mem_allocator_t *allocator);
    void *(*copy_from)(void *val,int val_size, ci_mem_allocator_t *allocator);
//    void (*data_release)(void *val, ci_mem_allocator_t *allocator);
} ci_cache_t;

CI_DECLARE_FUNC(struct ci_cache *) ci_cache_build( unsigned int cache_size,
                                                   unsigned int max_key_size,
						   unsigned int max_object_size,
						   int ttl,
						   ci_type_ops_t *key_ops,
						   void *(copy_to_cache)(void *val, int *val_size, ci_mem_allocator_t *allocator),
						   void *(copy_from_cache)(void *val,int val_size, ci_mem_allocator_t *allocator)
    );

CI_DECLARE_FUNC(void *) ci_cache_search(struct ci_cache *cache,void *key, void **val, ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(int) ci_cache_update(struct ci_cache *cache, void *key, void *val);
CI_DECLARE_FUNC(void) ci_cache_destroy(struct ci_cache *cache);


/*
  Only for internal use only:
  cb functions to store/retrieve vectors from cache....
*/
CI_DECLARE_FUNC(void *)ci_cache_store_vector_val(void *val, int *val_size, ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(void *)ci_cache_read_vector_val(void *val, int val_size, ci_mem_allocator_t *allocator);


#ifdef __cplusplus
}
#endif

#endif
