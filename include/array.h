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

#ifndef __ARRAY_H
#define __ARRAY_H

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

typedef struct ci_array_item{
    char *name;
    void *value;
    struct ci_array_item *next;
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
   * can grow up to a fixed size, and even if a name/value array item removed
   * its memory can not be free.
 */
typedef struct ci_array {
    ci_array_item_t *items;
    ci_array_item_t *last;
    char *mem;
    size_t max_size;
    ci_mem_allocator_t *alloc;
} ci_array_t;

/**
 * Allocate the required memory and initialize an ci_array_t object 
 \ingroup SIMPLE_ARRAYS
 \return the allocated object on success, or NULL on failure
 *
 */
CI_DECLARE_FUNC(ci_array_t *) ci_array_new(size_t max_size);

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
 \param name the name part of the name/value pair item to be added
 \param value the value part of the name/value pair item to be added
 \param size the size of the value part of the new item.
 \return a pointer to the new array item on success, NULL otherwise
 *
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_array_add(ci_array_t *array, const char *name, const void *value, size_t size);

/**
 * Search in an array for an item with the given name
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param name the item to be search for.
 \return pointer to the value pair of the array item if found, NULL otherwise
 *
 */
CI_DECLARE_FUNC(const void *) ci_array_search(ci_array_t *array, const char *name);

/**
 * Run the given function for each array item
 \ingroup SIMPLE_ARRAYS
 \param array a pointer to the ci_array_t object
 \param data a pointer to data which will be passed on fn function
 \param fn a pointer to the function which will be run for each array item
 */
CI_DECLARE_FUNC(void) ci_array_iterate(ci_array_t *array, void *data, void (*fn)(void *data, const char *name, const void *));
    void ci_array_error();

/**
 \defgroup STR_ARRAYS   Arrays of strings related API
 \ingroup ARRAYS
 * Arrays which store  name/value pair items
*/

/**
 \typedef ci_str_array_t
 \ingroup STR_ARRAYS
 * An alias to the ci_array_t object. It is used to store items with string
 * values to an array.
 * The ci_str_array_new, ci_str_array_destroy, ci_str_array_add, ci_str_array_search
 * and ci_str_array_iterate defines are similar to the equivalent ci_array_* 
 * functions with the required typecasting to work with strings.
 */
typedef ci_array_t ci_str_array_t;
#define ci_str_array_new ci_array_new
#define ci_str_array_destroy ci_array_destroy
#define ci_str_array_add(array, name, value) ci_array_add(array, name, value, strlen(value+1))
#define ci_str_array_search(array, name) (const char *)ci_array_search(array, name)
#define ci_str_array_iterate ci_array_iterate


/**
 \defgroup PTR_ARRAYS  Arrays of pointers
 \ingroup ARRAYS
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
 \def ci_ptr_array_new()
 \ingroup PTR_ARRAYS
 * Create a new ci_ptr_array_t object. Similar to the ci_array_new() function.
 */
#define ci_ptr_array_new ci_array_new

/**
 \def ci_ptr_array_destroy(ptr_array)
 \ingroup PTR_ARRAYS
 * Destroy a ci_ptr_array_t object. Similar to the ci_array_destroy function
 */
#define ci_ptr_array_destroy(ptr_array) ci_array_destroy(ptr_array)

/**
 \def ci_ptr_array_search(ptr_array, name)
 \ingroup PTR_ARRAYS
 * Search for a name/value pair item in a ci_ptr_array_t object. Similar to 
 * the ci_array_search function
 */
#define ci_ptr_array_search(ptr_array, name) ci_array_search(ptr_array, name)

/**
 \def ci_ptr_array_iterate(ptr_array, data, fn)
 \ingroup PTR_ARRAYS
 * Run the function fn for each item of the ci_ptr_array_t object. Similar to
 * the ci_array_iterate function
 */
#define ci_ptr_array_iterate(ptr_array, data, fn) ci_array_iterate(ptr_array, data, fn)

/**
 * Add an name/value pair item to the ci_ptr_array_t object.
 \ingroup PTR_ARRAYS
 \param ptr_array a pointer to the ci_ptr_array_t object
 \ param name the name part of the name/value pair item to be added
 \ param value a pointer to the value part of the name/value pair item to be added
 \return a pointer to the new array item on success, NULL otherwise
 *
 */
CI_DECLARE_FUNC(const ci_array_item_t *) ci_ptr_array_add(ci_ptr_array_t *ptr_array, const char *name, void *value);

/**
 * Pop and delete the first item of a  ci_ptr_array_t object.
 \ingroup PTR_ARRAYS
 \param ptr_array a pointer to the ci_ptr_array_t object
 \param name a pointer to a buffer where the name of the poped item will be store, or NULL
 \param name_size the size of name buffer
 \return a pointer to the value of the popped item
*/
CI_DECLARE_FUNC(void *) ci_ptr_array_pop(ci_ptr_array_t *ptr_array, char *name, size_t name_size);

#ifdef __cplusplus
}
#endif

#endif /*__ARRAY_H*/
