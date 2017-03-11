/*
 *  Copyright (C) 2004-2009 Christos Tsantilas
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

#ifndef __LOOKUP_TABLE_H
#define __LOOKUP_TABLE_H

#include "c-icap.h"
#include "mem.h"
#include "types_ops.h"
#include "array.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup LOOKUPTABLE  Lookup tables api
 \ingroup API
 \brief Macros, functions and structures used to implement and use lookup tables
 *
 * Lookup tables can considered as simple read only databases where the user
 * can search a set of values using a key
 */


struct ci_lookup_table;
struct ci_lookup_table_type {
    void *(*open)(struct ci_lookup_table *table); 
    void  (*close)(struct ci_lookup_table *table);
    void *(*search)(struct ci_lookup_table *table, void *key, void ***vals);
    void  (*release_result)(struct ci_lookup_table *table_data, void **val);
    const void * (*get_row)(struct ci_lookup_table *table, const void *key, const char *columns[], void ***vals);
    char *type;
};

/**
 * \brief The lookup table struct
 * \ingroup LOOKUPTABLE
 */
struct ci_lookup_table {
    void *(*open)(struct ci_lookup_table *table); 
    void  (*close)(struct ci_lookup_table *table);
    void *(*search)(struct ci_lookup_table *table, void *key, void ***vals);
    void  (*release_result)(struct ci_lookup_table *table, void **val);
    const void * (*get_row)(struct ci_lookup_table *table, const void *key, const char *columns[], void ***vals);
    char *type;
    char *path;
    char *args;
    int cols;
    ci_str_vector_t *col_names;
    const ci_type_ops_t *key_ops;
    const ci_type_ops_t *val_ops;
    ci_mem_allocator_t *allocator;
    const struct ci_lookup_table_type *_lt_type;
    void *data;
};

CI_DECLARE_FUNC(struct ci_lookup_table_type *) ci_lookup_table_type_register(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(void) ci_lookup_table_type_unregister(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(const struct ci_lookup_table_type *) ci_lookup_table_type_search(const char *type);

/**
 * \brief Create a lookup table
 * \ingroup LOOKUPTABLE
 * 
 \param table The path of the lookup table (eg file:/etc/c-icap/users.txt or
 *            ldap://hostname/o=base?cn,uid?uid=chtsanti)
 \return A pointer to  a lookup table object
 */
CI_DECLARE_FUNC(struct ci_lookup_table *) ci_lookup_table_create(const char *table);

/**
 * \brief Destroy a lookup table.
 * \ingroup LOOKUPTABLE
 *
 \param lt Pointer to the lookup table will be destroyed.
 */
CI_DECLARE_FUNC(void) ci_lookup_table_destroy(struct ci_lookup_table *lt);

/**
 * \brief Initializes the lookup table.
 *
 * \param table The lookup table object
 */
CI_DECLARE_FUNC(void *) ci_lookup_table_open(struct ci_lookup_table *table); 


/**
 * \brief Search for an object in the lookup table which matches a key.
 *
 * \param table The lookup table object
 * \param key The key value to search for
 * \param vals In this variable stored a 2d array which contains the return
 *            values 
 * \return NULL if none object matches, pointer to the object key value.
 */
CI_DECLARE_FUNC(const char *) ci_lookup_table_search(struct ci_lookup_table *table, const char *key, char ***vals);

/**
 * \brief Releases the data values returned from the search method.
 *
 * \param table The lookup table object
 * \param val The 2d array returned from the search method
 */
CI_DECLARE_FUNC(void)  ci_lookup_table_release_result(struct ci_lookup_table *table, void **val);

/**
 * \brief Search for an object in the lookup table which supports named columns.
 *
 * \param table The lookup table object
 * \param key The key value to search for
 * \param columns NULL terminated array with the names of the columns to
 *                retrieve.
 * \param vals In this variable stored a 2d array which contains the
 *             requested row
 * \return NULL if none object matches, pointer to the object key value.
 */

CI_DECLARE_FUNC(const char *) ci_lookup_table_get_row(struct ci_lookup_table *table, const char *key, const char *columns[], char ***vals);

#ifdef __cplusplus
}
#endif

#endif
