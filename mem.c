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


#include "c-icap.h"
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include "debug.h"
#include "mem.h"

/*******************************************************************/
/*A simple allocator implementation which uses the system malloc    */

void *os_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
  return malloc(size);
}

void os_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
  free(p);
}

void os_allocator_reset(ci_mem_allocator_t *allocator)
{
  /*nothing to do*/
}

void os_allocator_destroy(ci_mem_allocator_t *allocator)
{
  /*nothing to do*/
}

ci_mem_allocator_t *ci_create_os_allocator(int size)
{
  ci_mem_allocator_t *allocator = malloc(sizeof(ci_mem_allocator_t));
  if(!allocator)
    return NULL;
  allocator->alloc = os_allocator_alloc;
  allocator->free = os_allocator_free;
  allocator->reset = os_allocator_reset;
  allocator->destroy = os_allocator_destroy;
  allocator->data = NULL;
  return allocator;
}



/************************************************************/
/* The serial allocator implementation                      */


typedef struct serial_allocator{
     void *memchunk;
     void *curpos;
     void *endpos;
     struct serial_allocator *next;
} serial_allocator_t;


serial_allocator_t *serial_allocator_build(int size)
{
     serial_allocator_t *serial_alloc;
     serial_alloc = malloc(sizeof(serial_allocator_t));
     if (!serial_alloc)
          return NULL;
     if (size % 4)
          size = (size / 4 + 1) * 4;
     serial_alloc->memchunk = malloc(size);
     if (!serial_alloc->memchunk) {
          free(serial_alloc);
          return NULL;
     }
     serial_alloc->curpos = serial_alloc->memchunk;
     serial_alloc->endpos = serial_alloc->memchunk + size;
     serial_alloc->next = NULL;
     return serial_alloc;
}

void *serial_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
     int max_size;
     void *mem;
     serial_allocator_t *serial_alloc = (serial_allocator_t *)allocator->data;

     if(!serial_alloc)
       return NULL;

     if (size % 4)              /*round size to a multiple of 4 */
          size = (size / 4 + 1) * 4;

     max_size = serial_alloc->endpos - serial_alloc->memchunk;
     if (size > max_size)
          return NULL;

     while (size > (serial_alloc->endpos - serial_alloc->curpos)) {
          if (serial_alloc->next == NULL) {
               serial_alloc->next = serial_allocator_build(max_size);
               if (!serial_alloc->next)
                    return NULL;
          }
          serial_alloc = serial_alloc->next;
     }

     mem = serial_alloc->curpos;
     serial_alloc->curpos += size;
     return mem;
}

void serial_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
  /* We can not free :-)  */
}

void serial_allocator_reset(ci_mem_allocator_t *allocator)
{
  serial_allocator_t *cur;
  cur = (serial_allocator_t *)allocator->data;
  while (cur) {
    cur->curpos = cur->memchunk;
    cur = cur->next;
  }
}

void serial_allocator_destroy(ci_mem_allocator_t *allocator)
{
  serial_allocator_t *cur, *next;

  if(!allocator->data)
    return;

  cur = (serial_allocator_t *)allocator->data;
  next = cur->next;
  while (cur) {
    free(cur->memchunk);
    free(cur);
    cur = next;
    if (next)
      next = next->next;
  }
}

ci_mem_allocator_t *ci_create_serial_allocator(int size)
{
  ci_mem_allocator_t *allocator;
  serial_allocator_t *sdata= serial_allocator_build(size);
  if(!sdata)
    return NULL;
  allocator = malloc(sizeof(ci_mem_allocator_t));
  if(!allocator)
    return NULL;
  allocator->alloc = serial_allocator_alloc;
  allocator->free = serial_allocator_free;
  allocator->reset = serial_allocator_reset;
  allocator->destroy = serial_allocator_destroy;
  allocator->data = sdata;
  return allocator;
}
