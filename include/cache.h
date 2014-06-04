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

#ifndef __CACHE_H
#define __CACHE_H
#include "hash.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup CACHE cache api
 \ingroup API
 \brief Macros, functions and structures used to implement and use c-icap cache.
 */

struct ci_cache;

/**
 \ingroup CACHE
 \brief A struct which implements a cache type.
 * Modules implement a cache type, needs to implement members of this structure.
 */
typedef struct ci_cache_type {
    int (*init)(struct ci_cache *cache);
    const void *(*search)(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));
    int (*update)(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *cache_buf, const void *val, size_t cache_buf_size));
    void (*destroy)(struct ci_cache *cache);
    const char *name;
} ci_cache_type_t;

/**
 * Register a cache type to c-icap server.
 \ingroup CACHE
 */
CI_DECLARE_FUNC(void) ci_cache_type_register(const struct ci_cache_type *type);

/**
 * The ci_cache_t struct
 \ingroup CACHE
 */
typedef struct ci_cache{
    int (*init)(struct ci_cache *cache);

    // If dup_from_cache is NULL return a ci_buffer object
    const void * (*search)(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));

    // buf is of size val_size and buf_size==val_size 
    int (*update)(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size));
    void (*destroy)(struct ci_cache *cache);

    time_t ttl;
    unsigned int mem_size;
    unsigned int max_object_size;
    unsigned int flags;
    const ci_type_ops_t *key_ops;
    const ci_cache_type_t *_cache_type;
    void *cache_data;
} ci_cache_t;

/**
 * Builds a cache and return a pointer to the ci_cache_t object
 \ingroup CACHE
 \param cache_type The cache type to use. If the cache type not found return a cache object of type "local"
 \param cache_size The size of the cache
 \param max_object_size The maximum object size to store in cache
 \param ttl The ttl value for cached items in this cache
 \param key_ops If not null, the ci_types_ops_t object to use for comparing keys. By default keys are considered as c strings.
 */
CI_DECLARE_FUNC(ci_cache_t *) ci_cache_build( const char *cache_type,
                                              unsigned int cache_size,
                                              unsigned int max_object_size,
                                              int ttl,
                                              const ci_type_ops_t *key_ops
    );

/**
 * Searchs a cache for a stored object
 * If the dup_from_cache parameter is NULL, the returned value must be 
 * released using the ci_buffer_free function.
 \ingroup CACHE
 \param cache Pointer to the ci_cache_t object
 \param key Pointer to the key to search for
 \param val Pointer to store the pointer of returned value
 \param data Pointer to void object which will be passed to dup_from_cache function
 \param dup_from_cache Pointer to function which will be used to allocate memory and copy the stored value.
 */
CI_DECLARE_FUNC(const void *) ci_cache_search(ci_cache_t *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));

/**
 * Stores an object to cache
 \ingroup CACHE
 \param cache Pointer to the ci_cache_t object
 \param key The key of the stored object
 \param val Pointer to the object to be stored
 \param val_size The size of the object to be stored
 \param copy_to_cache The function to use to copy object to cache. If it is NULL the memcpy is used.
 */
CI_DECLARE_FUNC(int) ci_cache_update(ci_cache_t *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size));

/**
 * Destroy a cache_t object
 \ingroup CACHE
 */
CI_DECLARE_FUNC(void) ci_cache_destroy(ci_cache_t *cache);


/*
  Only for internal use only:
  cb functions to store/retrieve vectors from cache....
*/
CI_DECLARE_FUNC(size_t) ci_cache_store_vector_size(ci_vector_t *v);
CI_DECLARE_FUNC(void *) ci_cache_store_vector_val(void *buf, const void *val, size_t buf_size);
CI_DECLARE_FUNC(void *) ci_cache_read_vector_val(const void *val, size_t val_size, void *);


#ifdef __cplusplus
}
#endif

#endif
