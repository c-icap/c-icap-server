#ifndef __FILETYPE_H
#define __FILETYPE_H

#include "c-icap.h"

#define MAGIC_SIZE 50
#define NAME_SIZE 15
#define DESCR_SIZE 50

struct ci_data_type{
     char name[NAME_SIZE+1];
     char descr[DESCR_SIZE+1];
     unsigned int group;
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
#define ci_data_type_group(db,i)(db!=NULL?db->types[i].group:-1)
#define ci_data_type_descr(db,i)(db!=NULL?db->types[i].descr:NULL)
#define ci_data_group_name(db,i)(db!=NULL?db->groups[i].name:NULL)

enum {CI_ASCII_DATA,CI_ISO8859_DATA,CI_XASCII_DATA,CI_UTF_DATA,CI_HTML_DATA,CI_BIN_DATA};
enum {CI_TEXT_DATA,CI_OCTET_DATA};

CI_DECLARE_FUNC(struct ci_magics_db) *ci_magics_db_build(char *filename);
CI_DECLARE_FUNC(int) ci_magics_db_file_add(struct ci_magics_db *db,char *filename);
CI_DECLARE_FUNC(int) ci_get_data_type_id(struct ci_magics_db *db,char *name);
CI_DECLARE_FUNC(int) ci_get_data_group_id(struct ci_magics_db *db,char *group);

CI_DECLARE_FUNC(int) ci_filetype(struct ci_magics_db *db,char *buf, int buflen);

#endif
