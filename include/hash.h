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

#ifndef __HASH_H
#define __HASH_H

#include "c-icap.h"
#include "lookup_table.h"
#include "mem.h"

struct ci_hash_entry {
   unsigned int hash;
   const void *key;
   const void *val;
   struct ci_hash_entry *hnext;
};


struct ci_hash_table { 
    struct ci_hash_entry **hash_table;
    unsigned int hash_table_size;
    const ci_type_ops_t *ops;
    ci_mem_allocator_t *allocator;
};


CI_DECLARE_FUNC(unsigned int) ci_hash_compute(unsigned long hash_max_value, const void *key, int len);
CI_DECLARE_FUNC(struct ci_hash_table *) ci_hash_build(unsigned int hash_size, 
						      const ci_type_ops_t *ops, 
						      ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(void)   ci_hash_destroy(struct ci_hash_table *htable);
CI_DECLARE_FUNC(const void *) ci_hash_search(struct ci_hash_table *htable,const void *key);
CI_DECLARE_FUNC(void *) ci_hash_add(struct ci_hash_table *htable, const void *key, const void *val);

#endif
