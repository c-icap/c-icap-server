/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "c-icap.h"
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include "debug.h"
#include "mem.h"

ci_serial_allocator_t *ci_serial_allocator_create(int size)
{
     ci_serial_allocator_t *allocator;
     allocator = malloc(sizeof(ci_serial_allocator_t));
     if (!allocator)
          return NULL;
     if (size % 4)
          size = (size / 4 + 1) * 4;
     allocator->memchunk = malloc(size);
     if (!allocator->memchunk) {
          free(allocator);
          return NULL;
     }
     allocator->curpos = allocator->memchunk;
     allocator->endpos = allocator->memchunk + size;
     allocator->next = NULL;
     return allocator;
}

void ci_serial_allocator_release(ci_serial_allocator_t * allocator)
{
     ci_serial_allocator_t *cur, *next;
     cur = allocator;
     next = allocator->next;
     while (cur) {
          free(cur->memchunk);
          free(cur);
          cur = next;
          if (next)
               next = next->next;
     }
}

void ci_serial_allocator_reset(ci_serial_allocator_t * allocator)
{
     ci_serial_allocator_t *cur;
     cur = allocator;
     while (cur) {
          cur->curpos = cur->memchunk;
          cur = cur->next;
     }
}

void *ci_serial_allocator_alloc(ci_serial_allocator_t * allocator, int size)
{
     int max_size, free_size = 0;
     void *mem;

     if (size % 4)              /*round size to a multiple of 4 */
          size = (size / 4 + 1) * 4;

     max_size = allocator->endpos - allocator->memchunk;
     if (size > max_size)
          return NULL;

     while (size > (allocator->endpos - allocator->curpos)) {
          if (allocator->next == NULL) {
               allocator->next = ci_serial_allocator_create(max_size);
               if (!allocator->next)
                    return NULL;
          }
          allocator = allocator->next;
     }

     mem = allocator->curpos;
     allocator->curpos += size;
     return mem;
}
