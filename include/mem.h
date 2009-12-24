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

#ifndef __MEM_H
#define __MEM_H

#include "c-icap.h"

typedef struct ci_mem_allocator {
    void *(*alloc)(struct ci_mem_allocator *,size_t size);
    void (*free)(struct ci_mem_allocator *,void *);
    void (*reset)(struct ci_mem_allocator *);
    void (*destroy)(struct ci_mem_allocator *);
    void *data;
} ci_mem_allocator_t;


CI_DECLARE_DATA extern ci_mem_allocator_t *default_allocator;

CI_DECLARE_FUNC(void) ci_mem_allocator_destroy(ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_os_allocator();
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_serial_allocator(int size);
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_pool_allocator(int items_size, ci_mem_allocator_t *use_alloc);
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_pack_allocator(char *memblock, int size);
CI_DECLARE_FUNC(int) ci_pack_allocator_data_size(ci_mem_allocator_t *allocator);


CI_DECLARE_FUNC(int) ci_buffers_init();
CI_DECLARE_FUNC(void) ci_buffers_destroy();

CI_DECLARE_FUNC(void *)  ci_buffer_alloc(int block_size);
CI_DECLARE_FUNC(void)    ci_buffer_free(void *data);

CI_DECLARE_FUNC(int)     ci_object_pool_register(char *name, int size);
CI_DECLARE_FUNC(void)    ci_object_pool_unregister(int id);
CI_DECLARE_FUNC(void *)  ci_object_pool_alloc(int id);
CI_DECLARE_FUNC(void)    ci_object_pool_free(void *ptr);

#endif
