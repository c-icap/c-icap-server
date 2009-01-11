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

struct ci_cache_entry {
   unsigned int hash;
   time_t time;
   void *key;
   void *val;
   struct ci_cache_entry *qnext;
   struct ci_cache_entry *hnext;
};


struct ci_cache_table { 
    struct ci_cache_entry *first_queue_entry; 
    struct ci_cache_entry *last_queue_entry;
    struct ci_cache_entry **hash_table;
    time_t ttl;
    unsigned int cache_size;
    unsigned int max_object_size;
    unsigned int hash_table_size;
    unsigned int flags;
    ci_type_ops_t *key_ops;
    ci_mem_allocator_t *allocator;
    void *(*copy_to)(void *val, ci_mem_allocator_t *allocator);
    void *(*copy_from)(void *val, ci_mem_allocator_t *allocator);
//    void (*data_release)(void *val, ci_mem_allocator_t *allocator);
};

CI_DECLARE_FUNC(struct ci_cache_table *) ci_cache_build( unsigned int cache_size,
 							 unsigned int max_object_size,
							 int ttl,
							 ci_type_ops_t *key_ops,
							 void *(copy_to)(void *val, ci_mem_allocator_t *allocator),
							 void *(copy_from)(void *val, ci_mem_allocator_t *allocator)
    );

CI_DECLARE_FUNC(void *) ci_cache_search(struct ci_cache_table *cache,void *key, void **val, ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(int) ci_cache_update(struct ci_cache_table *cache, void *key, void *val);
CI_DECLARE_FUNC(void) ci_cache_destroy(struct ci_cache_table *cache);

#endif
