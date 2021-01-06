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

#ifndef __C_ICAP_ARRAY_H
#define __C_ICAP_ARRAY_H

#include "c-icap.h"
#include "mem.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup ARRAYS  Arrays, stacks, queues and vectors related API
 \ingroup API
 * Arrays, stacks, queues and vectors related API.
 */

typedef struct ci_array_item {
    char *name;
    void *value;
} ci_array_item_t;

/**
 \defgroup SIMPLE_ARRAYS  Simple arrays related API
 \ingroup ARRAYS
 * Arrays which store  name/value pair items
*/

/**
   \typedef ci_array_t
   \ingroup SIMPLE_ARRAYS
   * The ci_array_t objects can store a list of name/value pairs. Currently
   * can grow up to a fixed size.
 */
typedef struct ci_array {
    ci_array_item_t *items;
    char *mem;
    size_t max_size;
    unsigned int count;
    ci_mem_allocator_t *alloc;
} ci_array_t;

/**
 \def ci_array_value(array, pos)
 \ingroup SIMPLE_ARRAYS
 * Return the value of item on position 'pos'
 */
#define ci_array_value(array, pos) (pos < (array)->count ?  (array)->items[pos].value : NULL)

/**
 \def ci_array_name(array, pos)
 \ingroup SIMPLE_ARRAYS
 * Return the name of item on position 'pos'
 */
#define ci_array_name(array, pos) (pos < (array)->count ?  (array)->items[pos].name : NULL)

/**
 \def ci_array_size(array)
 \ingroup SIMPLE_ARRAYS
 * Return the size of array 'array'
 */
#define ci_array_size(array) ((array)->count)

/**
 * Allocate the required memory and initialize an ci_array_t object
 \ingroup SIMPLE_ARRAYS
 \param max_mem_size the maximum memory to use
 \return the allocated object on success, or NULL on failure
 *
 */
CI_DECLARE_FUNC(ci_array_t *) ci_array_new(size_t max_mem_size);

/**
 * Create and initialize an ci_array_t object for the given number of items
 \ingroup SIMPLE_ARRAYS
 \param items the maximum aray items
 \param item_size the items size
 \return the allocated object on success, or NULL on failure
 */
CI_DECLARE_FUNC(ci_array_t *) ci_array_new2(size_t items, size_t item_size);

/**
 * Destroy an ci_array_t object
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to ci_array_t object to be destroyed
 *
 */
CI_DECLARE_FUNC(void) ci_array_destroy(ci_array_t *array);

/**
 * Add an name/value pair item to the array.
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param name the name part of the name/value pair item to add
 \param value the value part of the name/value pair item to add
 \param size the size of the value part of the new item.
 \return a pointer to the new array item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_array_add(ci_array_t *array, const char *name, const void *value, size_t size);

/**
 * Delete the last element of the array.
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \return a pointer to the popped array item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const ci_array_item_t *)ci_array_pop(ci_array_t *array);

/**
 * Search in an array for an item with the given name
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param name the item to be search for.
 \return pointer to the value pair of the array item if found, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_array_search(ci_array_t *array, const char *name);

/**
 * Run the given function for each array item
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param data a pointer to data which will be passed on fn function
 \param fn a pointer to the function which will be run for each array item. The iteration will stop if the fn function return non zero value
 */
CI_DECLARE_FUNC(void) ci_array_iterate(const ci_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *));

/**
 * Get an item of the array.
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param pos The position of the item in array
 \return a pointer to the array item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_array_get_item(ci_array_t *array, int pos);

/**
 \defgroup STR_ARRAYS   Arrays of strings related API
 \ingroup SIMPLE_ARRAYS
 * Arrays which store  name/value pair items
*/

/**
 \typedef ci_str_array_t
 \ingroup STR_ARRAYS
 * An alias to the ci_array_t object. It is used to store items with string
 * values to an array.
 * The ci_str_array_new, ci_str_array_destroy, ci_str_array_add,
 * ci_str_array_search and ci_str_array_iterate defines are similar to
 * the equivalent ci_array_* functions with the required typecasting to
 * work with strings.
 */
typedef ci_array_t ci_str_array_t;
#define ci_str_array_new ci_array_new
#define ci_str_array_destroy ci_array_destroy
#define ci_str_array_add(array, name, value) ci_array_add((ci_array_t *)(array), name, value, (strlen(value)+1))
#define ci_str_array_pop(array) ci_array_pop((ci_array_t *)(array))
#define ci_str_array_get_item(array, pos) ci_array_get_item((ci_array_t *)(array), pos)
#define ci_str_array_search(array, name) (const char *)ci_array_search((ci_array_t *)(array), name)
#define ci_str_array_iterate ci_array_iterate
#define ci_str_array_value(array, pos) ci_array_value((ci_array_t *)(array), pos)
#define ci_str_array_name(array, pos) ci_array_name((ci_array_t *)(array), pos)
#define ci_str_array_size(array) ci_array_size((ci_array_t *)(array))


/**
 \defgroup PTR_ARRAYS  Arrays of pointers
 \ingroup SIMPLE_ARRAYS
 * Arrays of name/pointers to objects pairs
 */

/**
 \typedef ci_ptr_array_t
 \ingroup PTR_ARRAYS
 * The ci_ptr_array_t objects can store a list of name and pointer to object
 * pairs. It is similar to the ci_array_t object but does not store the value
 * but a pointer to the value.
 */
typedef ci_array_t ci_ptr_array_t;

/**
 \def ci_ptr_array_value(ptr_array, pos)
 \ingroup PTR_ARRAYS
 * Return the value of item at position 'pos'
 */
#define ci_ptr_array_value(array, pos) ci_array_value((ci_array_t *)(array), pos)

/**
 \def ci_ptr_array_value(ptr_array, pos)
 \ingroup PTR_ARRAYS
 * Return the name of item at position 'pos'
 */
#define ci_ptr_array_name(array, pos) ci_array_name((ci_array_t *)(array), pos)

/**
 \def ci_ptr_array_value(ptr_array)
 \ingroup PTR_ARRAYS
 * Return the size of ptr_array
 */
#define ci_ptr_array_size(array) ci_array_size((ci_array_t *)(array))

/**
 \def ci_ptr_array_new()
 \ingroup PTR_ARRAYS
 * Create a new ci_ptr_array_t object. Similar to the ci_array_new() function.
 */
#define ci_ptr_array_new ci_array_new

/**
 * Create and initialize an ci_ptr_array_t object for the given number of items
 \ingroup PTR_ARRAYS
 \param items the maximum aray items
 \return the allocated object on success, or NULL on failure
 */
CI_DECLARE_FUNC(ci_ptr_array_t *) ci_ptr_array_new2(size_t items);

/**
 \def ci_ptr_array_destroy(ptr_array)
 \ingroup PTR_ARRAYS
 * Destroy a ci_ptr_array_t object. Similar to the ci_array_destroy function
 */
#define ci_ptr_array_destroy(ptr_array) ci_array_destroy((ci_array_t *)(ptr_array))

/**
 * Search in an array for an item with the given name
 \ingroup PTR_ARRAYS
 \param array a pointer to the ci_ptr_array_t object
 \param name the item to be search for.
 \return pointer to the value pair of the array item if found, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_ptr_array_search(ci_ptr_array_t *array, const char *name);


/**
 \def ci_ptr_array_iterate(ptr_array, data, fn)
 \ingroup PTR_ARRAYS
 * Run the function fn for each item of the ci_ptr_array_t object. Similar to
 * the ci_array_iterate function
 */
#define ci_ptr_array_iterate(ptr_array, data, fn) ci_array_iterate((ci_array_t *)(ptr_array), data, fn)

/**
 * Add an name/value pair item to the ci_ptr_array_t object.
 \ingroup PTR_ARRAYS
 \param ptr_array a pointer to the ci_ptr_array_t object
 \param name the name part of the name/value pair item to be added
 \param value a pointer to the value part of the name/value pair item to be
 *      added
 \return a pointer to the new array item on success, NULL otherwise
 *
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_ptr_array_add(ci_ptr_array_t *ptr_array, const char *name, void *value);

/**
 * Pop and delete the last item of a  ci_ptr_array_t object.
 \ingroup PTR_ARRAYS
 \param ptr_array a pointer to the ci_ptr_array_t object
 \return a pointer to the popped array item
*/
CI_DECLARE_FUNC(const ci_array_item_t *) ci_ptr_array_pop(ci_ptr_array_t *ptr_array);

/**
 * Pop and delete the last item of a  ci_ptr_array_t object.
 \ingroup PTR_ARRAYS
 \param ptr_array a pointer to the ci_ptr_array_t object
 \param name a pointer to a buffer where the name of the poped item will be
 *      store, or NULL
 \param name_size the size of name buffer
 \return a pointer to the value of the popped item
*/
CI_DECLARE_FUNC(void *) ci_ptr_array_pop_value(ci_ptr_array_t *ptr_array, char *name, size_t name_size);

/**
 \def ci_ptr_array_get_item()
 \ingroup PTR_ARRAYS
 * Get an array item. Wrapper to the ci_array_get_item() function.
 */
#define ci_ptr_array_get_item(array, pos) ci_array_get_item((ci_array_t *)(array), pos)

/**
 \defgroup DYNAMIC_ARRAYS Dynamic arrays related API
 \ingroup ARRAYS
 * Arrays which store  name/value pair items, and can grow unlimited.
 *
 */

/**
 \typedef ci_dyn_array_t
 \ingroup DYNAMIC_ARRAYS
 * The ci_dyn_array_t objects can store a list of name/value pairs.
 * The memory RAM space of dynamic array items can not be released
 * before the ci_dyn_array destroyed.
 */
typedef struct ci_dyn_array {
    ci_array_item_t **items;
    int count;
    int max_items;
    ci_mem_allocator_t *alloc;
} ci_dyn_array_t;

/**
 \def ci_dyn_array_get_item(array, pos)
 \ingroup DYNAMIC_ARRAYS
 * Return the ci_array_item_t item on position 'pos'
 */
#define ci_dyn_array_get_item(array, pos) (pos < (array)->count ? (array)->items[pos] : NULL)

/**
 \def ci_dyn_array_value(array, pos)
 \ingroup DYNAMIC_ARRAYS
 * Return the value of item on position 'pos'
 */
#define ci_dyn_array_value(array, pos) ((pos < (array)->count && (array)->items[pos] != NULL) ?  (array)->items[pos]->value : NULL)

/**
 \def ci_dyn_array_name(array, pos)
 \ingroup DYNAMIC_ARRAYS
 * Return the name of item on position 'pos'
 */
#define ci_dyn_array_name(array, pos) ((pos < (array)->count && (array)->items[pos] != NULL) ?  (array)->items[pos]->name : NULL)

/**
 \def ci_dyn_array_size(array)
 \ingroup DYNAMIC_ARRAYS
 * Return the size of array 'array'
 */
#define ci_dyn_array_size(array) ((array)->count)

/**
 * Allocate the required memory and initialize an ci_dyn_array_t object
 \ingroup DYNAMIC_ARRAYS
 \param mem_size the initial size to use for dyn_array
 \return the allocated object on success, or NULL on failure
 *
 */
CI_DECLARE_FUNC(ci_dyn_array_t *) ci_dyn_array_new(size_t mem_size);

/**
 * Create and initialize an ci_dyn_array_t object for the given number of items
 \ingroup DYNAMIC_ARRAYS
 \param items the maximum aray items
 \param item_size the items size
 \return the allocated object on success, or NULL on failure
 */
CI_DECLARE_FUNC(ci_dyn_array_t *) ci_dyn_array_new2(size_t items, size_t item_size);

/**
 * Destroy an ci_dyn_array_t object
 \ingroup DYNAMIC_ARRAYS
 \param array a pointer to ci_dyn_array_t object to be destroyed
 */
CI_DECLARE_FUNC(void) ci_dyn_array_destroy(ci_dyn_array_t *array);

/**
 * Add an name/value pair item to a dynamic array.
 \ingroup DYNAMIC_ARRAYS
 \param array a pointer to the ci_dyn_array_t object
 \param name the name part of the name/value pair item to be added
 \param value the value part of the name/value pair item to be added
 \param size the size of the value part of the new item.
 \return a pointer to the new array item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_dyn_array_add(ci_dyn_array_t *array, const char *name, const void *value, size_t size);

/**
 * Search in an dynamic array for an item with the given name
 \ingroup DYNAMIC_ARRAYS
 \param array a pointer to the ci_dyn_array_t object
 \param name the item to be search for.
 \return pointer to the value pair of the array item if found, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_dyn_array_search(ci_dyn_array_t *array, const char *name);

/**
 * Run the given function for each dynamic array item
 \ingroup DYNAMIC_ARRAYS
 \param array a pointer to the ci_dyn_array_t object
 \param data a pointer to data which will be passed on fn function
 \param fn a pointer to the function which will be run for each array item.
 *      The iteration will stop if the fn function return non zero value.
 */
CI_DECLARE_FUNC(void) ci_dyn_array_iterate(const ci_dyn_array_t *array, void *data, int (*fn)(void *data, const char *name, const void *));

/**
 \defgroup PTR_DYNAMIC_ARRAYS   Dynamic arrays of pointers related API
 \ingroup DYNAMIC_ARRAYS
 * Arrays which store  name/value pair items
*/

/**
 \typedef ci_ptr_dyn_array_t
 \ingroup PTR_DYNAMIC_ARRAYS
 * An alias to the ci_dyn_array_t object. It is used to store pointers
 * to an array.
 * The ci_ptr_dyn_array_new, ci_ptr_dyn_array_destroy, ci_ptr_dyn_array_search
 * and ci_ptr_dyn_array_iterate defines are  equivalent to the ci_dyn_array_*
 * functions with the required typecasting.
 */
typedef ci_dyn_array_t ci_ptr_dyn_array_t;
#define ci_ptr_dyn_array_new(size) ci_dyn_array_new(size)
#define ci_ptr_dyn_array_new2(items, item_size) ci_dyn_array_new2(items, item_size)
#define ci_ptr_dyn_array_destroy(ptr_array) ci_dyn_array_destroy((ci_dyn_array_t *)(ptr_array))
#define ci_ptr_dyn_array_search(ptr_array, name) ci_dyn_array_search((ci_dyn_array_t *)(ptr_array), name)
#define ci_ptr_dyn_array_iterate(ptr_array, data, fn) ci_dyn_array_iterate((ci_dyn_array_t *)(ptr_array), data, fn)

#define ci_ptr_dyn_array_get_item(ptr_array, pos) ci_dyn_array_get_item((ci_dyn_array_t *)(ptr_array), pos)
#define ci_ptr_dyn_array_value(ptr_array, pos) ci_dyn_array_value((ci_dyn_array_t *)(ptr_array), pos)
#define ci_ptr_dyn_array_name(ptr_array, pos) ci_dyn_array_name((ci_dyn_array_t *)(ptr_array), pos)
#define ci_ptr_dyn_array_size(ptr_array) ci_dyn_array_size((ci_dyn_array_t *)(ptr_array))

/**
 * Add an name/value pair item to the array.
 \ingroup  PTR_DYNAMIC_ARRAYS
 \param ptr_array a pointer to the ci_ptr_dyn_array_t object
 \param name the name part of the name/pointer pair item to be added
 \param pointer the pointer part of the name/value pair item to be added
 \return a pointer to the new array item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_ptr_dyn_array_add(ci_ptr_dyn_array_t *ptr_array, const char *name, void *pointer);


/**
 \defgroup VECTORS  Simple vectors related API
 \ingroup ARRAYS
 * Structure which can store lists of objects
 */

/**
 \typedef ci_vector_t
 \ingroup VECTORS
 * The ci_vector_t objects can store a list of objects. Currently can grow up
 * to a fixed size.
 */
typedef struct ci_vector {
    void **items;
    void **last;
    char *mem;
    size_t max_size;
    int count;
    ci_mem_allocator_t *alloc;
} ci_vector_t;

/**
 * Allocate the required memory and initialize a ci_vector_t object
 \ingroup VECTORS
 \param max_size the maximum memory to use
 \return the allocated object on success, or NULL on failure
 */
CI_DECLARE_FUNC(ci_vector_t *) ci_vector_create(size_t max_size);

/**
 * Destroy an ci_vector_t object
 \ingroup VECTORS
 \param vector a pointer to ci_vector_t object to be destroyed
 */
CI_DECLARE_FUNC(void) ci_vector_destroy(ci_vector_t *vector);

/**
 * Add an  item to the vector.
 \ingroup VECTORS
 \param vector a pointer to the ci_vector_t object
 \param obj pointer to the object to add in vector
 \param size the size of the new item.
 \return a pointer to the new  item on success, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_vector_add(ci_vector_t *vector, const void *obj, size_t size);

/**
 * Run the given function for each vector item
 \ingroup VECTORS
 \param vector a pointer to the ci_vector_t object
 \param data a pointer to data which will be passed to the fn function
 \param fn a pointer to the function which will be run for each vector item.
 *      The iteration will stop if the fn function return non zero value.
 */
CI_DECLARE_FUNC(void) ci_vector_iterate(const ci_vector_t *vector, void *data, int (*fn)(void *data, const void *));

/**
 * Delete the last element of a vector.
 \ingroup VECTORS
 \param vector a pointer to the ci_vector_t object
 \return a pointer to the popped vector item on success, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_vector_pop(ci_vector_t *vector);

/**
 \def ci_vector_get(vector, i)
 \ingroup VECTORS
 * Return a pointer to the i item of the vector
 */
#define ci_vector_get(vector, i) (i < vector->count ? (const void *)vector->items[i]:  (const void *)NULL)


CI_DECLARE_FUNC(const void **) ci_vector_cast_to_voidvoid(ci_vector_t *vector);
CI_DECLARE_FUNC(ci_vector_t *)ci_vector_cast_from_voidvoid(const void **p);

/**
 \defgroup STR_VECTORS  Vectors of strings
 \ingroup VECTORS
 *
 */

/**
 \typedef ci_str_vector_t
 \ingroup STR_VECTORS
 * The ci_str_vector is used to implement string vectors.
 * The  ci_str_vector_create, ci_str_vector_destroy,  ci_str_vector_add,
 * and ci_str_vector_pop defines are similar and equivalent to the ci_vector_*
 * functions.
 */
typedef ci_vector_t ci_str_vector_t;
#define ci_str_vector_create ci_vector_create
#define ci_str_vector_destroy ci_vector_destroy
#define ci_str_vector_add(vect, string) ((const char *)ci_vector_add((ci_vector_t *)(vect), string, (strlen(string)+1)))
#define ci_str_vector_get(vector, i) (i < vector->count ? (const char *)vector->items[i]:  (const char *)NULL)
#define ci_str_vector_pop(vect)  ((const char *)ci_vector_pop((ci_vector_t *)(vect)))
#define ci_str_vector_cast_to_charchar(vector) ((const char **)ci_vector_cast_to_voidvoid((ci_vector_t *)(vector)))
#define ci_str_vector_cast_from_charchar(p) ((ci_str_vector_t *)ci_vector_cast_from_voidvoid((const void **)p))

/**
 * Run the given function for each string vector item
 \ingroup STR_VECTORS
 \param vector a pointer to the ci_vector_t object
 \param data a pointer to data which will be passed to the fn function
 \param fn a pointer to the function which will be run for each string vector
 *      item. The iteration will stop if the fn function return non zero value.
 */
CI_DECLARE_FUNC(void) ci_str_vector_iterate(const ci_str_vector_t *vector, void *data, int (*fn)(void *data, const char *));

/**
 * Search for a string in a string vector.
 \ingroup STR_VECTORS
 \param vector a pointer to the ci_vector_t object
 \param str the string to search for
 \return a pointer to the new  item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const char *) ci_str_vector_search(ci_str_vector_t *vector, const char *str);

/**
 \defgroup PTR_VECTORS  Vectors of pointers
 \ingroup VECTORS
 */

/**
 \typedef ci_ptr_vector_t
 \ingroup PTR_VECTORS
 * The ci_ptr_vector is used to implement vectors storing pointers.
 * The ci_ptr_vector_create, ci_ptr_vector_destroy, ci_ptr_vector_iterate,
 * and ci_ptr_vector_get defines are similar and equivalent to the ci_vector_* functions.
 */
typedef ci_vector_t ci_ptr_vector_t;
#define ci_ptr_vector_create ci_vector_create
#define ci_ptr_vector_destroy ci_vector_destroy
#define ci_ptr_vector_iterate ci_vector_iterate
#define ci_ptr_vector_get ci_vector_get

/**
 * Add an  item to the vector.
 \ingroup PTR_VECTORS
 \param vector a pointer to the ci_vector_t object
 \param pointer the pointer to store in vector
 \return a pointer to the new  item on success, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_ptr_vector_add(ci_vector_t *vector, void *pointer);

/**
 \defgroup LISTS Lists API
 \ingroup ARRAYS
 * Lists for storing items, and can grow unlimited.
 *
 */

typedef struct ci_list_item {
    void *item;
    struct ci_list_item *next;
} ci_list_item_t;

/**
 \typedef ci_list_t
 \ingroup LISTS
 * The ci_list_t objects can store a list of objects, with a predefined size.
 * The list items can be removed.
 * The memory RAM space of list can not be decreased before the
 * ci_list destroyed. However the memory of removed items reused.
 */
typedef struct ci_list {
    ci_list_item_t *items;
    ci_list_item_t *last;
    ci_list_item_t *trash;
    ci_list_item_t *cursor;
    ci_list_item_t *tmp;
    size_t obj_size;
    ci_mem_allocator_t *alloc;

    int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size);
    int (*copy_func)(void *newObj, const void *oldObj);
    void (*free_func)(void *obj);
} ci_list_t;

/**
 * Allocate the required memory and initialize a ci_list_t object
 \ingroup LISTS
 \param init_size the initial memory size to use
 \param obj_size the size of stored objects. If it is 0 then stores pointers
 *      to objects.
 \return the allocated object on success, or NULL on failure
 */
CI_DECLARE_FUNC(ci_list_t *) ci_list_create(size_t init_size, size_t obj_size);

/**
 * Gets the first item of the list and updates the list cursor to the next item.
 * \n WARNING: do not mix this macro with ci_list_iterate. Use the ci_list_head
 *    and ci_list_tail macros instead
 * \n WARNING: It is a non-reentrant function.
 *
 \ingroup LISTS
 \param list  a pointer to the ci_list_t object
 \return The first item if exist, NULL otherwise
 */
static inline void *ci_list_first(ci_list_t *list) {
    if (list && list->items) {
        list->cursor = list->items->next;
        return list->items->item;
    }
    return NULL;
}

/**
 * Return the next item of the list and updates the list cursor to the next
 * item.
 * \n WARNING: It does not check for valid list object.
 * \n WARNING: do not mix this macro with ci_list_iterate!
 * \n WARNING: It is a non-reentrant function.
 \ingroup LISTS
 \param list  a pointer to the ci_list_t object
 \return The next item if exist, NULL otherwise
*/
static inline void *ci_list_next(ci_list_t *list) {
    if ((list->tmp = list->cursor) != NULL) {
        list->cursor = list->cursor->next;
        return list->tmp->item;
    }
    return NULL;
}

/**
 \typedef ci_list_iterator_t
 \ingroup LISTS
 * An object which is used to iterate over static ci_list_t objects.
 */
typedef ci_list_item_t *  ci_list_iterator_t;


/**
 * Updates the passed ci_list_iterator_t object to point to the first list item.
 *
 * The ci_list_iterator_* family functions can not be used to update
 * (add/remove) list items, nor in cases where the list is updated while
 * we are accessing it.
 \ingroup LISTS
 \param list  a pointer to the ci_list_t object
 \param it an iterator object to use to iterate
 \return The first item if exist, NULL otherwise
 */
static inline void *ci_list_iterator_first(const ci_list_t *list, ci_list_iterator_t *it) {
    assert(list);
    assert(it);
    if ((*it = list->items))
        return (*it)->item;
    return NULL;
}

/**
 * Updates the passed ci_list_iterator_t object to point to the next item.
 *
 * The ci_list_iterator_* family functions can not be used to update
 * (add/remove) list items, nor in cases where the list is updated while
 * we are accessing it.
 \ingroup LISTS
 \param it the iterator object
 \return the current item if exist, NULL otherwise
 */
static inline void *ci_list_iterator_next(ci_list_iterator_t *it) {
    assert(it);
    if ((*it) && (*it = ((*it)->next)))
        return (*it)->item;
    return NULL;
}

/**
 * Retrieves the list item where the given iterator points.
 *
 \ingroup LISTS
 */
static inline void *ci_list_iterator_value(const ci_list_iterator_t *it) {
    assert(it);
    return (*it) ? (*it)->item : NULL;
}

/**
 \ingroup LISTS
 * Return the head of the list
 */
static inline void *ci_list_head(const ci_list_t *list) {
    return (list && list->items != NULL ? list->items->item : NULL);
}

/**
 \ingroup LISTS
 * Return last item of the list.
 */
static inline void *ci_list_tail(const ci_list_t *list) {
    return (list && list->last != NULL ? list->last->item : NULL);
}

/**
 * Destroy an ci_list_t object
 \ingroup LISTS
 \param list a pointer to ci_list_t object to be destroyed
 */
CI_DECLARE_FUNC(void) ci_list_destroy(ci_list_t *list);

/**
 * Run the given function for each list item
 * \n WARNING: It is a non-reentrant function.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param data a pointer to data which will be passed to the fn function
 \param fn a pointer to the function which will be run for each vector item.
 *      The iteration will stop if the fn function return non zero value.
 */
CI_DECLARE_FUNC(void) ci_list_iterate(ci_list_t *list, void *data, int (*fn)(void *data, const void *obj));

/**
 * Add an item to the head of list.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to the object to add in vector
 \return a pointer to the new  item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_list_push(ci_list_t *list, const void *obj);

/**
 * Add an item to the tail of list.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to the object to add in vector
 \return a pointer to the new  item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_list_push_back(ci_list_t *list, const void *data);

/**
 * Remove the first item of the list.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to store removed item
 \return a pointer to the obj on success, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_list_pop(ci_list_t *list, void *obj);

/**
 * Remove the last item of the list.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to store removed item
 \return a pointer to the obj on success, NULL otherwise
 */
CI_DECLARE_FUNC(void *) ci_list_pop_back(ci_list_t *list, void *obj);

/**
 * Remove the first found item equal to the obj.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to remove
 \return not 0 on success, 0 otherwise
 */
CI_DECLARE_FUNC(int) ci_list_remove(ci_list_t *list, const void *obj);

/**
 * Remove the first found item equal to the obj using the
 * cmp_func as comparison function.
 * The cmp_func should return 0 if the objects are equal, non-zero
 * otherwise.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to remove
 \param cmp_func the comparison function to use.
 \return not 0 on success, 0 otherwise
 */
CI_DECLARE_FUNC(int) ci_list_remove2(ci_list_t *list, const void *obj, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size));

/**
 * Return the first found item equal to the obj.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to remove
 \return the found item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_list_search(const ci_list_t *list, const void *data);

/**
 * Return the first found item equal to the obj, using the cmp_func as
 * comparison function.
 * The cmp_func should return 0 if the objects are equal, non-zero
 * otherwise.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param obj pointer to an object to remove
 \param cmp_func the comparison function to use
 \return the found item on success, NULL otherwise
 */
CI_DECLARE_FUNC(const void *) ci_list_search2(const ci_list_t *list, const void *data, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size));

/**
 * Sorts the list using as compare function the default.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 */
CI_DECLARE_FUNC(void) ci_list_sort(ci_list_t *list);

/**
 * Sorts the list using as compare function the cmp_func.
 \ingroup LISTS
 \param list a pointer to the ci_list_t object
 \param cmp_func the compare function to use
 */
CI_DECLARE_FUNC(void) ci_list_sort2(ci_list_t *list, int (*cmp_func)(const void *obj1, const void *obj2, size_t obj_size));

/*
  The following three functions are undocumented. Probably will be removed or replaced
  by others.
 */
CI_DECLARE_FUNC(void) ci_list_cmp_handler(ci_list_t *list, int (*cmp_func)(const void *obj, const void *user_data, size_t user_data_size));
CI_DECLARE_FUNC(void) ci_list_copy_handler(ci_list_t *list, int (*copy_func)(void *newObj, const void *oldObj));
CI_DECLARE_FUNC(void) ci_list_free_handler(ci_list_t *list, void (*free_func)(void *obj));

#ifdef __cplusplus
}
#endif

#endif /*__C_ICAP_ARRAY_H*/
