/*
 *  Copyright (C) 2011 Christos Tsantilas
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
#include "debug.h"
#include "mem.h"
#include "array.h"
#include <assert.h>

#define array_item_size(type) ( (size_t)&((type *)0)[1])

ci_array_t * ci_array_new(size_t size)
{
    ci_array_t *array;
    ci_mem_allocator_t *packer;
    void  *buffer;

    buffer = ci_buffer_alloc(size);
    if (!buffer)
        return NULL;

    packer = ci_create_pack_allocator_on_memblock(buffer, size);
    if (!packer) {
        ci_buffer_free(buffer);
        return NULL;
    }

    array = ci_pack_allocator_alloc(packer, sizeof(ci_array_t));
    if (!array) {
        ci_buffer_free(buffer);
        ci_mem_allocator_destroy(packer);
        return NULL;
    }

    array->max_size = size;
    array->count = 0;
    array->items = NULL;
    array->mem = buffer;
    array->alloc = packer;
    return array;
}

ci_array_t * ci_array_new2(size_t items, size_t item_size)
{
    size_t array_size;
    array_size = ci_pack_allocator_required_size() +
        _CI_ALIGN(sizeof(ci_array_t)) +
        items * (_CI_ALIGN(item_size) + _CI_ALIGN(sizeof(ci_array_item_t)));
    return ci_array_new(array_size);
}

void ci_array_destroy(ci_array_t *array)
{
    void *buffer = array->mem;
    assert(buffer);
    if (array->alloc)
        ci_mem_allocator_destroy(array->alloc);
    ci_buffer_free(buffer);
}

const ci_array_item_t * ci_array_add(ci_array_t *array, const char *name, const void *value, size_t size)
{
    ci_array_item_t *item;
    ci_mem_allocator_t *packer = array->alloc;
    assert(packer);
    item = ci_pack_allocator_alloc_unaligned(packer, array_item_size(ci_array_item_t));
    if (item) {
        item->name =  ci_pack_allocator_alloc_from_rear(packer, strlen(name) + 1);
        item->value =  ci_pack_allocator_alloc_from_rear(packer, size);
    }

    if (!item || !item->name || !item->value) {
        ci_debug_printf(2, "Not enough space to add the new item to array!\n");
        return NULL;
    }

    strcpy(item->name, name);
    memcpy(item->value, value, size);

    /*The array->items should point to the first item...*/
    if (array->items == NULL)
        array->items = item;

    array->count++;
    return item;
}

const void * ci_array_search(ci_array_t *array, const char *name)
{
    int i;
    for (i=0; i < array->count; i++) {
        if (strcmp(array->items[i].name, name) == 0) {
            return array->items[i].value;
        }
    }
    return NULL;
}

void ci_array_iterate(ci_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *))
{
    int i, ret = 0;
    for (i=0; i < array->count && ret == 0; i++) {
        ret = (*fn)(data, array->items[i].name, array->items[i].value);
    }
}

#define MIN_P(p1, p2) ((void *)p1 < (void *)p2 ? (void *)p1 : (void *)p2)
const ci_array_item_t *ci_array_pop(ci_array_t *array)
{
    ci_array_item_t *item;
    if (array->count == 0)
        return NULL;
    /*Delete the last element*/
    item = &array->items[array->count-1];
    ci_pack_allocator_set_start_pos(array->alloc, item);

    /*Delete the content of the last element*/
    array->count--;
    if (array->count == 0)
        ci_pack_allocator_set_end_pos(array->alloc, NULL);
    else
        ci_pack_allocator_set_end_pos(array->alloc, MIN_P(array->items[array->count-1].name, array->items[array->count-1].value) );

    return item;
}

const ci_array_item_t * ci_ptr_array_add(ci_ptr_array_t *ptr_array, const char *name, void *value)
{
    ci_array_item_t *item;
    ci_mem_allocator_t *packer = ptr_array->alloc;
    assert(packer);
    item = ci_pack_allocator_alloc_unaligned(packer, array_item_size(ci_array_item_t));
    if (item)
        item->name =  ci_pack_allocator_alloc_from_rear(packer, strlen(name) + 1);

    if (!item || !item->name) {
        ci_debug_printf(2, "Not enough space to add the new item to array!\n");
        return NULL;
    }
    strcpy(item->name, name);
    item->value = value;

    /*The array->items should point to the first item...*/
    if (ptr_array->items == NULL)
        ptr_array->items = item;

    ptr_array->count++;
    return item;
}

const ci_array_item_t *ci_ptr_array_pop(ci_ptr_array_t *ptr_array)
{
    ci_array_item_t *item;
    if (ptr_array->count == 0)
        return NULL;
    item = &ptr_array->items[ptr_array->count-1];
    ci_pack_allocator_set_start_pos(ptr_array->alloc, item);
    ptr_array->count--;
    return item;
}

void * ci_ptr_array_pop_value(ci_ptr_array_t *ptr_array, char *name, size_t name_size)
{
    const ci_array_item_t *item = ci_ptr_array_pop(ptr_array);
    if (!item)
        return NULL;
    strncpy(name, item->name, name_size);
    name[name_size-1] = '\0';
    return item->value;
}

/**************************************************************************/

ci_dyn_array_t * ci_dyn_array_new(size_t size)
{
    ci_dyn_array_t *array;
    ci_mem_allocator_t *packer;

    packer = ci_create_serial_allocator(size);
    if (!packer) {
        return NULL;
    }

    array = packer->alloc(packer, sizeof(ci_dyn_array_t));
    if (!array) {
        ci_mem_allocator_destroy(packer);
        return NULL;
    }

    array->max_size = size;
    array->items = NULL;
    array->last = NULL;
    array->alloc = packer;
    return array;
}

ci_dyn_array_t * ci_dyn_array_new2(size_t items, size_t item_size)
{
    size_t array_size;
    array_size = _CI_ALIGN(sizeof(ci_dyn_array_t)) +
        items * (_CI_ALIGN(item_size) + _CI_ALIGN(sizeof(ci_dyn_array_item_t)));
    return ci_dyn_array_new(array_size);
}

void ci_dyn_array_destroy(ci_dyn_array_t *array)
{
    if (array->alloc)
        ci_mem_allocator_destroy(array->alloc);
}

const ci_dyn_array_item_t * ci_dyn_array_add(ci_dyn_array_t *array, const char *name, const void *value, size_t size)
{
    ci_dyn_array_item_t *item;
    ci_mem_allocator_t *packer = array->alloc;
    int name_size;
    assert(packer);
    item = packer->alloc(packer, sizeof(ci_dyn_array_item_t));
    if (!item) {
        ci_debug_printf(2, "Not enough space to add the new item %s to array!\n", name);
        return NULL;
    }
    item->next = NULL;
    name_size = strlen(name) + 1;
    item->name = packer->alloc(packer, name_size);
    if (size > 0)
        item->value = packer->alloc(packer, size);
    else
        item->value = NULL;

    if (!item->name || (!item->value && size > 0)) {
        ci_debug_printf(2, "Not enough space to add the new item %s to array!\n", name);
        /*packer->free does not realy free anything bug maybe in the future will be able to release memory*/
        if (item->name) packer->free(packer, item->name);
        if (item->value) packer->free(packer, item->value);
        packer->free(packer, item);
        return NULL;
    }

    /*copy values*/
    memcpy(item->name, name, name_size);
    if (size > 0)
        memcpy(item->value, value, size);
    else
        item->value = (void *)value;

    if (array->items == NULL) {
        array->items = item;
        array->last = array->items;
    }
    else {
        assert(array->last);
        array->last->next = item;
        array->last = array->last->next; 
    }

    return item;
}

const void * ci_dyn_array_search(ci_dyn_array_t *array, const char *name)
{
    ci_dyn_array_item_t *item;
    for (item = array->items; item != NULL; item = item->next)
        if (strcmp(item->name, name) == 0)
            return item->value;

    /*did not found anything*/
    return NULL;
}

void ci_dyn_array_iterate(ci_dyn_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *value))
{
    ci_dyn_array_item_t *item;
    int i, ret = 0;
    for (i=0, item = array->items; item != NULL && ret == 0; item = item->next, i++)
        ret = (*fn)(data, item->name, item->value);
}

const ci_dyn_array_item_t * ci_ptr_dyn_array_add(ci_ptr_dyn_array_t *array, const char *name, void *value)
{
   return ci_dyn_array_add(array, name, value, 0);
}

/*This function should removed.*/
void *ci_ptr_dyn_array_pop_head(ci_ptr_dyn_array_t *array, char *name, size_t name_size)
{
    ci_dyn_array_item_t *item;
    void *value;
    item = array->items;
    if (!item)
        return NULL;

    array->items = array->items->next;
    if (array->items == NULL)
        array->last = NULL;

    value = item->value;
    if (name && name_size > 0) {
        strncpy(name, item->name, name_size);
        name[name_size-1] = '\0';
    }
    /*Does not really needed*/
    array->alloc->free(array->alloc, item->name);
    array->alloc->free(array->alloc, item);

    return value;
}

/**************/
/* Vectors API */

ci_vector_t * ci_vector_create(size_t max_size)
{
    ci_vector_t *vector;
    ci_mem_allocator_t *packer;
    void  *buffer;
    void **indx;

    buffer = ci_buffer_alloc(max_size);
    if (!buffer)
        return NULL;

    packer = ci_create_pack_allocator_on_memblock(buffer, max_size);
    if (!packer) {
        ci_buffer_free(buffer);
        return NULL;
    }

    vector = ci_pack_allocator_alloc(packer, sizeof(ci_vector_t));
    /*Allocate mem for the first item which points to NULL. Vectors are NULL terminated*/
    indx = ci_pack_allocator_alloc_unaligned(packer, array_item_size(void *));
    if (!vector || ! indx) {
        ci_buffer_free(buffer);
        ci_mem_allocator_destroy(packer);
        return NULL;
    }
    *indx = NULL;
    
    vector->max_size = max_size;
    vector->mem = buffer;
    vector->items = indx;
    vector->last = indx;
    vector->count = 0;
    vector->alloc = packer;
    return vector;

}

const void **ci_vector_cast_to_voidvoid(ci_vector_t *vector)
{
    return (const void **)vector->items;
}

ci_vector_t *ci_vector_cast_from_voidvoid(const void **p)
{
    const void *buf;
    ci_vector_t *v;
    v = (ci_vector_t *)((void *)p - _CI_ALIGN(sizeof(ci_vector_t)));
    buf = (void *)v - ci_pack_allocator_required_size();
    /*Check if it is a valid vector. The ci_buffer_blocksize will return 0, if buf  is not a ci_buffer object*/
    assert(v->mem == buf);
    assert(ci_buffer_blocksize(buf) != 0);
    return v;
}

void ci_vector_destroy(ci_vector_t *vector)
{
    void *buffer = vector->mem;
    assert(buffer);
    if (vector->alloc)
        ci_mem_allocator_destroy(vector->alloc);
    ci_buffer_free(buffer);
}

void * ci_vector_add(ci_vector_t *vector, void *value, size_t size)
{
    void *item, **indx;
    ci_mem_allocator_t *packer = vector->alloc;
    assert(packer);
    indx = ci_pack_allocator_alloc_unaligned(packer, array_item_size(void *));
    item = ci_pack_allocator_alloc_from_rear(packer, size);
    if (!item || !indx) {
        ci_debug_printf(2, "Not enough space to add the new item to vector!\n");
        return NULL;
    }

    memcpy(item, value, size);
    *(vector->last) = item;
    vector->last = indx;
    *(vector->last) = NULL;
    vector->count++;
    return item;
}

void * ci_vector_pop(ci_vector_t *vector)
{
    void *p;
    if (vector->count == 0)
        return NULL;

    /*Delete the last NULL element*/
    ci_pack_allocator_set_start_pos(vector->alloc, vector->last);

    /*Set last to the preview ellement*/
    vector->count--;
    vector->last = &vector->items[vector->count]; 

    /*Erase the content of last element*/
    if (vector->count == 0)
        ci_pack_allocator_set_end_pos(vector->alloc, NULL);
    else
        ci_pack_allocator_set_end_pos(vector->alloc, vector->items[vector->count-1]);

    /*The last element must point to NULL*/
    p = *(vector->last);
    *(vector->last) = NULL;
    return p;
}

void ci_vector_iterate(ci_vector_t *vector, void *data, int (*fn)(void *data, const void *))
{
    int i, ret = 0;
    for (i=0; vector->items[i] != NULL && ret == 0; i++)
        ret = (*fn)(data, vector->items[i]);
}


/*ci_str_vector functions */
void ci_str_vector_iterate(ci_str_vector_t *vector, void *data, int (*fn)(void *data, const char *))
{
    ci_vector_iterate(vector, data, (int(*)(void *, const void *))fn);
}

const char * ci_str_vector_search(ci_str_vector_t *vector, const char *item)
{
    int i;
    for (i=0; vector->items[i] != NULL; i++) {
        if (strcmp(vector->items[i], item) == 0)
            return vector->items[i];
    }

    return NULL;
}


/*ci_ptr_vector functions....*/
void * ci_ptr_vector_add(ci_vector_t *vector, void *value)
{
    void **indx;
    ci_mem_allocator_t *packer = vector->alloc;
    assert(packer);

    if (!value)
        return NULL;

    indx = ci_pack_allocator_alloc_unaligned(packer, array_item_size(void *));
    if (!indx) {
        ci_debug_printf(2, "Not enough space to add the new item to ptr_vector!\n");
        return NULL;
    }
    /*Store the pointer to the last ellement */
    *(vector->last) = value;

    /*And create a new NULL terminated item: */
    vector->last = indx;
    *(vector->last) = NULL;
    return value;
}

