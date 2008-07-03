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


CI_DECLARE_FUNC(struct ci_magics_db) *ci_magics_db_build(char *filename);
CI_DECLARE_FUNC(int) ci_magics_db_file_add(struct ci_magics_db *db,char *filename);
CI_DECLARE_FUNC(int) ci_get_data_type_id(struct ci_magics_db *db,char *name);
CI_DECLARE_FUNC(int) ci_get_data_group_id(struct ci_magics_db *db,char *group);

CI_DECLARE_FUNC(int) ci_filetype(struct ci_magics_db *db,char *buf, int buflen);
/* compiler does not allow me to define this function here :-(
   because of ci_request_t .......
CI_DECLARE_FUNC(int) ci_extend_filetype(struct ci_magics_db *db,
					ci_request_t *req,
					char *buf,int len,int *iscompressed);
*/
#endif
