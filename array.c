/*
 *  Copyright (C) 20011 Christos Tsantilas
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

ci_array_t * ci_array_new(size_t size)
{
    ci_array_t *array;
    ci_mem_allocator_t *packer;
    void  *buffer;

    buffer = ci_buffer_alloc(size);
    if (!buffer)
        return NULL;

    packer = ci_create_pack_allocator(buffer, size);
    if (!packer) {
        ci_buffer_free(buffer);
        return NULL;
    }

    array = packer->alloc(packer, sizeof(ci_array_t));
    if (!array) {
        ci_buffer_free(buffer);
        ci_mem_allocator_destroy(packer);
        return NULL;
    }

    array->max_size = size;
    array->items = NULL;
    array->last = NULL;
    array->mem = buffer;
    array->alloc = packer;
    return array;
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
    int name_size;
    assert(packer);
    item = packer->alloc(packer, sizeof(ci_array_item_t));
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

const void * ci_array_search(ci_array_t *array, const char *name)
{
    ci_array_item_t *item;
    for (item = array->items; item != NULL; item = item->next)
        if (strcmp(item->name, name) == 0)
            return item->value;

    /*did not found anything*/
    return NULL;
}

void ci_array_iterate(ci_array_t *array, void *data, void (*fn)(void *data, const char *name, const void *value))
{
    ci_array_item_t *item;
    int i;
    for (i=0, item = array->items; item != NULL; item = item->next, i++)
        (*fn)(data, item->name, item->value);
}

const ci_array_item_t * ci_ptr_array_add(ci_ptr_array_t *array, const char *name, void *value)
{
   return ci_array_add(array, name, value, 0);
}

void *ci_ptr_array_pop(ci_ptr_array_t *array, char *name, size_t name_size)
{
    ci_array_item_t *item;
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

