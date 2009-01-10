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
#include "ci_threads.h"
#include "debug.h"
#include "mem.h"

/*General Functions */

void ci_mem_allocator_destroy(ci_mem_allocator_t *allocator)
{
    allocator->destroy(allocator);
    /*space for ci_mem_allocator_t struct always should allocated 
      using malloc */
    free(allocator);
}

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

ci_mem_allocator_t *ci_create_os_allocator()
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
/****************************************************************/


typedef struct pack_allocator{
     void *memchunk;
     void *curpos;
     void *endpos;
} pack_allocator_t;



void *pack_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
     int max_size;
     void *mem;
     pack_allocator_t *pack_alloc = (pack_allocator_t *)allocator->data;

     if(!pack_alloc)
       return NULL;

     if (size % 4)              /*round size to a multiple of 4 */
          size = (size / 4 + 1) * 4;

     max_size = pack_alloc->endpos - pack_alloc->curpos;

     if (size > max_size)
          return NULL;

     mem = pack_alloc->curpos;
     pack_alloc->curpos += size;
     return mem;
}

void pack_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
  /* We can not free :-)  */
}

void pack_allocator_reset(ci_mem_allocator_t *allocator)
{
  pack_allocator_t *pack_alloc;
  pack_alloc = (pack_allocator_t *)allocator->data;
  pack_alloc->curpos = pack_alloc->memchunk;
}

void pack_allocator_destroy(ci_mem_allocator_t *allocator)
{
    if(allocator->data) {
	free(allocator->data);
	allocator->data = NULL;
    }
}

ci_mem_allocator_t *ci_create_pack_allocator(char *memblock, int size)
{
  ci_mem_allocator_t *allocator;
  pack_allocator_t *pack_alloc = malloc(sizeof(pack_allocator_t));
  if(!pack_alloc)
      return NULL;
  pack_alloc->memchunk = memblock;
  pack_alloc->curpos =pack_alloc->memchunk;
  pack_alloc->endpos = pack_alloc->memchunk + size;
  
  allocator = malloc(sizeof(ci_mem_allocator_t));
  if(!allocator)
    return NULL;
  allocator->alloc = pack_allocator_alloc;
  allocator->free = pack_allocator_free;
  allocator->reset = pack_allocator_reset;
  allocator->destroy = pack_allocator_destroy;
  allocator->data = pack_alloc;
  return allocator;
}


/****************************************************************/

struct mem_block_item {
    void *data;
    struct mem_block_item *next;
};

struct pool_allocator {
    int items_size;
    int strict;
    ci_mem_allocator_t *allocator;
    int free_allocator;
    ci_thread_mutex_t mutex;
    struct mem_block_item *free;
    struct mem_block_item *allocated;
};

struct pool_allocator *pool_allocator_build(int items_size, 
					    int strict,
					    ci_mem_allocator_t *use_alloc)
{
    int free_allocator = 0; 
    struct pool_allocator *palloc;
    
    if(!use_alloc) {
	free_allocator = 1;
	use_alloc = ci_create_os_allocator();
    }
    
    if(!use_alloc) {
	/*a debug message*/
	return NULL;
    }
    palloc = use_alloc->alloc(use_alloc, sizeof(struct pool_allocator));
    
    if(!palloc) {
	/*An error message*/
	if(free_allocator)
	    ci_mem_allocator_destroy(use_alloc);
	return NULL;
    }
    
    palloc->items_size = items_size;
    palloc->strict = strict;
    palloc->free = NULL;
    palloc->allocated = NULL;
    palloc->free_allocator = free_allocator; 
    palloc->allocator = use_alloc;
    ci_thread_mutex_init(&palloc->mutex);
    return palloc;
}

void *pool_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
    struct mem_block_item *mem_item;
    void *data = NULL;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    
    if(size > palloc->items_size)
	return NULL;
    
    ci_thread_mutex_lock(&palloc->mutex);
    
    if(palloc->free) {
	mem_item=palloc->free;
	palloc->free=palloc->free->next;
	data=mem_item->data;
	mem_item->data=NULL;
    }
    else {
	mem_item = palloc->allocator->alloc(palloc->allocator, 
					    sizeof(struct mem_block_item));
	mem_item->data=NULL;
	data = palloc->allocator->alloc(palloc->allocator, palloc->items_size);
    }
    
    mem_item->next=palloc->allocated;
    palloc->allocated = mem_item;
    
    ci_thread_mutex_unlock(&palloc->mutex);
    return data;
}

void pool_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    struct mem_block_item *mem_item;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    
    ci_thread_mutex_lock(&palloc->mutex);
    if(!palloc->allocated) {
	/*Yes can happen! after a reset but users did not free all objects*/
	free(p);
    }
    else {
	mem_item=palloc->allocated;
	palloc->allocated = palloc->allocated->next;
	
	mem_item->data = p;
	mem_item->next = palloc->free;
	palloc->free = mem_item;
    }
    ci_thread_mutex_unlock(&palloc->mutex);
}

void pool_allocator_reset(ci_mem_allocator_t *allocator)
{
    struct mem_block_item *mem_item, *cur;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    
    ci_thread_mutex_lock(&palloc->mutex);
    if(palloc->allocated) {
	mem_item = palloc->allocated;
	while(mem_item!=NULL) {
	    cur = mem_item;
	    mem_item = mem_item->next;
	    palloc->allocator->free(palloc->allocator, cur);
	}
	
    }
    palloc->allocated = NULL;
    if(palloc->free) {
	mem_item = palloc->free;
	while(mem_item!=NULL) {
	    cur = mem_item;
	    mem_item = mem_item->next;
	    palloc->allocator->free(palloc->allocator, cur->data);
	    palloc->allocator->free(palloc->allocator, cur);
	}
    }
    palloc->free = NULL;
    ci_thread_mutex_unlock(&palloc->mutex);
}


void pool_allocator_destroy(ci_mem_allocator_t *allocator)
{
    int free_allocator = 0;
    pool_allocator_reset(allocator);
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    ci_mem_allocator_t *use_alloc = palloc->allocator;
    ci_thread_mutex_destroy(&palloc->mutex);
    free_allocator = palloc->free_allocator;
    use_alloc->free(use_alloc, palloc);
    if(free_allocator)
	ci_mem_allocator_destroy(use_alloc);
}

ci_mem_allocator_t *ci_create_pool_allocator(int items_size, ci_mem_allocator_t *use_alloc)
{
    struct pool_allocator *palloc;
    ci_mem_allocator_t *allocator;
    
    palloc = pool_allocator_build(items_size, 0, use_alloc);
    allocator= malloc(sizeof(ci_mem_allocator_t));
    if (!allocator)
	return NULL;
    allocator->alloc = pool_allocator_alloc;
    allocator->free = pool_allocator_free;
    allocator->reset = pool_allocator_reset;
    allocator->destroy = pool_allocator_destroy;
    allocator->data = palloc;
    return allocator;
}
