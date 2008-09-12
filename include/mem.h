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

CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_os_allocator(int size);
CI_DECLARE_FUNC(ci_mem_allocator_t *) ci_create_serial_allocator(int size);


#endif
