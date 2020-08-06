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

#ifndef __C_ICAP_MEM_H
#define __C_ICAP_MEM_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

enum allocator_types {OS_ALLOC, SERIAL_ALLOC, POOL_ALLOC, PACK_ALLOC};

typedef struct ci_mem_allocator {
    void *(*alloc)(struct ci_mem_allocator *,size_t size);
    void (*free)(struct ci_mem_allocator *,void *);
    void (*reset)(struct ci_mem_allocator *);
    void (*destroy)(struct ci_mem_allocator *);
    void *data;
    char *name;
    int type;
    int must_free;
} ci_mem_allocator_t;


CI_DECLARE_DATA extern ci_mem_allocator_t *default_allocator;

CI_DECLARE_FUNC(void) ci_mem_allocator_destroy(ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_os_allocator();
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_serial_allocator(size_t size);

/*pack_allocator related functions ....*/
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_pack_allocator(char *memblock, size_t  size);
CI_DECLARE_FUNC(int) ci_pack_allocator_data_size(ci_mem_allocator_t *allocator);
CI_DECLARE_FUNC(void *) ci_pack_allocator_alloc(ci_mem_allocator_t *allocator,size_t size);
CI_DECLARE_FUNC(void) ci_pack_allocator_free(ci_mem_allocator_t *allocator,void *p);
/*The following six functions are only for c-icap internal use....*/
CI_DECLARE_FUNC(ci_mem_allocator_t *)ci_create_pack_allocator_on_memblock(char *memblock, size_t size);
CI_DECLARE_FUNC(size_t)  ci_pack_allocator_required_size();
CI_DECLARE_FUNC(void *) ci_pack_allocator_alloc_unaligned(ci_mem_allocator_t *allocator, size_t size);
CI_DECLARE_FUNC(void *) ci_pack_allocator_alloc_from_rear(ci_mem_allocator_t *allocator, int size);
CI_DECLARE_FUNC(void) ci_pack_allocator_set_start_pos(ci_mem_allocator_t *allocator, void *p);
CI_DECLARE_FUNC(void) ci_pack_allocator_set_end_pos(ci_mem_allocator_t *allocator, void *p);

CI_DECLARE_FUNC(int) ci_buffers_init();
CI_DECLARE_FUNC(void) ci_buffers_destroy();

CI_DECLARE_FUNC(void *)  ci_buffer_alloc(size_t block_size);
CI_DECLARE_FUNC(void *)  ci_buffer_alloc2(size_t block_size, size_t *allocated_size);
CI_DECLARE_FUNC(void *)  ci_buffer_realloc(void *data, size_t block_size);
CI_DECLARE_FUNC(void *)  ci_buffer_realloc2(void *data, size_t block_size, size_t *allocated_size);
CI_DECLARE_FUNC(void)    ci_buffer_free(void *data);

CI_DECLARE_FUNC(size_t)  ci_buffer_size(const void *data);
CI_DECLARE_FUNC(int)  ci_buffer_check(const void *data);

CI_DECLARE_FUNC(int)     ci_object_pool_register(const char *name, int size);
CI_DECLARE_FUNC(void)    ci_object_pool_unregister(int id);
CI_DECLARE_FUNC(void *)  ci_object_pool_alloc(int id);
CI_DECLARE_FUNC(void)    ci_object_pool_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
