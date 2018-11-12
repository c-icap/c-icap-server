/*
 *  Copyright (C) 2004-2018 Christos Tsantilas
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

#ifndef __C_ICAP_FILETYPE_H
#define __C_ICAP_FILETYPE_H

#include "c-icap.h"
#include "request.h"

#ifdef __cplusplus
extern "C"
{
#endif

enum {CI_ASCII_DATA,CI_ISO8859_DATA,CI_XASCII_DATA,CI_UTF_DATA,CI_HTML_DATA,CI_BIN_DATA};
enum {CI_TEXT_DATA,CI_OCTET_DATA};

#define CI_MAGIC_MAX_TYPE_GROUPS 64 /*Maximum number of groups of a single data type*/
#define MAX_GROUPS CI_MAGIC_MAX_TYPE_GROUPS

typedef struct ci_magics_db ci_magics_db_t;
/*low level functions should not used by users*/
CI_DECLARE_FUNC(struct ci_magics_db) *ci_magics_db_build(const char *filename);
CI_DECLARE_FUNC(int) ci_magics_db_file_add(struct ci_magics_db *db,const char *filename);
CI_DECLARE_FUNC(void) ci_magics_db_release(struct ci_magics_db *db);
CI_DECLARE_FUNC(int) ci_magics_db_types_num(const ci_magics_db_t *db);
CI_DECLARE_FUNC(int) ci_magics_db_groups_num(const ci_magics_db_t *db);
CI_DECLARE_FUNC(const char *) ci_magics_db_type_name(const ci_magics_db_t *db, int type);
CI_DECLARE_FUNC(const int *) ci_magics_db_type_groups(const ci_magics_db_t *db,int type);
CI_DECLARE_FUNC(const char *) ci_magics_db_type_descr(const ci_magics_db_t * db,int type);
CI_DECLARE_FUNC(const char *) ci_magics_db_group_name(const ci_magics_db_t *db,int group);
CI_DECLARE_FUNC(int) ci_magics_db_data_type(const struct ci_magics_db *db, const char *buf, int buflen);

/*Deprecated must removed*/
int ci_filetype(struct ci_magics_db *db, const char *buf, int buflen);
/*Deprecated, must removed in future*/
#define ci_magic_types_num(db) (ci_magics_db_types_num(db))
#define ci_magic_groups_num(db) (ci_magics_db_groups_num(db))
#define ci_data_type_name(db,i) (ci_magics_db_type_name(db, i))
#define ci_data_type_groups(db,i) (ci_magics_db_type_groups(db, i))
#define ci_data_type_descr(db,i) (ci_magics_db_type_descr(db, i))
#define ci_data_group_name(db,i) (ci_magics_db_group_name(db, i))

/*And the c-icap Library functions*/

/**
 * Read the magics db from a file and create a ci_magics_db object.
 * \ingroup DATATYPE
 *
 * The user normaly does not need to call this function inside c-icap server.
 * It is not a thread safe function, should called only during icap library
 * initialization before threads started.
 \param filename is the name of the file contains the db
 \return a pointer to a ci_magics_db object
 */
CI_DECLARE_FUNC(struct ci_magics_db) *ci_magic_db_load(const char *filename);

CI_DECLARE_FUNC(void) ci_magic_db_free();

/**
 * Return the type of data of an c-icap request object.
 * \ingroup DATATYPE
 *
 * This function checks the preview data of the request.
 * If the data are encoded this function try to uncompress them before
 * data type recognition
 *
 \param req the c-icap request (ci_request_t) data
 \param isencoded set to CI_ENCODE_GZIP, CI_ENCODE_DEFLATE or
 * CI_ENCODE_UNKNOWN if the data are encoded with an unknown method
 \return the data type or -1 if the data type recognition fails for a reason
 * (eg no preview data, of library not initialized)
 */
CI_DECLARE_FUNC(int) ci_magic_req_data_type(ci_request_t *req, int *isencoded);

CI_DECLARE_FUNC(int) ci_magic_data_type(const char *buf, int buflen);
CI_DECLARE_FUNC(int) ci_magic_data_type_ext(ci_headers_list_t *headers, const char *buf,int len,int *iscompressed);

/**
 * Finds the type id from type name.
 * \ingroup DATATYPE
 *
 \param name is the name of the magic type
 \return the type id
 */
CI_DECLARE_FUNC(int) ci_magic_type_id(const char *name);

/**
 * Finds the group id from group name.
 * \ingroup DATATYPE
 *
 \param group is the name of the group
 \return the group id
 */
CI_DECLARE_FUNC(int) ci_magic_group_id(const char *group);

/**
 * Checks if a magic type belongs to a magic types group.
 * \ingroup DATATYPE
 *
 \param type is the type id to check
 \param group is the group id
 \return non zero if the type belongs to group, zero otherwise
 */
CI_DECLARE_FUNC(int) ci_magic_group_check(int type, int group);


/**
 * The number of types stored in internal magic db.
 * \ingroup DATATYPE
 *
 \return the number of stored magic types
 */
CI_DECLARE_FUNC(int) ci_magic_types_count();

/**
 * The number of groups stored in internal magic db.
 * \ingroup DATATYPE
 *
 \return the number of stored magic groups
 */
CI_DECLARE_FUNC(int) ci_magic_groups_count();

/**
 * Retrieve the name of a magic type.
 * \ingroup DATATYPE
 *
 \param type the type id
 \return the name of the type or NULL if the type does not exists
 */
CI_DECLARE_FUNC(const char *) ci_magic_type_name(int type);

/**
 * Retrieve the short description  of a magic type.
 * \ingroup DATATYPE
 *
 \param type the type id
 \return the short description if the type or NULL if the type does not exists
 */
CI_DECLARE_FUNC(const char *) ci_magic_type_descr(int type);

/**
 * Retrieve the name of a magic types group.
 * \ingroup DATATYPE
 *
 \param group the group id
 \return the name of the group or NULL if the group does not exists
 */
CI_DECLARE_FUNC(const char *) ci_magic_group_name(int group);

/**
 * Retrieve the groups of the given type.
 * \ingroup DATATYPE
 *
 \param type the type id
 \return An array with the group ids, terminated with a '-1'
 */
CI_DECLARE_FUNC(const int*) ci_magic_type_groups(int type);

#ifdef __cplusplus
}
#endif

#endif
