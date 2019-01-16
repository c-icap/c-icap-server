/*
 *  Copyright (C) 2004-2010 Christos Tsantilas
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
#include "lookup_table.h"
#include "hash.h"
#include "debug.h"
#include <assert.h>

/******************************************************/
/* file lookup table implementation                   */

void *file_table_open(struct ci_lookup_table *table);
void  file_table_close(struct ci_lookup_table *table);
void *file_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  file_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type file_table_type = {
    file_table_open,
    file_table_close,
    file_table_search,
    file_table_release_result,
    NULL,
    "file"
};

struct text_table_entry {
    void *key;
    void **vals;
    struct text_table_entry *next;
};

struct text_table {
    struct text_table_entry *entries;
    struct ci_hash_table *hash_table;
    int rows;
};

struct text_table_entry *alloc_text_table_entry (int val_num, ci_mem_allocator_t *allocator)
{
    struct text_table_entry *e;
    int i;
    e = allocator->alloc(allocator, sizeof(struct text_table_entry));
    e->key = NULL;
    e->next = NULL;
    if (!e) {
        ci_debug_printf(1,"Error allocating memory for table entry \n");
        return NULL;
    }
    if (val_num>0) {
        e->vals = allocator->alloc(allocator, (val_num + 1)*sizeof(void *));
        if (!e->vals) {
            allocator->free(allocator, e);
            e = NULL;
            ci_debug_printf(1,"Error allocating memory for values of  table entry.\n");
            return NULL;
        }
        for (i = 0; i< val_num + 1; i++)
            e->vals[i] = NULL;
    } else
        e->vals = NULL; /*Only key */
    return e;
}

void release_text_table_entry (struct text_table_entry *e, struct ci_lookup_table *table)
{
    void **vals;
    int i;
    ci_mem_allocator_t *allocator = table->allocator;

    if (!e)
        return;

    if (e->vals) {
        vals = (void **)e->vals;
        for (i = 0; vals[i] != NULL; i++)
            table->val_ops->free(vals[i], allocator);
        allocator->free(allocator, e->vals);
    }

    if (e->key)
        table->key_ops->free(e->key, allocator);
    allocator->free(allocator, e);
}

int read_row(FILE *f, int cols, struct text_table_entry **e,
             struct ci_lookup_table *table)
{
    char line[65536];
    char *s,*val,*end;
    int row_cols,line_len,i;
    ci_mem_allocator_t *allocator = table->allocator;
    const ci_type_ops_t *key_ops = table->key_ops;
    const ci_type_ops_t *val_ops = table->val_ops;

    (*e) = NULL;

    if (!fgets(line,65535,f))
        return 0;
    line[65535] = '\0';

    if ((line_len=strlen(line))>65535) {
        line[64] = '\0';
        ci_debug_printf(1, "Too long line: %s...", line);
        return 0;
    }
    if (line[line_len-1] == '\n') line[line_len-1] = '\0'; /*eat the newline char*/

    /*Do a check for comments*/
    s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#') /*it is a comment*/
        return 1;
    if (*s == '\0') /*it is a blank line */
        return 1;
    if (cols < 0) {
        /*the line should have the format  key:val1, val2, ... */
        if (!(s = strchr(line, ':'))) {
            row_cols = 1;
        } else {
            row_cols = 2;
            while ((s = strchr(s, ','))) row_cols++,s++;
        }
    } else
        row_cols = cols;

    (*e) = alloc_text_table_entry(row_cols-1, allocator);
    if (!(*e)) {
        ci_debug_printf(1,"Error allocating memory for table entry:%s\n", line);
        return 0;
    }

    s = line;
    while (*s == ' ' || *s == '\t') s++;
    val=s;

    end = NULL;
    if (row_cols > 1)
        end = strchr(s, ':');
    if (end == NULL) /*no ":" char or the only column is the key (row_cols <=1)*/
        end = s + strlen(s);

    s = end+1; /*Now points to the end (*s = '\0') or after the ':' */

    end--;
    while (*end == ' ' || *end == '\t') end--;
    *(end+1) = '\0';
    (*e)->key = key_ops->dup(val, allocator);

    if (!(*e)->key) {
        ci_debug_printf(1, "Error reading key in line:%s\n", line);
        release_text_table_entry((*e), table);
        (*e) = NULL;
        return -1;
    }

    if (row_cols > 1) {
        assert((*e)->vals);
        for (i = 0; *s != '\0'; i++) { /*probably we have vals*/
            if (i >= row_cols) {
                /*here we are leaving memory leak, I think qill never enter this if ...*/
                ci_debug_printf(1, "Error in read_row of file lookup table!(line:%s)\n", line);
                release_text_table_entry((*e), table);
                (*e) = NULL;
                return -1;
            }

            while (*s == ' ' || *s == '\t') s++; /*find the start of the string*/
            val = s;
            end = s;
            while (*end != ',' && *end != '\0') end++;
            if (*end == '\0')
                s = end;
            else
                s = end + 1;

            end--;
            while (*end == ' ' || *end == '\t') end--;
            *(end+1) = '\0';
            (*e)->vals[i] = val_ops->dup(val, allocator);
        }
        (*e)->vals[i] = NULL;
    }
    return 1;
}

int load_text_table(char *filename, struct ci_lookup_table *table)
{
    FILE *f;
    struct text_table_entry *e, *l = NULL;
    int rows, ret;
    struct text_table *text_table = (struct text_table *)table->data;
    if ((f = fopen(filename, "r")) == NULL) {
        ci_debug_printf(1, "Error opening file: %s\n", filename);
        return 0;
    }

    rows = 0;
    if (text_table->entries != NULL) /*if table is not empty try to find the last element*/
        for (l = text_table->entries; l->next != NULL; l = l->next);

    while (0 < (ret = read_row(f, table->cols, &e, table))) {
        if (e) {
            e->next = NULL;
            if (text_table->entries == NULL) {
                text_table->entries = e;
                l = e;
            } else {
                l->next = e;
                l = e;
            }
        }
        rows++;
    }
    fclose(f);

    if (ret == -1) {
        ci_debug_printf(1, "Error loading file table %s: parse error on line %d\n",
                        filename, rows+1);
        file_table_close(table);
        return 0;
    }

    text_table->rows = rows;
    return 1;
}


void *file_table_open(struct ci_lookup_table *table)
{
    struct ci_mem_allocator *allocator = table->allocator;
    struct text_table *text_table = allocator->alloc(allocator, sizeof(struct text_table));

    if (!text_table)
        return NULL;

    text_table->entries = NULL;
    table->data = (void *)text_table;
    if (!load_text_table(table->path, table)) {
        return (table->data = NULL);
    }
    text_table->hash_table = NULL;
    return text_table;
}

void  file_table_close(struct ci_lookup_table *table)
{
    int i;
    void **vals = NULL;
    struct text_table_entry *tmp;
    struct ci_mem_allocator *allocator = table->allocator;
    struct text_table *text_table = (struct text_table *)table->data;

    if (!text_table) {
        ci_debug_printf(1,"Closing a non open file lookup table?(%s)\n", table->path);
        return;
    }

    while (text_table->entries) {
        tmp = text_table->entries;
        text_table->entries = text_table->entries->next;
        if (tmp->vals) {
            vals = (void **)tmp->vals;
            for (i = 0; vals[i] != NULL; i++)
                table->val_ops->free(vals[i], allocator);
            allocator->free(allocator, tmp->vals);
        }

        table->key_ops->free(tmp->key, allocator);
        allocator->free(allocator, tmp);
    }
    allocator->free(allocator, text_table);
    table->data = NULL;
}

void *file_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    struct text_table_entry *e;
    struct text_table *text_table = (struct text_table *)table->data;

    if (!text_table) {
        ci_debug_printf(1,"Search a non open lookup table?(%s)\n", table->path);
        return NULL;
    }

    e = text_table->entries;
    *vals = NULL;
    while (e) {
        if (table->key_ops->compare((void *)e->key,key) == 0) {
            *vals = (void **)e->vals;
            return (void *)e->key;
        }
        e = e->next;
    }
    return NULL;
}

void  file_table_release_result(struct ci_lookup_table *table_data,void **val)
{
    /*do nothing*/
}

/******************************************************/
/* hash lookup table implementation                   */

void *hash_table_open(struct ci_lookup_table *table);
void  hash_table_close(struct ci_lookup_table *table);
void *hash_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  hash_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type hash_table_type = {
    hash_table_open,
    hash_table_close,
    hash_table_search,
    hash_table_release_result,
    NULL,
    "hash"
};


void *hash_table_open(struct ci_lookup_table *table)
{
    struct text_table_entry *e;
    struct text_table *text_table = file_table_open(table);
    if (!text_table)
        return NULL;

    /* build the hash table*/
    ci_debug_printf(7, "Will build a hash for %d rows of data\n", text_table->rows);
    text_table->hash_table = ci_hash_build(text_table->rows,
                                           table->key_ops,
                                           table->allocator);
    if (!text_table->hash_table) {
        file_table_close(table);
        return NULL;
    }

    e = text_table->entries;
    while (e) {
        ci_hash_add(text_table->hash_table,e->key, e);
        e = e->next;
    }

    return text_table;
}

void  hash_table_close(struct ci_lookup_table *table)
{
    struct text_table *text_table = (struct text_table *)table->data;
    if (text_table && text_table->hash_table) {
        /*destroy the hash table */
        ci_hash_destroy(text_table->hash_table);
        text_table->hash_table = NULL;
    }
    /*... and then call the file_table_close:*/
    file_table_close(table);
}

void *hash_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    const struct text_table_entry *e;
    struct text_table *text_table = (struct text_table *)table->data;

    if (!text_table) {
        ci_debug_printf(1, "Search a non open hash lookup table?(%s)\n", table->path);
        return NULL;
    }

    *vals = NULL;
    e = ci_hash_search(text_table->hash_table, key);
    if (!e)
        return NULL;

    *vals = (void **)e->vals;
    return (void *)e->key;
}

void  hash_table_release_result(struct ci_lookup_table *table_data,void **val)
{
    /*do nothing*/
}

/******************************************************/
/* regex lookup table implementation                   */

void *regex_table_open(struct ci_lookup_table *table);
void  regex_table_close(struct ci_lookup_table *table);
void *regex_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  regex_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type regex_table_type = {
    regex_table_open,
    regex_table_close,
    regex_table_search,
    regex_table_release_result,
    NULL,
    "regex"
};


void *regex_table_open(struct ci_lookup_table *table)
{
#ifdef USE_REGEX
    struct text_table *text_table;
    if (table->key_ops != &ci_str_ops) {
        ci_debug_printf(1,"This type of table is not compatible with regex tables!\n");
        return NULL;
    }
    table->key_ops = &ci_regex_ops;

    text_table = file_table_open(table);
    if (!text_table)
        return NULL;

    return text_table;
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
    return NULL;
#endif
}

void  regex_table_close(struct ci_lookup_table *table)
{
#ifdef USE_REGEX
    /*just call the file_table_close:*/
    file_table_close(table);
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
#endif
}

void *regex_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
#ifdef USE_REGEX
    return file_table_search(table, key, vals);
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
    return NULL;
#endif
}

void  regex_table_release_result(struct ci_lookup_table *table_data,void **val)
{
    /*do nothing*/
}

