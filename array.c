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
    array->flags = 0;
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

ci_array_t  *ci_array_rebuild(ci_array_t *old)
{
    ci_array_t *array;
    ci_mem_allocator_t *packer = old->alloc;
    void  *buffer = old->mem;
    size_t size = old->max_size;

    packer->reset(packer);
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

void ci_array_destroy(ci_array_t *array)
{
    void *buffer = array->mem;
    _CI_ASSERT(buffer);
    if (array->alloc)
        ci_mem_allocator_destroy(array->alloc);
    ci_buffer_free(buffer);
}

void *ci_array_value(const ci_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return (pos < array->count ?  array->items[pos].value : NULL);
}

const char *ci_array_name(const ci_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return (pos < array->count ?  array->items[pos].name : NULL);
}

unsigned int ci_array_size(const ci_array_t *array)
{
    _CI_ASSERT(array);
    return array->count;
}

const ci_array_item_t *ci_array_get_item(const ci_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return (pos >= array->count) ? NULL : &(array->items[pos]);
}

const ci_array_item_t * ci_array_add(ci_array_t *array, const char *name, const void *value, size_t size)
{
    ci_array_item_t *item;
    ci_mem_allocator_t *packer = array->alloc;
    _CI_ASSERT(array);
    _CI_ASSERT(array->alloc);
    packer = array->alloc;
    item = ci_pack_allocator_alloc_unaligned(packer, array_item_size(ci_array_item_t));
    size_t name_size = strlen(name) + 1;
    if (item) {
        item->name =  ci_pack_allocator_alloc_from_rear(packer, name_size);
        item->value =  ci_pack_allocator_alloc_from_rear(packer, size);
    }

    if (!item || !item->name || !item->value) {
        ci_debug_printf(2, "Not enough space to add the new item to array!\n");
        return NULL;
    }

    strncpy(item->name, name, name_size);
    item->name[name_size - 1] = '\0';
    memcpy(item->value, value, size);

    /*The array->items should point to the first item...*/
    if (array->items == NULL)
        array->items = item;

    array->count++;
    return item;
}

const void * ci_array_search(const ci_array_t *array, const char *name)
{
    int i;
    _CI_ASSERT(array);
    for (i = 0; i < array->count; i++) {
        if (strcmp(array->items[i].name, name) == 0) {
            return array->items[i].value;
        }
    }
    return NULL;
}

void ci_array_iterate(const ci_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *))
{
    int i, ret = 0;
    _CI_ASSERT(array);
    for (i = 0; i < array->count && ret == 0; i++) {
        ret = (*fn)(data, array->items[i].name, array->items[i].value);
    }
}

#define MIN_P(p1, p2) ((void *)p1 < (void *)p2 ? (void *)p1 : (void *)p2)
const ci_array_item_t *ci_array_pop(ci_array_t *array)
{
    ci_array_item_t *item;
    _CI_ASSERT(array);
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

const ci_array_item_t *ci_str_array_add(ci_str_array_t *array, const char *name, const char *value) {
    return ci_array_add((ci_array_t *)array, name, value, (strlen(value)+1));
}

ci_ptr_array_t * ci_ptr_array_new2(size_t items)
{
    size_t array_size;
    array_size = ci_pack_allocator_required_size() +
                 _CI_ALIGN(sizeof(ci_ptr_array_t)) +
                 items * (_CI_ALIGN(sizeof(void *)) + _CI_ALIGN(sizeof(ci_array_item_t)));
    return ci_ptr_array_new(array_size);
}

void * ci_ptr_array_search(const ci_ptr_array_t *array, const char *name)
{
    /*return a writable object*/
    return (void *)ci_array_search(array, name);
}

const ci_array_item_t * ci_ptr_array_add(ci_ptr_array_t *ptr_array, const char *name, void *value)
{
    ci_array_item_t *item;
    ci_mem_allocator_t *packer;
    _CI_ASSERT(ptr_array);
    _CI_ASSERT(ptr_array->alloc);
    packer = ptr_array->alloc;
    item = ci_pack_allocator_alloc_unaligned(packer, array_item_size(ci_array_item_t));
    size_t name_size = strlen(name) + 1;
    if (item)
        item->name =  ci_pack_allocator_alloc_from_rear(packer, name_size);

    if (!item || !item->name) {
        ci_debug_printf(2, "Not enough space to add the new item to array!\n");
        return NULL;
    }
    strncpy(item->name, name, name_size);
    item->name[name_size - 1] = '\0';
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
    _CI_ASSERT(ptr_array);
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
    /*use 25% of memory for index.*/
    size_t index_memory = size / 4;
    size_t items_memory = size - index_memory;
    size_t items_count = index_memory / sizeof(ci_array_item_t *);
    size_t item_size = items_memory / items_count;
    if (item_size < sizeof(ci_array_item_t))
        item_size = sizeof(ci_array_item_t);

    return ci_dyn_array_new2(items_count, item_size);
}

ci_dyn_array_t * ci_dyn_array_new2(size_t items, size_t item_size)
{
    ci_dyn_array_t *array;
    ci_mem_allocator_t *packer;
    size_t array_size;

    /*
      Items of size = item_size + sizeof(ci_array_item)+sizeof(name)
      for sizeof(name) assume a size of 16 bytes.
      We can not be accurate here....
     */
    array_size = _CI_ALIGN(sizeof(ci_dyn_array_t)) +
                 items * (_CI_ALIGN(item_size) + _CI_ALIGN(sizeof(ci_array_item_t)) + _CI_ALIGN(16));

    packer = ci_create_serial_allocator(array_size);
    if (!packer) {
        return NULL;
    }

    array = packer->alloc(packer, sizeof(ci_dyn_array_t));
    if (!array) {
        ci_mem_allocator_destroy(packer);
        return NULL;
    }

    if (items < 32)
        items = 32;

    array->max_items = items;
    array->items = ci_buffer_alloc(items*sizeof(ci_array_item_t *));
    array->count = 0;
    array->alloc = packer;
    array->flags = 0;
    return array;
}

void ci_dyn_array_destroy(ci_dyn_array_t *array)
{
    _CI_ASSERT(array);
    if (array->items)
        ci_buffer_free(array->items);

    if (array->alloc)
        ci_mem_allocator_destroy(array->alloc);
}

ci_dyn_array_t  * ci_dyn_array_rebuild(ci_dyn_array_t *old)
{
    _CI_ASSERT(old);
    old->count = 0;
    ci_mem_allocator_t *packer;
    _CI_ASSERT(old->alloc);
    packer = old->alloc;
    packer->reset(packer);
    return old;
}

const ci_array_item_t *ci_dyn_array_get_item(const ci_dyn_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return (pos < array->count ? array->items[pos] : NULL);
}

void *ci_dyn_array_value(const ci_dyn_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return ((pos < array->count && array->items[pos] != NULL) ?  array->items[pos]->value : NULL);
}

const char *ci_dyn_array_name(const ci_dyn_array_t *array, unsigned int pos)
{
    _CI_ASSERT(array);
    return ((pos < array->count && array->items[pos] != NULL) ?  array->items[pos]->name : NULL);
}

unsigned int ci_dyn_array_size(const ci_dyn_array_t *array)
{
    _CI_ASSERT(array);
    return array->count;
}


const ci_array_item_t * ci_dyn_array_add(ci_dyn_array_t *array, const char *name, const void *value, size_t size)
{
    ci_array_item_t *item;
    ci_array_item_t **items_space;
    ci_mem_allocator_t *packer;
    int name_size;
    _CI_ASSERT(array);

    if (array->count == array->max_items) {
        items_space = ci_buffer_realloc(array->items, (array->max_items + 32)*sizeof(ci_array_item_t *));
        if (!items_space)
            return NULL;
        array->items = items_space;
        array->max_items += 32;
    }

    _CI_ASSERT(array->alloc);
    packer = array->alloc;
    item = packer->alloc(packer, sizeof(ci_array_item_t));
    if (!item) {
        ci_debug_printf(2, "Not enough space to add the new item %s to array!\n", name);
        return NULL;
    }
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

    array->items[array->count++] = item;
    return item;
}

const void * ci_dyn_array_search(const ci_dyn_array_t *array, const char *name)
{
    int i;
    _CI_ASSERT(array);
    for (i = 0; i < array->count; ++i)
        if (strcmp(array->items[i]->name, name) == 0)
            return array->items[i]->value;

    /*did not found anything*/
    return NULL;
}

void ci_dyn_array_iterate(const ci_dyn_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *value))
{
    int i, ret = 0;
    _CI_ASSERT(array);
    for (i = 0; i < array->count && ret == 0; i++)
        ret = (*fn)(data, array->items[i]->name, array->items[i]->value);
}

const ci_array_item_t * ci_ptr_dyn_array_add(ci_ptr_dyn_array_t *array, const char *name, void *value)
{
    return ci_dyn_array_add(array, name, value, 0);
}

/**************/
/* Vectors API */

typedef enum {
    CI_VECTOR_MEM_ALIGN = 0x0001
} ci_vector_flags_t;

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
    vector->flags = CI_VECTOR_MEM_ALIGN;
    return vector;
}

void ci_vector_set_align(ci_vector_t *vector, int onoff)
{
    _CI_ASSERT(vector);
    if (onoff)
        vector->flags |= CI_VECTOR_MEM_ALIGN;
    else
        vector->flags &= ~CI_VECTOR_MEM_ALIGN;
}

const void **ci_vector_cast_to_voidvoid(ci_vector_t *vector)
{
    _CI_ASSERT(vector);
    return (const void **)vector->items;
}

ci_vector_t *ci_vector_cast_from_voidvoid(const void **p)
{
    const void *buf;
    ci_vector_t *v;
    _CI_ASSERT(p);
    v = (ci_vector_t *)((char *)p - _CI_ALIGN(sizeof(ci_vector_t)));
    buf = (const void *)((char *)v - ci_pack_allocator_required_size());
    _CI_ASSERT(v->mem == buf);
    _CI_ASSERT(ci_buffer_check(buf));
    return v;
}

void ci_vector_destroy(ci_vector_t *vector)
{
    void *buffer;
    _CI_ASSERT(vector);
    _CI_ASSERT(vector->mem);
    buffer = vector->mem;
    if (vector->alloc)
        ci_mem_allocator_destroy(vector->alloc);
    ci_buffer_free(buffer);
}

const void *ci_vector_get(const ci_vector_t *vector, unsigned int i)
{
    _CI_ASSERT(vector);
    return (i < vector->count ? (const void *)vector->items[i]:  (const void *)NULL);
}

const void * ci_vector_get2(const ci_vector_t *vector, unsigned int i, size_t *size)
{
    _CI_ASSERT(vector);
    if (i >= vector->count)
        return (const void *)NULL;
    const void *item = (const void *)vector->items[i];
    if (size) {
        if (i > 0)
            *size = vector->items[i - 1] - vector->items[i];
        else {
            void *vector_data_end = vector->mem + vector->max_size;
            *size = vector_data_end - vector->items[0];
        }
    }
    return item;
}

int ci_vector_size(const ci_vector_t *vector)
{
    _CI_ASSERT(vector);
    return vector->count;
}

const void * ci_vector_add(ci_vector_t *vector, const void *value, size_t size)
{
    void *item, **indx;
    ci_mem_allocator_t *packer = vector->alloc;
    _CI_ASSERT(vector);
    _CI_ASSERT(vector->alloc);
    packer = vector->alloc;
    indx = ci_pack_allocator_alloc_unaligned(packer, array_item_size(void *));
    int align = (vector->flags & CI_VECTOR_MEM_ALIGN);
    item = ci_pack_allocator_alloc_from_rear2(packer, size, align);
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
    _CI_ASSERT(vector);
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

void ci_vector_iterate(const ci_vector_t *vector, void *data, int (*fn)(void *data, const void *))
{
    int i, ret = 0;
    _CI_ASSERT(vector);
    for (i = 0; vector->items[i] != NULL && ret == 0; i++)
        ret = (*fn)(data, vector->items[i]);
}

size_t ci_vector_data_size(ci_vector_t *v)
{
    _CI_ASSERT(v);
    const void *vector_data_start = (const void *)(v->items[v->count -1]);
    const void *vector_data_end = v->mem + v->max_size;
    /*compute the required memory for storing the vector*/
    size_t vector_data_size = vector_data_end - vector_data_start;
    return vector_data_size;
}

/*ci_str_vector functions */
ci_str_vector_t *ci_str_vector_create(size_t max_size)
{
    ci_vector_t *v = ci_vector_create(max_size);
    v->flags &=  ~CI_VECTOR_MEM_ALIGN;
    return (ci_str_vector_t *)v;
}

const char *ci_str_vector_get(ci_str_vector_t *vector, unsigned int i)
{
    _CI_ASSERT(vector);
    return (i < vector->count ? (const char *)vector->items[i]:  (const char *)NULL);
}

const char *ci_str_vector_add(ci_str_vector_t *vect, const char *string)
{
    return (const char *)ci_vector_add((ci_vector_t *)vect, string, strlen(string) + 1);
}

const char *ci_str_vector_add2(ci_str_vector_t *vect, const char *string, size_t len)
{
    /* Allocate and store len + 1 to terminate string, just in case it
       is not NULL terminated. */
    char *val = (char *)ci_vector_add((ci_vector_t *)vect, string, len + 1);
    *(val + len) = '\0';
    return (const char *)val;
}

void ci_str_vector_iterate(const ci_str_vector_t *vector, void *data, int (*fn)(void *data, const char *))
{
    ci_vector_iterate(vector, data, (int(*)(void *, const void *))fn);
}

const char * ci_str_vector_search(ci_str_vector_t *vector, const char *item)
{
    int i;
    _CI_ASSERT(vector);
    for (i = 0; vector->items[i] != NULL; i++) {
        if (strcmp(vector->items[i], item) == 0)
            return vector->items[i];
    }

    return NULL;
}


/*ci_ptr_vector functions....*/
void * ci_ptr_vector_add(ci_vector_t *vector, void *value)
{
    void **indx;
    ci_mem_allocator_t *packer;
    _CI_ASSERT(vector);
    _CI_ASSERT(vector->alloc);
    packer = vector->alloc;

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
    vector->count++;
    return value;
}


/****************/
/* Lists API    */

ci_list_t * ci_list_create(size_t init_size, size_t obj_size)
{
    ci_list_t *list = NULL;
    ci_mem_allocator_t *alloc = NULL;
    if (init_size < 1024)
        init_size = 1024;

    alloc = ci_create_serial_allocator(init_size);
    list = alloc->alloc(alloc, sizeof(ci_list_t));
    list->alloc = alloc;
    list->items = NULL;
    list->last = NULL;
    list->trash = NULL;
    list->cursor = NULL;
    list->obj_size = obj_size;

    /*By default do not use any handler*/
    list->cmp_func = NULL;
    list->copy_func = NULL;
    list->free_func = NULL;
    return list;
}

void ci_list_destroy(ci_list_t *list)
{
    _CI_ASSERT(list);
    ci_mem_allocator_t *alloc = list->alloc;
    ci_mem_allocator_destroy(alloc);
}

void * ci_list_first(ci_list_t *list)
{
    if (list && list->items) {
        list->cursor = list->items->next;
        return list->items->item;
    }
    return NULL;
}

void * ci_list_next(ci_list_t *list)
{
    if ((list->tmp = list->cursor) != NULL) {
        list->cursor = list->cursor->next;
        return list->tmp->item;
    }
    return NULL;
}

void * ci_list_iterator_first(const ci_list_t *list, ci_list_iterator_t *it)
{
    _CI_ASSERT(list);
    _CI_ASSERT(it);
    if ((*it = list->items))
        return (*it)->item;
    return NULL;
}

void * ci_list_iterator_next(ci_list_iterator_t *it)
{
    _CI_ASSERT(it);
    if ((*it) && (*it = ((*it)->next)))
        return (*it)->item;
    return NULL;
}

void * ci_list_iterator_value(const ci_list_iterator_t *it)
{
    _CI_ASSERT(it);
    return (*it) ? (*it)->item : NULL;
}

void * ci_list_head(const ci_list_t *list)
{
    return (list && list->items != NULL ? list->items->item : NULL);
}

void * ci_list_tail(const ci_list_t *list)
{
    return (list && list->last != NULL ? list->last->item : NULL);
}

void ci_list_cmp_handler(ci_list_t *list, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size))
{
    _CI_ASSERT(list);
    list->cmp_func = cmp_func;
}

void ci_list_free_handler(ci_list_t *list, void (*free_func)(void *obj))
{
    _CI_ASSERT(list);
    list->free_func = free_func;
}

void ci_list_copy_handler(ci_list_t *list, int (*copy_func)(void *newObj, const void *oldObj))
{
    _CI_ASSERT(list);
    list->copy_func = copy_func;
}

void ci_list_iterate(ci_list_t *list, void *data, int (*fn)(void *data, const void *obj))
{
    ci_list_item_t *it;
    _CI_ASSERT(list);
    for (list->cursor = list->items; list->cursor != NULL; ) {
        it = list->cursor;
        list->cursor = list->cursor->next;
        if ((*fn)(data, it->item))
            return;
    }
}

static ci_list_item_t *list_alloc_item(ci_list_t *list, const void *data)
{
    ci_list_item_t *it;
    _CI_ASSERT(list);
    if (list->trash) {
        it = list->trash;
        list->trash = list->trash->next;
    } else {
        it = list->alloc->alloc(list->alloc, sizeof(ci_list_item_t));
        if (!it)
            return NULL;

        if (list->obj_size) {
            it->item = list->alloc->alloc(list->alloc, list->obj_size);
            if (!it->item)
                return NULL;
        }
    }
    it->next = NULL;
    if (list->obj_size) {
        memcpy(it->item, data, list->obj_size);
        if (list->copy_func)
            list->copy_func(it->item, data);
    } else
        it->item = (void *)data;
    return it;
}

const void * ci_list_push(ci_list_t *list, const void *data)
{
    _CI_ASSERT(list);
    ci_list_item_t *it = list_alloc_item(list, data);
    if (!it)
        return NULL;
    if (list->items) {
        it->next = list->items;
        list->items = it;
    } else {
        list->items = list->last = it;
    }
    return it->item;
}

const void * ci_list_push_back(ci_list_t *list, const void *data)
{
    _CI_ASSERT(list);
    ci_list_item_t *it = list_alloc_item(list, data);
    if (!it)
        return NULL;
    if (list->last != NULL) {
        list->last->next = it;
        list->last = it;
    } else {
        list->items = list->last = it;
    }
    return it->item;
}

void *ci_list_pop(ci_list_t *list, void *data)
{
    _CI_ASSERT(list);
    ci_list_item_t *it = list->items;
    if (list->items == NULL)
        return NULL;

    if (list->last == list->items) {
        list->last = NULL;
        list->items = NULL;
        list->cursor = NULL;
    } else {
        if (list->cursor == list->items)
            list->cursor = list->items->next;
        list->items = list->items->next;
    }

    it->next = list->trash;
    list->trash = it;

    if (list->obj_size) {
        memcpy(data, it->item, list->obj_size);
        if (list->copy_func)
            list->copy_func(data, it->item);
        if (list->free_func)
            list->free_func(it->item);
        return data;
    } else
        return (*((void **)data) = it->item);
}

void *ci_list_pop_back(ci_list_t *list, void *data)
{
    _CI_ASSERT(list);
    ci_list_item_t *tmp, *it = list->last;
    if (list->items == NULL)
        return NULL;

    if (list->last == list->items) {
        list->last = NULL;
        list->items = NULL;
        list->cursor = NULL;
    } else {
        if (list->cursor == list->last)
            list->cursor = NULL;
        for (tmp = list->items; tmp != NULL && tmp->next != list->last; tmp = tmp->next){}
        _CI_ASSERT(tmp != NULL);
        list->last = tmp;
        list->last->next = NULL;
    }

    it->next = list->trash;
    list->trash = it;

    if (list->obj_size) {
        memcpy(data, it->item, list->obj_size);
        if (list->copy_func)
            list->copy_func(data, it->item);
        if (list->free_func)
            list->free_func(it->item);
        return data;
    } else
        return (*((void **)data) = it->item);
}

static int default_cmp(const void *obj1, const void *obj2, size_t size)
{
    return memcmp(obj1, obj2, size);
}

static int pointers_cmp(const void *obj1, const void *obj2, size_t size)
{
    return (obj1 == obj2 ? 0 : (obj1 > obj2 ? 1 : -1));
}

int ci_list_remove2(ci_list_t *list, const void *obj, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size))
{
    ci_list_item_t *it, *prev;
    _CI_ASSERT(list);
    prev = NULL;
    for (it = list->items; it != NULL; prev = it,it = it->next) {
        if (cmp_func(it->item, obj, list->obj_size) == 0) {
            if (prev) {
                prev->next = it->next;
            } else { /*it is the first item*/
                list->items = it->next;
            }
            if (list->cursor == it)
                list->cursor = list->cursor->next;
            it->next = list->trash;
            list->trash = it;
            if (list->free_func && list->obj_size)
                list->free_func(it->item);
            return 1;
        }
    }

    return 0;
}

int ci_list_remove3(ci_list_t *list, const void *user_data, void *store_removed, size_t store_removed_size, int (*cmp_func)(const void *obj, const void *user_data, size_t obj_size))
{
    _CI_ASSERT(list);
    _CI_ASSERT(store_removed_size >= list->obj_size);
    ci_list_item_t *it, *prev;
    for (it = list->items, prev = NULL; it != NULL; prev = it,it = it->next) {
        if (cmp_func(it->item, user_data, list->obj_size) == 0) {
            memcpy(store_removed, it->item, list->obj_size);
            if (list->copy_func)
                list->copy_func(store_removed, it->item);
            if (prev) {
                prev->next = it->next;
            } else { /*it is the first item*/
                list->items = it->next;
            }
            if (list->cursor == it)
                list->cursor = list->cursor->next;
            it->next = list->trash;
            list->trash = it;
            if (list->free_func && list->obj_size)
                list->free_func(it->item);
            return 1;
        }
    }

    return 0;
}

int ci_list_remove(ci_list_t *list, const void *obj)
{
    int (*cmp_func)(const void *, const void *, size_t);
    _CI_ASSERT(list);
    if (list->cmp_func)
        cmp_func = list->cmp_func;
    else if (list->obj_size)
        cmp_func = default_cmp;
    else
        cmp_func = pointers_cmp;

    return ci_list_remove2(list, obj, cmp_func);
}

const void * ci_list_search(const ci_list_t *list, const void *data)
{
    int (*cmp_func)(const void *, const void *, size_t);
    _CI_ASSERT(list);
    if (list->cmp_func)
        cmp_func = list->cmp_func;
    else if (list->obj_size)
        cmp_func = default_cmp;
    else
        cmp_func = pointers_cmp;

    return ci_list_search2(list, data, cmp_func);
}

const void * ci_list_search2(const ci_list_t *list, const void *user_data, int (*cmp_func)(const void *obj, const void *user_data, size_t obj_size))
{
    ci_list_item_t *it;
    _CI_ASSERT(list);
    for (it = list->items; it != NULL; it = it->next) {
        if (cmp_func(it->item, user_data, list->obj_size) == 0)
            return it->item;
    }
    return NULL;
}

void ci_list_sort(ci_list_t *list)
{
    int (*cmp_func)(const void *, const void *, size_t);
    _CI_ASSERT(list);
    if (list->cmp_func)
        cmp_func = list->cmp_func;
    else if (list->obj_size)
        cmp_func = default_cmp;
    else
        cmp_func = pointers_cmp;

    ci_list_sort2(list, cmp_func);
}

void ci_list_sort2(ci_list_t *list, int (*cmp_func)(const void *obj1, const void *obj2, size_t obj_size))
{
    ci_list_item_t *it;
    ci_list_item_t *sortedHead = NULL, *sortedTail = NULL;
    ci_list_item_t **currentSorted, *currentHead;
    _CI_ASSERT(list);
    if (!list->items || ! list->items->next)
        return;

    it = list->items;
    while (it) {
        currentHead = it;
        it = it->next;
        currentSorted = &sortedHead;
        while (!(*currentSorted == NULL || cmp_func(currentHead->item, (*currentSorted)->item, list->obj_size) < 0))
            currentSorted = &(*currentSorted)->next;
        currentHead->next = *currentSorted;
        *currentSorted = currentHead;
        if ((*currentSorted)->next == NULL)
            sortedTail = (*currentSorted);
    }
    list->items = sortedHead;
    list->last = sortedTail;
}

/*Flat arrays functions*/
size_t ci_flat_array_build_from_vector_to(ci_vector_t *v, void *buf, size_t buf_size)
{
    _CI_ASSERT(v);
    const void *vector_data_start = (const void *)(v->items[v->count -1]);
    const void *vector_data_end = v->mem + v->max_size;
    /*compute the required memory for storing the vector*/
    size_t vector_data_size = vector_data_end - vector_data_start;
    size_t vector_indx_size = (v->count + 1) * sizeof(void *);
    const size_t flat_size = sizeof(void *) + vector_indx_size + vector_data_size;
    if (!buf)
        return flat_size;

    if (flat_size > buf_size)
        return 0;

    void **ppvoid = (void **)buf;
    ppvoid[0] = (void *)flat_size;
    void **data_indx = ppvoid + 1;
    void *data = buf + sizeof(void *) + vector_indx_size;
    memcpy(data, vector_data_start, vector_data_size);
    int i;
    for (i = 0; v->items[i]!= NULL; i++) {
        data_indx[i] = (void *)((void *)v->items[i] - vector_data_start + vector_indx_size + sizeof(void *));
        _CI_ASSERT(data_indx[i] <= (void *)flat_size);
    }
    data_indx[i] = NULL;

    return flat_size;
}

void *ci_flat_array_build_from_vector(ci_vector_t *v)
{
    const size_t flat_size = ci_vector_data_size(v) + (ci_vector_size(v) + 1) * sizeof(void *) + sizeof(void *);
    void *flat = ci_buffer_alloc(flat_size);
    int ret = ci_flat_array_build_from_vector_to(v, flat, flat_size);
    if (!ret) {
        ci_buffer_free(flat);
        return NULL;
    }
    return flat;
}

int ci_flat_array_copy_to_ci_vector_t(const void *flat, ci_vector_t *v)
{
    const void *item = NULL;
    size_t item_size = 0;
    int i;
    for(i = 0; (item = ci_flat_array_item(flat, i, &item_size)) != NULL; i++) {
        if (!ci_vector_add(v, item, item_size))
            return 0; /*Not enough space? abort*/
    }
    return 1;
}

ci_vector_t * ci_flat_array_to_ci_vector_t(const void *flat)
{
    size_t flat_size = ci_flat_array_size(flat);
    /*A vector needs space for data plus index like the flat arrays and also
      some space for vector structures. About 1024 bytes for the last should
      be enough.*/
    ci_vector_t *v = ci_vector_create(flat_size + 1024);
    if (!ci_flat_array_copy_to_ci_vector_t(flat, v)) {
        ci_vector_destroy(v);
        ci_debug_printf(1, "Failed to build a ci_vector_t from flat array\n");
        return NULL;
    }
    return v;
}

void **ci_flat_array_to_ppvoid(void *flat, size_t *data_size)
{
    /*First item of flat is the size of flat array.
      We are going to remove it to allow release the
      generated 'void **' pointer with a single *free function.
    */
    void **ppvoid = (void **)flat;
    size_t flat_size = (size_t)ppvoid[0];
    void **data_indx = ppvoid + 1;
    int i;
    for (i = 0; data_indx[i] != NULL; i++) {
        _CI_ASSERT(data_indx[i] <= (void *)flat_size);
        /*The data_indx has the pos of the array item inside the array.
          convert it to pointer.
         */
        ppvoid[i] = (flat + (size_t)data_indx[i]);
    }
    ppvoid[i] = NULL;
    if (data_size)
        *data_size = flat_size;
    return ppvoid;
}

int ci_flat_array_check(const void *flat)
{
    void **ppvoid = (void **)flat;
    size_t flat_size = (size_t)ppvoid[0];
    void **data_indx = ppvoid + 1;
    int i;
    if (data_indx[0] > (void *)flat_size)
        return 0;
    for (i = 1; data_indx[i] != NULL; i++) {
        if (data_indx[i] > data_indx[i - 1])
            return 0;
    }
    return 1;
}

const void *ci_flat_array_item(const void *flat, int indx, size_t *data_size)
{
    _CI_ASSERT(flat);
    void **ppvoid = (void **)flat;
    size_t flat_size = (size_t)ppvoid[0];
    void **data_indx = ppvoid + 1;
    if (data_indx[indx]) {
        const size_t end_pos = (indx == 0 ? flat_size : (size_t)data_indx[indx - 1]);
        _CI_ASSERT((size_t)data_indx[indx] <= end_pos);
        if (data_size)
            *data_size = (end_pos - (size_t)data_indx[indx]);
        return flat + (size_t)data_indx[indx];
    }
    return NULL;
}

static size_t flat_required_size(const void *items[], size_t item_sizes[])
{
    int i;
    size_t flat_size = 0;
    for (i = 0; items[i] != NULL; i++) {
        flat_size += item_sizes[i];
    }
    flat_size += (i + 1) * sizeof(void *)/*null terminated items index*/ + sizeof(void *) /*array size*/;
    return flat_size;
}

static void flat_build(const void *items[], size_t item_sizes[], void *flat, size_t flat_size)
{
    int i;
    void **indx = (void **)flat;
    indx[0] = (void *) flat_size;
    indx++;
    void *store = flat + flat_size;
    for(i = 0; items[i] != NULL; i++) {
        store -= item_sizes[i];
        _CI_ASSERT((flat + (i + 1) * sizeof(void *)) < store);
        memcpy(store, items[i], item_sizes[i]);
        indx[i] = (void *)(store - flat);
    }
    indx[i] = NULL;
}

int ci_flat_array_build_to(const void *items[], size_t item_sizes[], void *buffer, size_t buffer_size)
{
    size_t flat_size = flat_required_size(items, item_sizes);
    if (buffer_size < flat_size)
        return 0;
    flat_build(items, item_sizes, buffer, buffer_size);
    return 1;
}

void * ci_flat_array_build(const void *items[], size_t item_sizes[])
{
    size_t flat_size = flat_required_size(items, item_sizes);
    void *flat = ci_buffer_alloc(flat_size);
    if (flat)
        flat_build(items, item_sizes, flat, flat_size);
    return flat;
}

int ci_flat_array_strings_build_to(const char *items[], void *buffer, size_t buffer_size)
{
    size_t items_sizes[1024]; /*Assume a maximum of 1024 items*/
    int i;
    for (i = 0; items[i] != NULL && i < 1024; i++)
        items_sizes[i] = strlen(items[i]) + 1;
    return ci_flat_array_build_to((const void **)items, items_sizes, buffer, buffer_size);
}

void * ci_flat_array_strings_build(const char *items[])
{
    size_t items_sizes[1024]; /*Assume a maximum of 1024 items*/
    int i;
    for (i = 0; items[i] != NULL && i < 1024; i++)
        items_sizes[i] = strlen(items[i]) + 1;
    return ci_flat_array_build((const void **)items, items_sizes);
}
