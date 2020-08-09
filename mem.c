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
#include <assert.h>

int ci_buffers_init();

/*General Functions */
ci_mem_allocator_t *default_allocator = NULL;
static int MEM_ALLOCATOR_POOL = -1;
static int PACK_ALLOCATOR_POOL = -1;

static size_t sizeof_pack_allocator();
ci_mem_allocator_t *ci_create_pool_allocator(int items_size);

CI_DECLARE_FUNC(int) mem_init()
{
    int ret = -1;
    ret = ci_buffers_init();

    default_allocator = ci_create_os_allocator();
    if (!default_allocator && ret ==-1)
        ret = 0;

    MEM_ALLOCATOR_POOL = ci_object_pool_register("ci_mem_allocator_t", sizeof(ci_mem_allocator_t));
    assert(MEM_ALLOCATOR_POOL >= 0);

    PACK_ALLOCATOR_POOL = ci_object_pool_register("pack_allocator_t", sizeof_pack_allocator());
    assert(PACK_ALLOCATOR_POOL >= 0);

    return ret;
}

void mem_reset()
{
}

void ci_mem_allocator_destroy(ci_mem_allocator_t *allocator)
{
    allocator->destroy(allocator);
    /*space for ci_mem_allocator_t struct is not always allocated
      using malloc */
    if (allocator->must_free == 1)
        free(allocator);
    else if (allocator->must_free == 2)
        ci_object_pool_free(allocator);
    /*
      else if (allocator->must_free == 0)
          user is responsible to release the struct
    */

}

/******************/
static ci_mem_allocator_t *alloc_mem_allocator_struct()
{
    ci_mem_allocator_t *alc;
    if (MEM_ALLOCATOR_POOL < 0) {
        alc = (ci_mem_allocator_t *) malloc(sizeof(ci_mem_allocator_t));
        alc->must_free = 1;
    } else {
        alc = ci_object_pool_alloc(MEM_ALLOCATOR_POOL);
        alc->must_free = 2;
    }

    return alc;
}

/*******************************************************************/
/* Buffers pool api functions                                      */
#define BUF_SIGNATURE 0xAA55
struct mem_buffer_block {
    uint16_t sig;
    size_t ID;
    union {
        double __align;
        char ptr[1];
    } data;
};

#if !defined(offsetof)
#define offsetof(type,member) ((size_t) &((type*)0)->member)
#endif
#define PTR_OFFSET offsetof(struct mem_buffer_block,data.ptr[0])

static ci_mem_allocator_t *short_buffers[16];
static ci_mem_allocator_t *long_buffers[16];

enum {
    BUF64_POOL, BUF128_POOL, BUF256_POOL,BUF512_POOL, BUF1024_POOL,
    BUF2048_POOL, BUF4096_POOL, BUF8192_POOL, BUF16384_POOL, BUF32768_POOL,
    BUF_END_POOL
};

static ci_mem_allocator_t *Pools[BUF_END_POOL];

int ci_buffers_init()
{
    int i;
    memset(Pools, 0, sizeof(Pools));
    memset(short_buffers, 0, sizeof(short_buffers));
    memset(long_buffers, 0, sizeof(long_buffers));

    Pools[BUF64_POOL] = ci_create_pool_allocator(64+PTR_OFFSET);
    Pools[BUF128_POOL] = ci_create_pool_allocator(128+PTR_OFFSET);
    Pools[BUF256_POOL] = ci_create_pool_allocator(256+PTR_OFFSET);
    Pools[BUF512_POOL] = ci_create_pool_allocator(512+PTR_OFFSET);
    Pools[BUF1024_POOL] = ci_create_pool_allocator(1024+PTR_OFFSET);

    Pools[BUF2048_POOL] = ci_create_pool_allocator(2048+PTR_OFFSET);
    Pools[BUF4096_POOL] = ci_create_pool_allocator(4096+PTR_OFFSET);
    Pools[BUF8192_POOL] = ci_create_pool_allocator(8192+PTR_OFFSET);
    Pools[BUF16384_POOL] = ci_create_pool_allocator(16384+PTR_OFFSET);
    Pools[BUF32768_POOL] = ci_create_pool_allocator(32768+PTR_OFFSET);

    short_buffers[0] = Pools[BUF64_POOL];
    short_buffers[1] = Pools[BUF128_POOL];
    short_buffers[2] = short_buffers[3] = Pools[BUF256_POOL];
    short_buffers[4] = short_buffers[5] =
                           short_buffers[6] = short_buffers[7] = Pools[BUF512_POOL];
    for (i = 8; i < 16; i++)
        short_buffers[i] = Pools[BUF1024_POOL];

    long_buffers[0] = Pools[BUF2048_POOL];
    long_buffers[1] = Pools[BUF4096_POOL];
    long_buffers[2] = long_buffers[3] = Pools[BUF8192_POOL];
    long_buffers[4] = long_buffers[5] =
                          long_buffers[6] = long_buffers[7] = Pools[BUF16384_POOL];
    for (i = 8; i < 16; i++)
        long_buffers[i] = Pools[BUF32768_POOL];

    return 1;
}

static int short_buffer_sizes[16] =  {
    64,
    128,
    256,256,
    512, 512, 512, 512,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
};

static int long_buffer_sizes[16] =  {
    2048,
    4096,
    8192, 8192,
    16384, 16384, 16384, 16384,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768
};

void ci_buffers_destroy()
{
    int i;
    for (i = 0; i < BUF_END_POOL; i++) {
        if (Pools[i] != NULL)
            ci_mem_allocator_destroy(Pools[i]);
    }
    memset(Pools, 0, sizeof(Pools));
    memset(short_buffers, 0, sizeof(short_buffers));
    memset(long_buffers, 0, sizeof(long_buffers));
}

void *ci_buffer_alloc2(size_t block_size, size_t *allocated_size)
{
    int type;
    size_t mem_size, allocated_buffer_size;
    struct mem_buffer_block *block = NULL;
    mem_size = block_size + PTR_OFFSET;
    type = (block_size-1) >> 6;
    if (type < 16) {
        assert(short_buffers[type] != NULL);
        block = short_buffers[type]->alloc(short_buffers[type], mem_size);
        allocated_buffer_size = short_buffer_sizes[type];
    } else if (type < 512) {
        int long_sub_type = type >> 5;
        assert(long_buffers[long_sub_type] != NULL);
        block = long_buffers[long_sub_type]->alloc(long_buffers[long_sub_type], mem_size);
        allocated_buffer_size = long_buffer_sizes[long_sub_type];
    } else {
        block = (struct mem_buffer_block *)malloc(mem_size);
        allocated_buffer_size = block_size;
    }

    if (!block) {
        ci_debug_printf(1, "Failed to allocate space for buffer of size:%d\n", (int)block_size);
        return NULL;
    }

    block->sig = BUF_SIGNATURE;
    if (allocated_size) {
        *allocated_size = allocated_buffer_size;
        block->ID = allocated_buffer_size;
    } else
        block->ID = block_size;
    ci_debug_printf(9, "Requested size %d, getting buffer %p from pool %d:%d\n", (int)block_size, (void *)block->data.ptr, type, (int)allocated_buffer_size);
    return (void *)block->data.ptr;
}

void *ci_buffer_alloc(size_t block_size)
{
    return ci_buffer_alloc2(block_size, NULL);
}

static struct mem_buffer_block *to_block(const void *data)
{
    struct mem_buffer_block *block;
    block = (struct mem_buffer_block *)(data-PTR_OFFSET);
    if (block->sig != BUF_SIGNATURE) {
        ci_debug_printf(1,"ci_buffer internal check: ERROR, %p is not a ci_buffer object. This is a bug!!!!\n", data);
        return NULL;
    }
    return block;
}

CI_DECLARE_FUNC(int)  ci_buffer_check(const void *data)
{
    return to_block(data) ? 1 : 0;
}

CI_DECLARE_FUNC(size_t)  ci_buffer_size(const void *data)
{
    const struct mem_buffer_block *block = to_block(data);
    return block ? block->ID : 0;
}

static size_t ci_buffer_real_size(const void *data)
{
    const struct mem_buffer_block *block = to_block(data);
    if (!block)
        return 0;

    int type;
    size_t buffer_block_size = 0;
    type = (block->ID - 1) >> 6;
    if (type < 16) {
        assert(short_buffers[type] != NULL);
        buffer_block_size = short_buffer_sizes[type];
    } else if (type < 512) {
        type = type >> 5;
        assert(long_buffers[type] != NULL);
        buffer_block_size = long_buffer_sizes[type];
    } else
        buffer_block_size = block->ID;
    return buffer_block_size;
}

void *  ci_buffer_realloc2(void *data, size_t new_block_size, size_t *allocated_size)
{
    if (!data)
        return ci_buffer_alloc2(new_block_size, allocated_size);

    size_t current_buffer_size = 0;
    struct mem_buffer_block *block;

    if (!(block = to_block(data)))
        return NULL;

    current_buffer_size = ci_buffer_real_size(data);
    assert(current_buffer_size > 0);
    ci_debug_printf(9, "Current buffer %p of size for realloc: %d, requested block size: %d. The initial size: %d\n",
                    data,
                    (int)current_buffer_size, (int)new_block_size, (int)block->ID);

    /*If no block_size created than our buffer actual size probably requires a realloc.....*/
    if (new_block_size > current_buffer_size) {
        data = ci_buffer_alloc2(new_block_size, allocated_size);
        if (!data)
            return NULL;
        memcpy(data, block->data.ptr, block->ID);
        ci_buffer_free(block->data.ptr);
    } else {
        /*we neeed to update block->ID to the new requested size...*/
        if (allocated_size) {
            *allocated_size = current_buffer_size;
            block->ID = current_buffer_size;
        } else
            block->ID = new_block_size;
    }

    ci_debug_printf(9, "New memory buffer %p of size %d, actual reserved buffer of size: %d\n", data, (int) new_block_size, (int)ci_buffer_real_size(data));

    return data;
}

void * ci_buffer_realloc(void *data, size_t block_size)
{
    return ci_buffer_realloc2(data, block_size, NULL);
}

void ci_buffer_free(void *data)
{
    int type;
    size_t block_size;
    struct mem_buffer_block *block;

    if (!data)
        return;

    if (!(block = to_block(data)))
        return;

    block_size = block->ID;
    type = (block_size-1) >> 6;
    if (type < 16) {
        assert(short_buffers[type] != NULL);
        short_buffers[type]->free(short_buffers[type], block);
        ci_debug_printf(9, "Store buffer %p (used %d bytes) to short pool %d:%d\n", data, (int)block_size, type, short_buffer_sizes[type]);
    } else if (type < 512) {
        int long_sub_type = type >> 5;
        assert(long_buffers[long_sub_type] != NULL);
        long_buffers[long_sub_type]->free(long_buffers[long_sub_type], block);
        ci_debug_printf(9, "Store buffer %p (used %d bytes) to long pool %d:%d\n", data, (int)block_size, type, long_buffer_sizes[long_sub_type]);
    } else {
        ci_debug_printf(9, "Free buffer %p (free at %p, used %d bytes)\n", data, block, (int)block->ID);
        free(block);
    }
}

/*******************************************************************/
/*Object pools                                                     */
#define OBJ_SIGNATURE 0x55AA
ci_mem_allocator_t **object_pools = NULL;
int object_pools_size = 0;
int object_pools_used = 0;

int ci_object_pools_init()
{
    return 1;
}

void ci_object_pools_destroy()
{
    int i;
    for (i = 0; i < object_pools_used; i++) {
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
    if (object_pools == NULL) {
        object_pools = malloc(STEP*sizeof(ci_mem_allocator_t *));
        object_pools_size = STEP;
        ID = 0;
    } else {
        for (i = 0; i < object_pools_used; i++)  {
            if (object_pools[i] == NULL) {
                ID = i;
                break;
            }
        }
        if (ID == -1) {
            if (object_pools_size == object_pools_used) {
                object_pools_size += STEP;
                object_pools = realloc(object_pools, object_pools_size*sizeof(ci_mem_allocator_t *));
            }
            ID=object_pools_used;
        }
    }
    if (object_pools == NULL) //??????
        return -1;

    object_pools[ID] = ci_create_pool_allocator(size+PTR_OFFSET);

    object_pools_used++;
    return ID;
}

void ci_object_pool_unregister(int id)
{
    if (id >= object_pools_used || id < 0) {
        /*A error message ....*/
        return;
    }
    if (object_pools[id]) {
        ci_mem_allocator_destroy(object_pools[id]);
        object_pools[id] = NULL;
    }

}

void *ci_object_pool_alloc(int id)
{
    struct mem_buffer_block *block = NULL;
    if (id >= object_pools_used || id < 0 || !object_pools[id]) {
        /*A error message ....*/
        ci_debug_printf(1, "Invalid object pool %d. This is a BUG!\n", id);
        return NULL;
    }
    block = object_pools[id]->alloc(object_pools[id], 1/*A small size smaller than obj size*/);
    if (!block) {
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
        ci_debug_printf(1,"ci_object_pool_free: ERROR, %p is not internal buffer. This is a bug!!!!\n", ptr);
        return;
    }
    if (block->ID > object_pools_used || block->ID < 0 || !object_pools[block->ID]) {
        ci_debug_printf(1,"ci_object_pool_free: ERROR, %p is pointing to corrupted mem? This is a bug!!!!\n", ptr);
        return;
    }
    ci_debug_printf(8, "Storing to objects pool object %d\n", (int)block->ID);
    object_pools[block->ID]->free(object_pools[block->ID], block);
}

/*******************************************************************/
/*A simple allocator implementation which uses the system malloc    */

static void *os_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
    return malloc(size);
}

static void os_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    free(p);
}

static void os_allocator_reset(ci_mem_allocator_t *allocator)
{
    /*nothing to do*/
}

static void os_allocator_destroy(ci_mem_allocator_t *allocator)
{
    /*nothing to do*/
}

ci_mem_allocator_t *ci_create_os_allocator()
{
    ci_mem_allocator_t *allocator = alloc_mem_allocator_struct();
    if (!allocator)
        return NULL;
    allocator->alloc = os_allocator_alloc;
    allocator->free = os_allocator_free;
    allocator->reset = os_allocator_reset;
    allocator->destroy = os_allocator_destroy;
    allocator->data = NULL;
    allocator->name = NULL;
    allocator->type = OS_ALLOC;
    return allocator;
}



/************************************************************/
/* The serial allocator implementation                      */


typedef struct serial_allocator {
    void *memchunk;
    void *curpos;
    void *endpos;
    struct serial_allocator *next;
} serial_allocator_t;


static serial_allocator_t *serial_allocator_build(size_t size)
{
    serial_allocator_t *serial_alloc;
    void *buffer;
    size = _CI_ALIGN(size);
    /*The serial_allocator and mem_allocator structures will be
     allocated in the buffer */
    if (size < sizeof(serial_allocator_t) + sizeof(ci_mem_allocator_t))
        return NULL;

    /*The allocated block size maybe is larger, than the requested.
      Fix size to actual block size */
    buffer = ci_buffer_alloc2(size, &size);
    serial_alloc = buffer;

    serial_alloc->memchunk = buffer + sizeof(serial_allocator_t);
    size -= sizeof(serial_allocator_t);
    serial_alloc->curpos = serial_alloc->memchunk;
    serial_alloc->endpos = serial_alloc->memchunk + size;
    serial_alloc->next = NULL;
    return serial_alloc;
}

static void *serial_allocation(serial_allocator_t *serial_alloc, size_t size)
{
    size_t max_size;
    void *mem;
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

static void *serial_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
    serial_allocator_t *serial_alloc = (serial_allocator_t *)allocator->data;

    if (!serial_alloc)
        return NULL;
    return serial_allocation(serial_alloc, size);
}

static void serial_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    /* We can not free :-)  */
}

static void serial_allocator_reset(ci_mem_allocator_t *allocator)
{
    serial_allocator_t *serial_alloc, *sa;
    void *tmp;
    serial_alloc = (serial_allocator_t *)allocator->data;
    serial_alloc->curpos = serial_alloc->memchunk + _CI_ALIGN(sizeof(ci_mem_allocator_t));
    sa = serial_alloc->next;
    serial_alloc->next = NULL;

    /*release any other allocated chunk*/
    while (sa) {
        tmp = (void *)sa;
        ci_buffer_free(tmp);
        sa = sa->next;
    }
}

static void serial_allocator_destroy(ci_mem_allocator_t *allocator)
{
    serial_allocator_t *cur, *next;

    if (!allocator->data)
        return;

    cur = (serial_allocator_t *)allocator->data;
    next = cur->next;
    while (cur) {
        ci_buffer_free((void *)cur);
        cur = next;
        if (next)
            next = next->next;
    }
}

ci_mem_allocator_t *ci_create_serial_allocator(size_t size)
{
    ci_mem_allocator_t *allocator;

    serial_allocator_t *sdata= serial_allocator_build(size);

    /*Allocate space for ci_mem_allocator_t from our serial allocator ...*/
    allocator = serial_allocation(sdata, sizeof(ci_mem_allocator_t));
    if (!allocator) {
        ci_buffer_free((void *)sdata);
        return NULL;
    }
    allocator->alloc = serial_allocator_alloc;
    allocator->free = serial_allocator_free;
    allocator->reset = serial_allocator_reset;
    allocator->destroy = serial_allocator_destroy;
    allocator->data = sdata;
    allocator->name = NULL;
    allocator->type = SERIAL_ALLOC;
    /*It is allocated in our buffer space...*/
    allocator->must_free = 0;
    return allocator;
}
/****************************************************************/


typedef struct pack_allocator {
    void *memchunk;
    void *curpos;
    void *endpos;
    void *end;
    int must_free;
} pack_allocator_t;

/*Api functions for pack allocator:*/
void *ci_pack_allocator_alloc_unaligned(ci_mem_allocator_t *allocator, size_t size)
{
    int max_size;
    void *mem;
    pack_allocator_t *pack_alloc;

    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;

    if (!pack_alloc)
        return NULL;

    max_size = pack_alloc->endpos - pack_alloc->curpos;

    if (size > max_size)
        return NULL;

    mem = pack_alloc->curpos;
    pack_alloc->curpos += size;
    return mem;
}

void *ci_pack_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
    size = _CI_ALIGN(size); /*round size to a correct alignment size*/
    return ci_pack_allocator_alloc_unaligned(allocator, size);
}

void  *ci_pack_allocator_alloc_from_rear(ci_mem_allocator_t *allocator, int size)
{
    int max_size;
    void *mem;
    pack_allocator_t *pack_alloc;

    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;

    if (!pack_alloc)
        return NULL;

    size = _CI_ALIGN(size); /*round size to a correct alignment size*/
    max_size = pack_alloc->endpos - pack_alloc->curpos;

    if (size > max_size)
        return NULL;

    pack_alloc->endpos -= size; /*Allocate block from the end of memory block*/
    mem = pack_alloc->endpos;
    return mem;
}

void ci_pack_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    /* We can not free :-)  */
}

void ci_pack_allocator_reset(ci_mem_allocator_t *allocator)
{
    pack_allocator_t *pack_alloc;
    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;
    pack_alloc->curpos = pack_alloc->memchunk;
    pack_alloc->endpos = pack_alloc->end;
}

void ci_pack_allocator_destroy(ci_mem_allocator_t *allocator)
{
    pack_allocator_t *pack_alloc;
    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;
    if (pack_alloc->must_free != 0) {
        ci_object_pool_free(allocator->data);
        allocator->data = NULL;
    }
}

/*If "off" is not aligned return the first smaller aligned offset*/
#define _ALIGNED_OFFSET(off) (off != _CI_ALIGN(off) ? _CI_ALIGN(off - _CI_NBYTES_ALIGNMENT) : off)

ci_mem_allocator_t *init_pack_allocator(ci_mem_allocator_t *allocator, pack_allocator_t *pack_alloc, char *memblock, size_t size, int free)
{
    /*We may not be able to use all of the memblock size.
      We need to support allocating memory space from the end, so we
      need to have aligned the pack_alloc->end to correctly calculate
      memory block offsets from the end in ci_pack_allocator_alloc_from_rear
      function.
    */
    size =  _ALIGNED_OFFSET(size);
    pack_alloc->memchunk = memblock;
    pack_alloc->curpos =pack_alloc->memchunk;
    pack_alloc->end = pack_alloc->memchunk + size;
    pack_alloc->endpos = pack_alloc->end;
    pack_alloc->must_free = free;

    allocator->alloc = ci_pack_allocator_alloc;
    allocator->free = ci_pack_allocator_free;
    allocator->reset = ci_pack_allocator_reset;
    allocator->destroy = ci_pack_allocator_destroy;
    allocator->data = pack_alloc;
    allocator->name = NULL;
    allocator->type = PACK_ALLOC;
    allocator->must_free = free;
    return allocator;
}

ci_mem_allocator_t *ci_create_pack_allocator(char *memblock, size_t size)
{
    ci_mem_allocator_t *allocator;
    pack_allocator_t *pack_alloc;
    pack_alloc = ci_object_pool_alloc(PACK_ALLOCATOR_POOL);
    if (!pack_alloc)
        return NULL;
    allocator = alloc_mem_allocator_struct();
    if (!allocator) {
        ci_object_pool_free(pack_alloc);
        return NULL;
    }

    return   init_pack_allocator(allocator, pack_alloc, memblock, size, 2);
}

/*similar to the above but allocates required space for pack_allocator on the given memblock*/
ci_mem_allocator_t *ci_create_pack_allocator_on_memblock(char *memblock, size_t size)
{
    ci_mem_allocator_t *allocator;

    /*We need to allocate space on memblock for internal structures*/
    if (size <= (_CI_ALIGN(sizeof(pack_allocator_t)) + _CI_ALIGN(sizeof(ci_mem_allocator_t))))
        return NULL;

    pack_allocator_t *pack_alloc = (pack_allocator_t *)memblock;
    memblock += _CI_ALIGN(sizeof(pack_allocator_t));
    size -= _CI_ALIGN(sizeof(pack_allocator_t));
    allocator = (ci_mem_allocator_t *)memblock;
    memblock += _CI_ALIGN(sizeof(ci_mem_allocator_t));
    size -= _CI_ALIGN(sizeof(ci_mem_allocator_t));

    return   init_pack_allocator(allocator, pack_alloc, memblock, size, 0);
}

int ci_pack_allocator_data_size(ci_mem_allocator_t *allocator)
{
    assert(allocator->type == PACK_ALLOC);
    pack_allocator_t *pack_alloc = (pack_allocator_t *)allocator->data;
    return (int) (pack_alloc->curpos - pack_alloc->memchunk) +
           (pack_alloc->end - pack_alloc->endpos);
}

size_t  ci_pack_allocator_required_size()
{
    return _CI_ALIGN(sizeof(pack_allocator_t)) + _CI_ALIGN(sizeof(ci_mem_allocator_t));
}

static size_t sizeof_pack_allocator() {return sizeof(pack_allocator_t);}

void ci_pack_allocator_set_start_pos(ci_mem_allocator_t *allocator, void *p)
{
    pack_allocator_t *pack_alloc;
    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;
    assert(p >= pack_alloc->memchunk);
    pack_alloc->curpos = p;
}

void ci_pack_allocator_set_end_pos(ci_mem_allocator_t *allocator, void *p)
{
    pack_allocator_t *pack_alloc;
    assert(allocator->type == PACK_ALLOC);
    pack_alloc = (pack_allocator_t *)allocator->data;
    assert(p <= pack_alloc->end);
    if (p == NULL)
        pack_alloc->endpos = pack_alloc->end;
    else
        pack_alloc->endpos = p;
}

/****************************************************************/

struct mem_block_item {
    void *data;
    struct mem_block_item *next;
};

struct pool_allocator {
    int items_size;
    int strict;
    int alloc_count;
    int hits_count;
    ci_thread_mutex_t mutex;
    struct mem_block_item *free;
    struct mem_block_item *allocated;
};

static struct pool_allocator *pool_allocator_build(int items_size,
        int strict)
{
    struct pool_allocator *palloc;

    palloc = (struct pool_allocator *)malloc(sizeof(struct pool_allocator));

    if (!palloc) {
        return NULL;
    }

    palloc->items_size = items_size;
    palloc->strict = strict;
    palloc->free = NULL;
    palloc->allocated = NULL;
    palloc->alloc_count = 0;
    palloc->hits_count = 0;
    ci_thread_mutex_init(&palloc->mutex);
    return palloc;
}

static void *pool_allocator_alloc(ci_mem_allocator_t *allocator,size_t size)
{
    struct mem_block_item *mem_item;
    void *data = NULL;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;

    if (size > palloc->items_size)
        return NULL;

    ci_thread_mutex_lock(&palloc->mutex);

    if (palloc->free) {
        mem_item = palloc->free;
        palloc->free=palloc->free->next;
        data = mem_item->data;
        mem_item->data = NULL;
        palloc->hits_count++;
    } else {
        mem_item = malloc(sizeof(struct mem_block_item));
        mem_item->data = NULL;
        data = malloc(palloc->items_size);
        palloc->alloc_count++;
    }

    mem_item->next = palloc->allocated;
    palloc->allocated = mem_item;

    ci_thread_mutex_unlock(&palloc->mutex);
    ci_debug_printf(8, "pool hits: %d allocations: %d\n", palloc->hits_count, palloc->alloc_count);
    return data;
}

static void pool_allocator_free(ci_mem_allocator_t *allocator,void *p)
{
    struct mem_block_item *mem_item;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;

    ci_thread_mutex_lock(&palloc->mutex);
    if (!palloc->allocated) {
        /*Yes can happen! after a reset but users did not free all objects*/
        free(p);
    } else {
        mem_item = palloc->allocated;
        palloc->allocated = palloc->allocated->next;

        mem_item->data = p;
        mem_item->next = palloc->free;
        palloc->free = mem_item;
    }
    ci_thread_mutex_unlock(&palloc->mutex);
}

static void pool_allocator_reset(ci_mem_allocator_t *allocator)
{
    struct mem_block_item *mem_item, *cur;
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;

    ci_thread_mutex_lock(&palloc->mutex);
    if (palloc->allocated) {
        mem_item = palloc->allocated;
        while (mem_item != NULL) {
            cur = mem_item;
            mem_item = mem_item->next;
            free(cur);
        }

    }
    palloc->allocated = NULL;
    if (palloc->free) {
        mem_item = palloc->free;
        while (mem_item != NULL) {
            cur = mem_item;
            mem_item = mem_item->next;
            free(cur->data);
            free(cur);
        }
    }
    palloc->free = NULL;
    ci_thread_mutex_unlock(&palloc->mutex);
}


static void pool_allocator_destroy(ci_mem_allocator_t *allocator)
{
    pool_allocator_reset(allocator);
    struct pool_allocator *palloc = (struct pool_allocator *)allocator->data;
    ci_thread_mutex_destroy(&palloc->mutex);
    free(palloc);
}

ci_mem_allocator_t *ci_create_pool_allocator(int items_size)
{
    struct pool_allocator *palloc;
    ci_mem_allocator_t *allocator;

    palloc = pool_allocator_build(items_size, 0);
    /*Use always malloc for ci_mem_alocator struct.*/
    allocator = (ci_mem_allocator_t *) malloc(sizeof(ci_mem_allocator_t));
    if (!allocator)
        return NULL;
    allocator->alloc = pool_allocator_alloc;
    allocator->free = pool_allocator_free;
    allocator->reset = pool_allocator_reset;
    allocator->destroy = pool_allocator_destroy;
    allocator->data = palloc;
    allocator->name = NULL;
    allocator->type = POOL_ALLOC;
    allocator->must_free = 1;
    return allocator;
}
