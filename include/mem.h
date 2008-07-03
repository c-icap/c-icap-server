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

typedef struct ci_serial_allocator{
     void *memchunk;
     void *curpos;
     void *endpos;
     struct ci_serial_allocator *next;
} ci_serial_allocator_t;

CI_DECLARE_FUNC(ci_serial_allocator_t *) ci_serial_allocator_create(int size);
CI_DECLARE_FUNC(void) ci_serial_allocator_release(ci_serial_allocator_t *allocator);
CI_DECLARE_FUNC(void) ci_serial_allocator_reset(ci_serial_allocator_t *allocator);
CI_DECLARE_FUNC(void) *ci_serial_allocator_alloc(ci_serial_allocator_t *allocator,int size);


#endif
