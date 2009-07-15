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

#ifndef __FILETYPE_H
#define __FILETYPE_H

#include "c-icap.h"
#include "request.h"

/**
 \defgroup Api for data/file type recogintion 
 \ingroup API
 * Macros, functions and structures used for data type recognition
 */


#define MAGIC_SIZE 50
#define NAME_SIZE 15
#define DESCR_SIZE 50
#define MAX_GROUPS 64 /*Maximum number of groups of a single data type*/

struct ci_data_type{
     char name[NAME_SIZE+1];
     char descr[DESCR_SIZE+1];
     int groups[MAX_GROUPS];
};

struct ci_data_group{
     char name[NAME_SIZE+1];
     char descr[DESCR_SIZE+1];
};


typedef struct ci_magic{
     int offset;
     unsigned char magic[MAGIC_SIZE+1];
     size_t len;
     unsigned int type;
} ci_magic_t;

#define DECLARE_ARRAY(array,type) type *array;int array##_num;int array##_size;

struct ci_magics_db{
     DECLARE_ARRAY(types,struct ci_data_type)
     DECLARE_ARRAY(groups,struct ci_data_group)
     DECLARE_ARRAY(magics,struct ci_magic)
};

#define ci_magic_types_num(db) (db!=NULL?db->types_num:0)
#define ci_magic_groups_num(db)(db!=NULL?db->groups_num:0)
#define ci_data_type_name(db,i)(db!=NULL?db->types[i].name:NULL)
#define ci_data_type_groups(db,i)(db!=NULL?db->types[i].groups:NULL)
#define ci_data_type_descr(db,i)(db!=NULL?db->types[i].descr:NULL)
#define ci_data_group_name(db,i)(db!=NULL?db->groups[i].name:NULL)

enum {CI_ASCII_DATA,CI_ISO8859_DATA,CI_XASCII_DATA,CI_UTF_DATA,CI_HTML_DATA,CI_BIN_DATA};
enum {CI_TEXT_DATA,CI_OCTET_DATA};
enum {CI_ENCODE_NONE=0,CI_ENCODE_GZIP,CI_ENCODE_DEFLATE,CI_ENCODE_UNKNOWN};

/*low level functions should not used by users*/
CI_DECLARE_FUNC(struct ci_magics_db) *ci_magics_db_build(char *filename);
CI_DECLARE_FUNC(int) ci_magics_db_file_add(struct ci_magics_db *db,char *filename);
CI_DECLARE_FUNC(int) ci_get_data_type_id(struct ci_magics_db *db,char *name);
CI_DECLARE_FUNC(int) ci_get_data_group_id(struct ci_magics_db *db,char *group);
CI_DECLARE_FUNC(int) ci_belongs_to_group(struct ci_magics_db *db, int type, int group);

CI_DECLARE_FUNC(int) ci_filetype(struct ci_magics_db *db,char *buf, int buflen);
CI_DECLARE_FUNC(int) ci_extend_filetype(struct ci_magics_db *db,
					ci_request_t *req,
					char *buf,int len,int *iscompressed);

/*And the c-icap Library functions*/

/**
 * Read the magics db from a file and create a ci_magics_db object
 *
 * The user normaly does not need to call this function inside c-icap server. 
 * It is not a thread safe function, should called only during icap library 
 * initialization before threads started.
 \param filename is the name of the file contains the db
 \return a pointer to a ci_magics_db object
 */
CI_DECLARE_FUNC(struct ci_magics_db) *ci_magic_db_load(char *filename);

/**
 * Recognizes the type of (preview) data of an c-icap request object
 *
 * If the data are encoded this function try to uncompress them 
 * to examine its data type
 *
 \param req the c-icap request (ci_request_t) data
 \param isencoded set to CI_ENCODE_GZIP, CI_ENCODE_DEFLATE or 
 * CI_ENCODE_UNKNOWN if the data are encoded with an unknown method
 \return the data type or -1 if the data type recognition fails for a reason 
 * (eg no preview data, of library not initialized)
 */
CI_DECLARE_FUNC(int) ci_magic_req_data_type(ci_request_t *req, int *isencoded);

CI_DECLARE_FUNC(int) ci_magic_data_type(char *buf, int buflen);
CI_DECLARE_FUNC(int) ci_magic_data_type_ext(ci_headers_list_t *headers, char *buf,int len,int *iscompressed);

/**
 * Finds the type id from type name
 *
 \param name is the name of the magic type
 \return the type id
 */
CI_DECLARE_FUNC(int) ci_magic_type_id(char *name);

/**
 * Finds the group id from group name
 *
 \param name is the name of the group
 \return the group id
 */
CI_DECLARE_FUNC(int) ci_magic_group_id(char *group);

/**
 * Checks if a magic type belongs to a magic types group
 *
 \param type is the type id to check
 \param group is the group id
 \return non zero if the type belongs to group, zero otherwise
 */
CI_DECLARE_FUNC(int) ci_magic_group_check(int type, int group);


/**
 * The number of types stored in internal magic db
 *
 \return the number of stored magic types
 */
CI_DECLARE_FUNC(int) ci_magic_types_count();

/**
 * The number of groups stored in internal magic db
 *
 \return the number of stored magic groups
 */
CI_DECLARE_FUNC(int) ci_magic_groups_count();

/**
 * Retrieve the name of a magic type
 *
 \param type the type id
 \return the name of the type or NULL if the type does not exists
 */
CI_DECLARE_FUNC(char *) ci_magic_type_name(int type);

/**
 * Retrieve the short description  of a magic type
 *
 \param type the type id
 \return the short description if the type or NULL if the type does not exists
 */
CI_DECLARE_FUNC(char *) ci_magic_type_descr(int type);

/**
 * Retrieve the name of a magic types group
 *
 \param type the group id
 \return the name of the group or NULL if the group does not exists
 */
CI_DECLARE_FUNC(char *) ci_magic_group_name(int group);

#endif
