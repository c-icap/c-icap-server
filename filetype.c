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

#include "common.h"
#include "c-icap.h"
#include "util.h"
#include "debug.h"
#include "encoding.h"
#include "request.h"
#include "request_util.h"
#include "mem.h"
#include "filetype.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define MAGIC_SIZE 64
#define NAME_SIZE 31
#define DESCR_SIZE 63

struct ci_data_type {
    char name[NAME_SIZE+1];
    char descr[DESCR_SIZE+1];
    int groups[CI_MAGIC_MAX_TYPE_GROUPS];
};

struct ci_data_group {
    char name[NAME_SIZE+1];
    char descr[DESCR_SIZE+1];
};

struct ci_magic_block {
    int offset;
    unsigned char magic[MAGIC_SIZE+1];
    size_t len;
};

typedef struct ci_magic {
    unsigned int type;
    int blocks_num;
    struct ci_magic_block blocks[];
} ci_magic_t;

#define MAX_MAGIC_BLOCKS 64
struct ci_magic_record {
    struct ci_magic_block blocks[MAX_MAGIC_BLOCKS];
    int blocks_num;
    char type[NAME_SIZE + 1];
    char *groups[CI_MAGIC_MAX_TYPE_GROUPS + 1];
    char descr[DESCR_SIZE + 1];
};

#define DECLARE_ARRAY(array,type) type *array;int array##_num;int array##_size;

struct ci_magics_db {
    DECLARE_ARRAY(types,struct ci_data_type)
    DECLARE_ARRAY(groups,struct ci_data_group)
    DECLARE_ARRAY(magics,struct ci_magic *)
};

static struct ci_magics_db *_MAGIC_DB = NULL;

struct ci_data_type predefined_types[] = {
    {"ASCII", "ASCII text file", {CI_TEXT_DATA, -1}},
    {"ISO-8859", "ISO-8859 text file", {CI_TEXT_DATA, -1}},
    {"EXT-ASCII", "Extended ASCII (Mac,IBM PC etc.)", {CI_TEXT_DATA, -1}},
    {"UTF", "Unicode text file ", {CI_TEXT_DATA, -1}},
    {"HTML", "HTML text", {CI_TEXT_DATA, -1}},
    {"BINARY", "Unknown data", {CI_OCTET_DATA, -1}},
    {"", "", {-1}}
};

struct ci_data_group predefined_groups[] = {
    {"TEXT", "All texts"},
    {"DATA", "Undefined data type"},
    {"", ""}
};

#define DECLARE_ARRAY_FUNCTIONS(structure,array,type,size) int array##_init(structure *db){ \
                                                     if((db->array=malloc(size*sizeof(type)))==NULL) \
                                                      return 0; \
                                                     db->array##_num=0; \
                                                     db->array##_size=size;\
                                                     return 1; \
                                                    }

#define CHECK_SIZE(db,array,type,size)   if(db->array##_num >= db->array##_size){\
                                               if((newdata=realloc(db->array,(db->array##_size+size)*sizeof(type)))==NULL)\
                                                         return -1;\
                                                    db->array##_size +=size; \
                                                db->array =newdata;\
                                        }

DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db, types, struct ci_data_type, 50)
DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db, groups, struct ci_data_group, 15)
DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db, magics, struct ci_magic *, 50)


int types_add(struct ci_magics_db *db, const char *name, const char *descr, int *groups)
{
    struct ci_data_type *newdata;
    int indx, i;

    CHECK_SIZE(db, types, struct ci_data_type, 50);
    indx = db->types_num;
    db->types_num++;
    strncpy(db->types[indx].name, name, NAME_SIZE);
    db->types[indx].name[NAME_SIZE] = '\0';
    strncpy(db->types[indx].descr, descr, DESCR_SIZE);
    db->types[indx].descr[DESCR_SIZE] = '\0';
    i = 0;
    while (groups[i] >= 0 && i < CI_MAGIC_MAX_TYPE_GROUPS) {
        db->types[indx].groups[i] = groups[i];
        i++;
    }
    db->types[indx].groups[i] = -1;
    return indx;
}

int groups_add(struct ci_magics_db *db, const char *name, const char *descr)
{
    struct ci_data_group *newdata;
    int indx;

    CHECK_SIZE(db, groups, struct ci_data_group, 15);
    indx = db->groups_num;
    db->groups_num++;
    strncpy(db->groups[indx].name, name, NAME_SIZE);
    db->groups[indx].name[NAME_SIZE] = '\0';
    strncpy(db->groups[indx].descr, descr, DESCR_SIZE);
    db->groups[indx].descr[DESCR_SIZE] = '\0';
    return indx;
}

static int magic_add(struct ci_magics_db *db, int type,const struct ci_magic_block *blocks, int blocks_num)
{
    struct ci_magic **newdata;
    int indx;
    CHECK_SIZE(db, magics, struct ci_magic *, 50);
    indx = db->magics_num;
    /*The ci_magic struct plus space for the ci_magic_blocks array (the ci_magic::blocks flexible array)*/
    db->magics[indx] = malloc(sizeof(struct ci_magic) + blocks_num * sizeof(struct ci_magic_block));
    if (db->magics[indx]) {
        db->magics[indx]->type = type;
        memcpy(db->magics[indx]->blocks, blocks, blocks_num * sizeof(struct ci_magic_block));
        db->magics[indx]->blocks_num = blocks_num;
        db->magics_num++;
        return indx;
    }
    return -1;
}

static int ci_get_data_type_id(struct ci_magics_db *db, const char *name)
{
    int i = 0;
    for (i = 0; i < db->types_num; i++) {
        if (strcasecmp(name, db->types[i].name) == 0)
            return i;
    }
    return -1;
}

static int ci_get_data_group_id(struct ci_magics_db *db, const char *group)
{
    int i = 0;
    for (i = 0; i < db->groups_num; i++) {
        if (strcasecmp(group, db->groups[i].name) == 0)
            return i;
    }
    return -1;
}

static int ci_belongs_to_group(struct ci_magics_db *db, int type, int group)
{
    int i;
    if (db->types_num < type)
        return 0;
    i = 0;
    while (db->types[type].groups[i] >= 0 && i < CI_MAGIC_MAX_TYPE_GROUPS) {
        if (db->types[type].groups[i] == group)
            return 1;
        i++;
    }
    return 0;
}

static void reset_magic_record(struct ci_magic_record *record)
{
    int i;
    i = 0;
    while (record->groups[i] != NULL) {
        free(record->groups[i]);
        record->groups[i] = NULL;
        i++;
    }
    memset(record, 0, sizeof(struct ci_magic_record));
}

#define RECORD_LINE 32768
static int parse_record(char *line, struct ci_magic_record *record)
{
    char *s, *end;
    char num[4];
    int len, c, i;

    if ((len = strlen(line)) == 0) /*Empty line*/
        return 0;

    if (line[0] == '#') /*Comment ....... */
        return 0;

    if (record->blocks_num == MAX_MAGIC_BLOCKS)
        return -1;

    s = line;
    errno = 0;
    record->blocks[record->blocks_num].offset = strtol(s, &end, 10);
    while (*end && isspace((int)*end)) ++end;
    if (*end != ':' || errno != 0)
        return -1;

    s = end + 1;
    i = 0;
    end = line + len;
    while (*s != ':' && s < end && i < MAGIC_SIZE) {
        if (*s == '\\') {
            s++;
            c = -1;
            if (*s == 'x' && (end - s) >= 2) {
                s++;
                num[0] = *(s++);
                num[1] = *(s++);
                num[2] = '\0';
                c = strtol(num, NULL, 16);
            } else if ((end - s) >= 3) {
                num[0] = *(s++);
                num[1] = *(s++);
                num[2] = *(s++);
                num[3] = '\0';
                c = strtol(num, NULL, 8);
            }
            if (c > 256 || c < 0) {
                return -1;
            }
            record->blocks[record->blocks_num].magic[i++] = c;
        } else {
            record->blocks[record->blocks_num].magic[i++] = *s;
            s++;
        }
    }
    record->blocks[record->blocks_num].len = i;
    ++record->blocks_num;
    while (*s && isspace((int)*s)) ++s;
    if (s >= end || *s != ':') {
        /*Unexpected end of the line, parse error */
        return -1;
    }
    ++s;
    if (*s == '\\' && *(s+1) == '\0') {
        /*Multiline record, continue to the next line */
        return 0;
    }

    if ((end = strchr(s, ':')) == NULL) {
        return -1;
    }
    *end = '\0';
    ci_str_trim(s);
    strncpy(record->type, s, NAME_SIZE);
    record->type[NAME_SIZE] = '\0';
    s = end + 1;

    if ((end = strchr(s, ':')) == NULL) {
        return -1;
    }
    *end = '\0';
    ci_str_trim(s);
    strncpy(record->descr, s, DESCR_SIZE);
    record->descr[DESCR_SIZE] = '\0';

    s = end + 1;
    i = 0;
    while ((end = strchr(s, ':')) != NULL) {
        *end = '\0';
        record->groups[i] = malloc(NAME_SIZE + 1);
        ci_str_trim(s);
        strncpy(record->groups[i], s, NAME_SIZE);
        record->groups[i][NAME_SIZE] = '\0';
        i++;
        if (i >= CI_MAGIC_MAX_TYPE_GROUPS - 1)
            break;
        s = end + 1;
    }

    record->groups[i] = malloc(NAME_SIZE + 1);
    ci_str_trim(s);
    strncpy(record->groups[i], s, NAME_SIZE);
    record->groups[i][NAME_SIZE] = '\0';
    i++;
    record->groups[i] = NULL;
    return 1;
}

struct ci_magics_db *ci_magics_db_init()
{
    struct ci_magics_db *db;
    int i, ret;
    db = malloc(sizeof(struct ci_magics_db));
    if (!db)
        return NULL;

    memset(db, 0, sizeof(struct ci_magics_db));
    ret = types_init(db) && groups_init(db) && magics_init(db);
    if (!ret) {
        ci_magics_db_release(db);
        return NULL;
    }

    i = 0;                     /*Copy predefined types */
    while (predefined_types[i].name[0] != '\0') {
        ret = types_add(db, predefined_types[i].name, predefined_types[i].descr,
                        predefined_types[i].groups);
        if (ret < 0) { /*memory allocation ?*/
            ci_magics_db_release(db);
            return NULL;
        }
        i++;
    }

    i = 0;                     /*Copy predefined groups */
    while (predefined_groups[i].name[0] != '\0') {
        ret = groups_add(db, predefined_groups[i].name, predefined_groups[i].descr);
        if (ret < 0) { /*memory allocation ?*/
            ci_magics_db_release(db);
            return NULL;
        }
        i++;
    }
    return db;
}

void ci_magics_db_release(struct ci_magics_db *db)
{
    int i;
    if (db->types)
        free(db->types);
    if (db->groups)
        free(db->groups);
    if (db->magics) {
        for (i = 0; i < db->magics_num; ++i)
            free(db->magics[i]);
        free(db->magics);
    }
    free(db);
}

int ci_magics_db_file_add(struct ci_magics_db *db, const char *filename)
{
    int type;
    int ret, error, group, i, lineNum;
    int groups[CI_MAGIC_MAX_TYPE_GROUPS + 1];
    char line[RECORD_LINE];
    struct ci_magic_record record;
    FILE *f;

    if ((f = fopen(filename, "r")) == NULL) {
        ci_debug_printf(1, "Error opening magic file: %s\n", filename);
        return 0;
    }

    memset(&record, 0, sizeof(struct ci_magic_record));
    lineNum = 0;
    error = 0;
    while (!error && fgets(line, RECORD_LINE, f) != NULL) {
        lineNum++;
        ci_str_trim(line);
        ret = parse_record(line, &record);
        if (!ret)
            continue;
        if (ret < 0) {
            error = 1;
            break;
        }
        if ((type = ci_get_data_type_id(db, record.type)) < 0) {
            for (i=0; record.groups[i] != NULL && !error && i < CI_MAGIC_MAX_TYPE_GROUPS; ++i) {
                if ((group = ci_get_data_group_id(db, record.groups[i])) < 0) {
                    group = groups_add(db, record.groups[i], "");
                }
                groups[i] = group;
                if (group < 0)
                    error = 1;
            }
            if (!error) {
                groups[i] = -1;
                type = types_add(db, record.type, record.descr, groups);
                if (type < 0)
                    error = 1;
            }
        }

        if (magic_add(db, (unsigned int) type, record.blocks, record.blocks_num) < 0)
            error = 1;

        reset_magic_record(&record);
    }
    fclose(f);
    if (error) {            /*An error occured ..... */
        ci_debug_printf(1, "Error reading magic file (%d), line number: %d\nBuggy line: %s\n", ret, lineNum, line);
        return 0;
    }
    ci_debug_printf(3, "In database: magic: %d, types: %d, groups: %d\n",
                    db->magics_num, db->types_num, db->groups_num);
    return 1;


}

struct ci_magics_db *ci_magics_db_build(const char *filename)
{
    struct ci_magics_db *db;


    if ((db = ci_magics_db_init()) != NULL)
        ci_magics_db_file_add(db, filename);
    return db;
}

int ci_magics_db_types_num(const ci_magics_db_t *db)
{
    return (db != NULL ? db->types_num : 0);
}

int ci_magics_db_groups_num(const ci_magics_db_t *db)
{
    return (db != NULL ? db->groups_num:0);
}

const char *ci_magics_db_type_name(const ci_magics_db_t *db, int i)
{
    return (db != NULL && i < db->types_num && i >= 0 ? db->types[i].name : NULL);
}

const int *ci_magics_db_type_groups(const ci_magics_db_t *db,int i)
{
    return (db != NULL && i < db->types_num && i >= 0 ? db->types[i].groups : NULL);
}

const char *ci_magics_db_type_descr(const ci_magics_db_t * db,int i)
{
    return (db != NULL && i < db->types_num && i >= 0 ? db->types[i].descr : NULL);
}

const char *ci_magics_db_group_name(const ci_magics_db_t *db,int i)
{
    return (db != NULL && i < db->groups_num && i >= 0 ? db->groups[i].name : NULL);
}


static int check_magics(const struct ci_magics_db *db, const char *buf, int buflen)
{
    int i, j;
    int matched;
    const char *test;
    int required;
    for (i = 0; i < db->magics_num; i++) {
        matched = 0;
        for (j = 0; j < db->magics[i]->blocks_num; ++j) {
            test = buf + db->magics[i]->blocks[j].offset;
            required = db->magics[j]->blocks[j].offset + db->magics[i]->blocks[j].len;
            if (buflen >= required &&
                    memcmp(test, db->magics[i]->blocks[j].magic, db->magics[i]->blocks[j].len) == 0)
                ++matched;
            else
                break;
        }
        if (matched == db->magics[i]->blocks_num)
            return db->magics[i]->type;
    }
    return -1;
}

/*The folowing table taking from the file project........*/

/*0 are the characters which never appears in text */
#define T 1                     /* character appears in plain ASCII text */
#define I 2                     /* character appears in ISO-8859 text */
#define X 4                     /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static const char text_chars[256] = {
    /*                  BEL BS HT LF    FF CR    */
    0, 0, 0, 0, 0, 0, 0, T, T, T, T, 0, T, T, 0, 0,    /* 0x0X */
    /*                              ESC          */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, T, 0, 0, 0, 0,    /* 0x1X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,    /* 0x2X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,    /* 0x3X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,    /* 0x4X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,    /* 0x5X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,    /* 0x6X */
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, 0,    /* 0x7X */
    /*            NEL                            */
    X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,    /* 0x8X */
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,    /* 0x9X */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,    /* 0xaX */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,    /* 0xbX */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,    /* 0xcX */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,    /* 0xdX */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,    /* 0xeX */
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I     /* 0xfX */
};


/*
ASCII if res <= 1
ISO if res <= 3
EXTEND if res <= 7
*/

static int check_ascii(unsigned char *buf, int buflen)
{
    unsigned int i, res = 0, type;
    for (i = 0; i < buflen; i++) {     /*May be only a small number (30-50 bytes) of the first data must be checked */
        if ((type = text_chars[buf[i]]) == 0)
            return -1;
        res = res | type;
    }
    if (res <= 1)
        return CI_ASCII_DATA;
    if (res <= 3)
        return CI_ISO8859_DATA;

    return CI_XASCII_DATA; /*Extend ascii for web pages? */
}


static unsigned int utf_boundaries[] =
{ 0x0, 0x0, 0x07F, 0x7FF, 0xFFFF, 0x1FFFFF, 0x3FFFFFF };

static int isUTF8(unsigned char *c, int size)
{
    int i, r_size = 0;
    unsigned int ucs_c = 0;

    if (text_chars[(int) *c] == T)
        return 1;

    if ((*c & 0xE0) == 0xC0) { /*2 byte unicode char ... */
        ucs_c = (*c) & 0x1F;
        r_size = 2;
    } else if ((*c & 0xF0) == 0xE0) {  /*3 byte unicode char */
        ucs_c = (*c) & 0x0F;
        r_size = 3;
    } else if ((*c & 0xF8) == 0xF0) {  /*4 byte unicode char */
        ucs_c = (*c) & 0x07;
        r_size = 4;
    } else if ((*c & 0xFC) == 0xF8) {  /*5 byte unicode char */
        ucs_c = (*c) & 0x03;
        r_size = 5;
    } else if ((*c & 0xFE) == 0xFC) {  /*6 byte unicode char */
        ucs_c = (*c) & 0x01;
        r_size = 6;
    }

    if (!r_size /*|| r_size >4 */ )    /*In practice there are not yet 5 and 6 sized utf characters */
        return 0;

    for (i = 1; i < r_size && i < size; i++) {
        if ((*(c + i) & 0xC0) != 0x80)
            return 0;
        ucs_c = (ucs_c << 6) | (*(c + i) & 0x3F);
    }

    if (i < r_size) {
        /*Not enough length ... */
        return -1;
    }

    if (ucs_c <= utf_boundaries[r_size]) {
        /*Over long character .... */
        return 0;
    }

    /* check UTF-16 surrogates? ........ */
    if ((ucs_c >= 0xd800 && ucs_c <= 0xdfff) || ucs_c == 0xfffe
            || ucs_c == 0xffff) {
        return 0;
    }
    return r_size;
}

static int check_unicode(unsigned char *buf, int buflen)
{
    int i, ret = 0;
    int endian = 0;
    /*check for utf8 ........ */
    for (i = 0; i < buflen; i += ret) {
        if ((ret = isUTF8(buf + i, buflen - i)) <= 0)
            break;
    }

    if (ret < 0 && i == 0)
        ret = 0;              /*Not enough data to check */

    if (ret)                   /*Even if the last char is unknown ret != 0 mean is utf */
        return CI_UTF_DATA;   /*... but what about if buflen is about 2 or 3 bytes long ? */

    /*check for utf16 .... */
    if (buflen < 2)
        return -1;
    /*I read somewhere that only Microsoft uses the first 2 bytes to identify utf16 documents */
    if (buf[0] == 0xff && buf[1] == 0xfe)      /*Litle endian utf16 */
        endian = 0;
    else if (buf[0] == 0xfe && buf[1] == 0xff) /*big endian utf16 .... */
        endian = 1;
    else
        return -1;

    /*The only check we can do is for the ascii characters ...... */
    for (i = 2; i < buflen; i += 2) {
        if (endian) {
            if (buf[i] == 0 && buf[i + 1] < 128
                    && text_chars[buf[i + 1]] != T)
                return -1;
        } else {
            if (buf[i + 1] == 0 && buf[i] < 128 && text_chars[buf[i]] != T)
                return -1;
        }
    }


    /*utf32 ????? who are using it? */

    return CI_UTF_DATA;
}

int ci_magics_db_data_type(const struct ci_magics_db *db, const char *buf, int buflen)
{
    int ret;

    if (buflen <= 0)
        return -1;

    if ((ret = check_magics(db, buf, buflen)) >= 0)
        return ret;

    /*At the feature the check_ascii and check_unicode must be merged ....*/
    if ((ret = check_ascii((unsigned char *) buf, buflen)) >= 0)
        return ret;

    if ((ret = check_unicode((unsigned char *) buf, buflen)) >= 0) {
        return CI_UTF_DATA;
    }

    return CI_BIN_DATA;        /*binary data */
}

int ci_filetype(struct ci_magics_db *db, const char *buf, int buflen)
{
    return ci_magics_db_data_type(db, buf, buflen);
}

/*return the datatype or -1 if not able to determine the data type*/
static int extend_object_type(struct ci_magics_db *db, ci_headers_list_t *headers, const char *buf,
                              int len, int *iscompressed)
{
    int unzipped_buf_len = 0;
    char *unzipped_buf = NULL;
    const char *checkbuf = buf;
    const char *content_type = NULL;
    const char *content_encoding = NULL;

    *iscompressed = CI_ENCODE_NONE;

    if (len <= 0)
        return -1;

    if (headers) {
        content_encoding = ci_headers_value(headers, "Content-Encoding");
        if (content_encoding) {
            ci_debug_printf(8, "Content-Encoding: %s\n", content_encoding);
            *iscompressed = ci_encoding_method(content_encoding);
            if (*iscompressed >= 0) {
                unzipped_buf_len = len > 1024 ? len : 1024;
                if (!(unzipped_buf = ci_buffer_alloc(unzipped_buf_len))) {
                    ci_debug_printf(1, "Error allocating buffer of size %d for uncompressing previewed object, for uncompressing data\n", unzipped_buf_len);
                    return -1;
                }

                if (ci_uncompress_preview(*iscompressed, buf, len, unzipped_buf, &unzipped_buf_len) == CI_ERROR || unzipped_buf_len <= 0) {
                    ci_debug_printf(3,"Error uncompressing encoded data using '%s', can not determine datatype\n", ci_encoding_method_str(*iscompressed));
                    ci_buffer_free(unzipped_buf);
                    return -1;
                }
                checkbuf = unzipped_buf;
                len = unzipped_buf_len;
            }
        }
    }

    int file_type = ci_magics_db_data_type(db, checkbuf, len);

    ci_debug_printf(7, "File type returned: %s,%s\n",
                    ci_data_type_name(db, file_type),
                    ci_data_type_descr(db, file_type));
    /*The following until we have an internal html recognizer ..... */
    if (ci_belongs_to_group(db, file_type, CI_TEXT_DATA)
            && headers
            && (content_type = ci_headers_value(headers, "Content-Type")) != NULL) {
        if (strcasestr(content_type, "text/html")
                || strcasestr(content_type, "text/css")
                || strcasestr(content_type, "text/javascript"))
            file_type = CI_HTML_DATA;
    }

    ci_debug_printf(7, "The file type now is: %s,%s\n",
                    ci_data_type_name(db, file_type),
                    ci_data_type_descr(db, file_type));

    if (unzipped_buf)
        ci_buffer_free(unzipped_buf);

    return file_type;
}

static int ci_extend_filetype(struct ci_magics_db *db, ci_request_t *req, const char *buf,
                              int len, int *iscompressed)
{
    ci_headers_list_t *heads;
    if (ci_req_type(req) == ICAP_RESPMOD)
        heads = ci_http_response_headers(req);
    else
        heads = NULL;

    return extend_object_type(db, heads, buf, len, iscompressed);
}


struct ci_magics_db *ci_magic_db_load(const char *filename)
{
    if (!_MAGIC_DB)
        return (_MAGIC_DB = ci_magics_db_build(filename));

    if (ci_magics_db_file_add(_MAGIC_DB, filename))
        return _MAGIC_DB;
    else
        return NULL;
}

void ci_magic_db_free()
{
    if (_MAGIC_DB)
        ci_magics_db_release(_MAGIC_DB);

    _MAGIC_DB = NULL;
}


int ci_magic_req_data_type(ci_request_t *req, int *isencoded)
{
    if (!_MAGIC_DB)
        return -1;

    if (!req->preview_data.used)
        return -1;

    if (req->preview_data_type <0 ) /*if there is not a cached value compute it*/
        req->preview_data_type =
            ci_extend_filetype(_MAGIC_DB, req,
                               req->preview_data.buf, req->preview_data.used,
                               isencoded);

    return req->preview_data_type;
}

int ci_magic_data_type(const char *buf, int len)
{
    if (!_MAGIC_DB)
        return -1;

    return ci_magics_db_data_type(_MAGIC_DB, buf, len);

}

int ci_magic_data_type_ext(ci_headers_list_t *headers, const char *buf, int len, int *iscompressed)
{
    if (!_MAGIC_DB)
        return -1;

    return extend_object_type(_MAGIC_DB, headers, buf, len, iscompressed);
}


int ci_magic_type_id(const char *name)
{
    if (!_MAGIC_DB)
        return -1;

    return ci_get_data_type_id(_MAGIC_DB, name);
}

int ci_magic_group_id(const char *group)
{
    if (!_MAGIC_DB)
        return -1;

    return ci_get_data_group_id(_MAGIC_DB, group);
}

int ci_magic_group_check(int type, int group)
{
    if (!_MAGIC_DB)
        return 0;

    return ci_belongs_to_group(_MAGIC_DB, type, group);
}

int ci_magic_types_count()
{
    return ci_magics_db_types_num(_MAGIC_DB);
}

int ci_magic_groups_count()
{
    return ci_magics_db_groups_num(_MAGIC_DB);
}

const char * ci_magic_type_name(int type)
{
    return ci_magics_db_type_name(_MAGIC_DB, type);
}

const char * ci_magic_type_descr(int type)
{
    return ci_magics_db_type_descr(_MAGIC_DB, type);
}

const char * ci_magic_group_name(int group)
{
    return ci_magics_db_group_name(_MAGIC_DB, group);
}

const int* ci_magic_type_groups(int type)
{
    return ci_magics_db_type_groups(_MAGIC_DB, type);
}
