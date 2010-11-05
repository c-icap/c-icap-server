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

#include "common.h"
#include "hash.h"
#include "debug.h"
#include <assert.h>

unsigned int ci_hash_compute(unsigned long hash_max_value, void *key, int len)
{
    unsigned long hash = 5381;
    unsigned char *s = key;
    int i;
    
    if(len) {
	for (i=0; i<len; i++,s++)
	    hash = ((hash << 5) + hash) + *s;
    }
    else
	while (*s) {
	    hash = ((hash << 5) + hash) + *s; /* hash * 33 + current char */
	    s++;
	}
    
    if(hash==0) hash++;
    hash = hash & hash_max_value; /*Keep only the bits we need*/
    return hash;
}

struct ci_hash_table * ci_hash_build(unsigned int hash_size, 
				     ci_type_ops_t *ops, ci_mem_allocator_t *allocator)
{
    struct ci_hash_table *htable;
    unsigned int new_hash_size;
    htable = allocator->alloc(allocator, sizeof(struct ci_hash_table));

    if(!htable) {
	/*a debug message ....*/
	return NULL;
    }
    new_hash_size = 63;
    if(hash_size > 63) {
	while(new_hash_size<hash_size && new_hash_size < 0xFFFFFF){
	    new_hash_size++; 
	    new_hash_size = (new_hash_size << 1) -1;
	}
    }
    htable->hash_table=allocator->alloc(allocator, (new_hash_size+1)*sizeof(struct ci_hash_entry *));
    if(!htable->hash_table) {
	allocator->free(allocator, htable);
	return NULL;
    }
    memset(htable->hash_table, 0, (new_hash_size + 1)*sizeof(struct ci_hash_entry *));

    htable->hash_table_size = new_hash_size; 
    htable->ops = ops;
    htable->allocator = allocator;
    return htable;
}

void ci_hash_destroy(struct ci_hash_table *htable)
{
    int i;
    struct ci_hash_entry *e;
    ci_mem_allocator_t *allocator = htable->allocator;
    for (i=0; i<= htable->hash_table_size; i++) {
        while(htable->hash_table[i]) {
            e = htable->hash_table[i];
            htable->hash_table[i] = htable->hash_table[i]->hnext;
            allocator->free(allocator, e);
        }
    }
    htable->allocator->free(allocator,htable->hash_table );
    allocator->free(allocator, htable);
}

const void * ci_hash_search(struct ci_hash_table *htable,const void *key)
{
    struct ci_hash_entry *e;
    unsigned int hash=ci_hash_compute(htable->hash_table_size, key, htable->ops->size(key));
    
    assert(hash <= htable->hash_table_size); /*is it possible?*/
    
    e = htable->hash_table[hash];
    while(e != NULL) {
	if(htable->ops->compare(e->key, key) == 0)
	    return e->val;
	e=e->hnext;
    }
    return NULL;
}

void * ci_hash_add(struct ci_hash_table *htable, const void *key, const void *val)
{
    struct ci_hash_entry *e;
    unsigned int hash=ci_hash_compute(htable->hash_table_size, key, htable->ops->size(key));
    assert(hash <= htable->hash_table_size);

    e = htable->allocator->alloc(htable->allocator, sizeof(struct ci_hash_entry));

    if(!e)
	return NULL;

    e->hnext = NULL;
    e->key=key;
    e->val=val;
    e->hash=hash;
   
//    if(htable->hash_table[hash])
//	ci_debug_printf(9, "ci_hash_update:::Found %s\n", htable->hash_table[hash]->val);

    /*Make it the first entry in the current hash entry*/
    e->hnext=htable->hash_table[hash];
    htable->hash_table[hash] = e;
    return e;
}
