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
    _CI_ASSERT(buffer);
    if (array->alloc)
        ci_mem_allocator_destroy(array->alloc);
    ci_buffer_free(buffer);
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
    _CI_ASSERT(vector);
    return (const void **)vector->items;
}

ci_vector_t *ci_vector_cast_from_voidvoid(const void **p)
{
    const void *buf;
    ci_vector_t *v;
    _CI_ASSERT(p);
    v = (ci_vector_t *)((void *)p - _CI_ALIGN(sizeof(ci_vector_t)));
    buf = (void *)v - ci_pack_allocator_required_size();
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

const void * ci_vector_add(ci_vector_t *vector, const void *value, size_t size)
{
    void *item, **indx;
    ci_mem_allocator_t *packer = vector->alloc;
    _CI_ASSERT(vector);
    _CI_ASSERT(vector->alloc);
    packer = vector->alloc;
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


/*ci_str_vector functions */
const char *ci_str_vector_add(ci_str_vector_t *vect, const char *string)
{
    return (const char *)ci_vector_add((ci_vector_t *)vect, string, strlen(string) + 1);
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

const void * ci_list_search2(const ci_list_t *list, const void *data, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size))
{
    ci_list_item_t *it;
    _CI_ASSERT(list);
    for (it = list->items; it != NULL; it = it->next) {
        if (cmp_func(it->item, data, list->obj_size) == 0)
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
