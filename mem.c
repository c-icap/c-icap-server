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
#include "c-icap.h"
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include "ci_threads.h"
#include "debug.h"
#include "mem.h"

int ci_buffers_init();

/*General Functions */
ci_mem_allocator_t *default_allocator;

int mem_init()
{
    int ret;
    ret = ci_buffers_init();
    if(!ret)
	return 0;

    default_allocator = ci_create_os_allocator();
    if (!default_allocator)
	return 0;

    return 1;
}

void mem_reset()
{
}

void ci_mem_allocator_destroy(ci_mem_allocator_t *allocator)
{
    allocator->destroy(allocator);
    /*space for ci_mem_allocator_t struct always should allocated 
      using malloc */
    free(allocator);
}

/*******************************************************************/
/* Buffers pool api functions                                      */
#define BUF_SIGNATURE 0xAA55
struct mem_buffer_block {
    uint16_t sig;
    int ID;
    union {
       double __align;
       char ptr[1];
    } data;
};

#define offsetof(type,member) ((size_t) &((type*)0)->member)
#define PTR_OFFSET offsetof(struct mem_buffer_block,data.ptr[0])

ci_mem_allocator_t *short_buffers[16];
ci_mem_allocator_t *long_buffers[16];

int ci_buffers_init() {
   int i;
   ci_mem_allocator_t *buf64_pool, *buf128_pool, 
	              *buf256_pool,*buf512_pool, *buf1024_pool;
   ci_mem_allocator_t *buf2048_pool, *buf4096_pool,
                      *buf8192_pool, *buf16384_pool, *buf32768_pool;

   buf64_pool = ci_create_pool_allocator(64+PTR_OFFSET, NULL);
   buf128_pool = ci_create_pool_allocator(128+PTR_OFFSET, NULL);
   buf256_pool = ci_create_pool_allocator(256+PTR_OFFSET, NULL);
   buf512_pool = ci_create_pool_allocator(512+PTR_OFFSET, NULL);
   buf1024_pool = ci_create_pool_allocator(1024+PTR_OFFSET, NULL);

   buf2048_pool = ci_create_pool_allocator(2048+PTR_OFFSET, NULL);
   buf4096_pool = ci_create_pool_allocator(4096+PTR_OFFSET, NULL);
   buf8192_pool = ci_create_pool_allocator(8192+PTR_OFFSET, NULL);
   buf16384_pool = ci_create_pool_allocator(16384+PTR_OFFSET, NULL);
   buf32768_pool = ci_create_pool_allocator(32768+PTR_OFFSET, NULL);

   short_buffers[0] = buf64_pool;
   short_buffers[1] = buf128_pool;
   short_buffers[2] = short_buffers[3] = buf256_pool;
   short_buffers[4] = short_buffers[5] = 
              short_buffers[6] = short_buffers[7] = buf512_pool;
   for(i=8; i<16;i++)
      short_buffers[i] = buf1024_pool;

   long_buffers[0] = buf2048_pool;
   long_buffers[1] = buf4096_pool;
   long_buffers[2] = long_buffers[3] = buf8192_pool;
   long_buffers[4] = long_buffers[5] = 
             long_buffers[6] = long_buffers[7] = buf16384_pool; 
   for(i=8; i<16;i++)
      long_buffers[i] = buf32768_pool;

   return 1; 
}

int short_buffer_sizes[16] =  {
    64, 
    128,
    256,256,
    512, 512, 512, 512,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
};

int long_buffer_sizes[16] =  {
    2048,
    4096,
    8192, 8192,
    16384, 16384, 16384, 16384, 
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768
};

void ci_buffers_destroy() {
   int i;
  for(i=0; i<16; i++) {
     if (short_buffers[i] != NULL)
        ci_mem_allocator_destroy(short_buffers[i]); 
  }
}

void *ci_buffer_alloc(int block_size)
{
     int type, size;
     struct mem_buffer_block *block = NULL;
     size = block_size + PTR_OFFSET;
     type = (block_size-1) >> 6;
     if (type< 16 && short_buffers[type] != NULL) {
        block = short_buffers[type]->alloc(short_buffers[type], size);
     }
     else if(type < 512) {
        type = type >> 5;       
        if (long_buffers[type]!= NULL) {
            block = long_buffers[type]->alloc(long_buffers[type], size);
        }
     }

     if(!block) {
        block = (struct mem_buffer_block *)malloc(size); 
     }
     block->sig = BUF_SIGNATURE; 
     block->ID = block_size;
     ci_debug_printf(8, "Geting buffer from pool %d:%d\n", block_size, type);
     return (void *)block->data.ptr;
}

void * ci_buffer_realloc(void *data, int block_size)
{
    int type, full_block_size = 0;
    struct mem_buffer_block *block;

    if (!data)
        return ci_buffer_alloc(block_size);

    block = (struct mem_buffer_block *)(data-PTR_OFFSET);
    if (block->sig != BUF_SIGNATURE) {
        ci_debug_printf(1,"ci_buffer_realloc: ERROR, not internal buffer. This is a bug!!!!");
        return NULL;
    }

    type = (block_size-1) >> 6;
     if (type< 16 && short_buffers[type] != NULL) {
         full_block_size = short_buffer_sizes[type];
     }
     else if(type < 512) {
         type = type >> 5;       
         if (long_buffers[type]!= NULL) {
             full_block_size = long_buffer_sizes[type];
         }
     }
     
     if (!full_block_size)
         full_block_size = block->ID;

     if (block_size > full_block_size) {
         data = ci_buffer_alloc(block_size);
         if (!data)
             return NULL;
         memcpy(data, block->data.ptr, block->ID);
         ci_buffer_free(block->data.ptr);
     }
     
     return data;
}

void ci_buffer_free(void *data) {
    int block_size, type;
    struct mem_buffer_block *block = (struct mem_buffer_block *)(data-PTR_OFFSET);
    if (block->sig != BUF_SIGNATURE) {
        ci_debug_printf(1,"ci_mem_buffer_free: ERROR, not internal buffer. This is a bug!!!!");
        return;
    }
    block_size = block->ID; 
    type = (block_size-1) >> 6;
    if (type < 16 && short_buffers[type] != NULL) {
        short_buffers[type]->free(short_buffers[type], block);
	ci_debug_printf(8, "Store buffer to short pool %d:%d\n", block_size, type);
    }
    else if(type < 512) {
        type = type >> 5;       
        if (long_buffers[type]!= NULL)
            long_buffers[type]->free(long_buffers[type], block);
	else
	    free(block);
	ci_debug_printf(8, "Store buffer to long pool %d:%d\n", block_size, type);
    } else {
        free(block);
    }
}

/*******************************************************************/
/*Object pools                                                     */
#define OBJ_SIGNATURE 0x55AA                                                   
ci_mem_allocator_t **object_pools = NULL;
int object_pools_size=0;
int object_pools_used=0;

int ci_object_pools_init()
{
    return 1;
}

void ci_object_pools_destroy()
{
    int i;
    for(i=0;i< object_pools_used; i++) {
	if (object_pools[i] != NULL)
	    ci_mem_allocator_destroy(object_pools[i]); 
    }
}

#define STEP 128
int ci_object_pool_register(const char *name, int size)
{
    int ID, i;
    ID = -1;
    /*search for an empty position on object_pools and assign here?*/
    if(object_pools == NULL) {
	object_pools = malloc(STEP*sizeof(ci_mem_allocator_t *));
	object_pools_size = STEP;
	ID = 0;
    }
    else {
	for(i=0; i<object_pools_used; i++)  {
	    if (object_pools[i] == NULL) { 
		ID = i; 
		break;
	    }
	}
	if (ID == -1) {
	    if(object_pools_size == object_pools_used) {
		object_pools_size += STEP;
		object_pools = realloc(object_pools, object_pools_size*sizeof(ci_mem_allocator_t *));
	    }
	    ID=object_pools_used;
	}
    }
    if(object_pools == NULL) //??????
	return -1;

    object_pools[ID] = ci_create_pool_allocator(size+PTR_OFFSET, NULL);

    object_pools_used++;
    return ID;
}

void ci_object_pool_unregister(int id)
{
    if(id >= object_pools_used || id < 0) {
	/*A error message ....*/
	return;
    }
    if(object_pools[id]) {
	ci_mem_allocator_destroy(object_pools[id]); 
	object_pools[id] = NULL;
    }

}

void *ci_object_pool_alloc(int id)
{
    struct mem_buffer_block *block = NULL;
    if(id >= object_pools_used || id < 0 || !object_pools[id]) {
	/*A error message ....*/
	ci_debug_printf(1, "Invalid object pool %d. This is a BUG!\n", id);
	return NULL;
    }
    block = object_pools[id]->alloc(object_pools[id], 1/*A small size smaller than obj size*/);
    if(!block) {
	ci_debug_printf(2, "Failed to allocate object from pool %d\n", id);
	return NULL;
    }
    ci_debug_printf(8, "Allocating from objects pool object %d\n", id);
    block->sig = OBJ_SIGNATURE; 
    block->ID = id;
    return (void *)block->data.ptr;
}

void ci_object_pool_free(void *ptr)
{
    struct mem_buffer_block *block = (struct mem_buffer_block *)(ptr-PTR_OFFSET);
    if (block->sig != OBJ_SIGNATURE) {
        ci_debug_printf(1,"ci_mem_buffer_free: ERROR, not internal buffer. This is a bug!!!!\n");
        return;
    }
    if(block->ID > object_pools_used || block->ID < 0 || !object_pools[block->ID]) {
	ci_debug_printf(1,"ci_mem_buffer_free: ERROR, corrupted mem? This is a bug!!!!\n");
	return;
    }
    ci_debug_printf(8, "Storing to objects pool object %d\n", block->ID);
    object_pools[block->ID]->free(object_pools[block->ID], block);    
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

     size = _CI_ALIGN(size);
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

     size = _CI_ALIGN(size); /*round size to a correct alignment size*/
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

     size = _CI_ALIGN(size); /*round size to a correct alignment size*/
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

/*Api functions for pack allocator:*/
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

int ci_pack_allocator_data_size(ci_mem_allocator_t *allocator)
{
   pack_allocator_t *pack_alloc = (pack_allocator_t *)allocator->data;
   return (int) (pack_alloc->curpos - pack_alloc->memchunk);
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
    int alloc_count;
    int hits_count;
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
    palloc->alloc_count = 0;
    palloc->hits_count = 0;
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
	palloc->hits_count++;
    }
    else {
	mem_item = palloc->allocator->alloc(palloc->allocator, 
					    sizeof(struct mem_block_item));
	mem_item->data=NULL;
	data = palloc->allocator->alloc(palloc->allocator, palloc->items_size);
	palloc->alloc_count++;
    }
    
    mem_item->next=palloc->allocated;
    palloc->allocated = mem_item;

    ci_thread_mutex_unlock(&palloc->mutex);
    ci_debug_printf(8, "pool hits:%d allocations: %d\n", palloc->hits_count, palloc->alloc_count);
    return data;
}

void pool_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    struct mem_block_item *mem_item;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    
    ci_thread_mutex_lock(&palloc->mutex);
    if(!palloc->allocated) {
	/*Yes can happen! after a reset but users did not free all objects*/
        palloc->allocator->free(palloc->allocator,p);
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
