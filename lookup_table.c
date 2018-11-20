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
#include "debug.h"
#include "mem.h"


/***********************************************************/
/* Global variables                                        */

/*we can support up to 128  lookup table types, looks enough*/
const struct ci_lookup_table_type *lookup_tables_types[128];
int lookup_tables_types_num = 0;

/*********************************************************************/
/*Lookuptable library functions                                      */

struct ci_lookup_table_type *ci_lookup_table_type_register( struct ci_lookup_table_type *lt_type)
{
    if (lookup_tables_types_num >= 128) {
        ci_debug_printf(1,"c-icap does not support more than 128 loookup table types");
        return NULL;
    }
    lookup_tables_types[lookup_tables_types_num++] = lt_type;
    return lt_type;
}

void ci_lookup_table_type_unregister( struct ci_lookup_table_type *lt_type)
{
    int i;
    for (i = 0; lookup_tables_types[i] != lt_type && i < lookup_tables_types_num; i++);

    if (i<lookup_tables_types_num) {
        lookup_tables_types_num--;
        for (; i < lookup_tables_types_num; i++)
            lookup_tables_types[i] = lookup_tables_types[i+1];
    }
}

const struct ci_lookup_table_type *ci_lookup_table_type_search(const char *type)
{
    int i;
    for (i = 0; i<lookup_tables_types_num; i++) {
        if (strcmp(type,lookup_tables_types[i]->type) == 0)
            return lookup_tables_types[i];
    }
    return NULL;
}

static const void * lookup_table_get_row(struct ci_lookup_table *table, const void *key, const char *columns[], void ***vals);
struct ci_lookup_table *ci_lookup_table_create_ext(const char *table,
        const ci_type_ops_t *key_ops,
        const ci_type_ops_t *val_ops,
        ci_mem_allocator_t *allocator)
{
    char *ttype,*path,*args,*s;
    const struct ci_lookup_table_type *lt_type;
    struct ci_lookup_table *lt;
    char *stable = strdup(table);
    if (!stable) {
        /*A debug message.....*/
        return NULL;
    }

    /*Normaly the table has the form tabletype:/path/{args}*/
    s = strchr(stable, ':');

    if (!s) { /*Then it is a simple text file*/
        ttype = "file";
        path = stable;
        args = NULL;
    } else {
        ttype = stable;
        path = s+1;
        *s = '\0';
        s = strchr(path, '{');
        if (s) {
            *s = '\0'; /* path ends here */
            args = s+1; /*args start here */

            if ((s = strchr(args, '}'))) *s = '\0'; /*else args is all the remains string */
        } else /*No args*/
            args = NULL;
    }
    lt_type = ci_lookup_table_type_search(ttype);
    if (!lt_type || !lt_type->open) {
        ci_debug_printf(1,"Not lookuptable of type :%s!!!\n", ttype);
        free(stable);
        return NULL;
    }

    lt = malloc(sizeof(struct ci_lookup_table));
    if (!lt) {
        ci_debug_printf(1,"memory allocation error!!");
        free(stable);
        return NULL;
    }

    lt->path = strdup(path);
    if (args)
        lt->args = strdup(args);
    else
        lt->args = NULL;

    free(stable);


    lt->cols = -1;
    lt->key_ops = key_ops;
    lt->val_ops = val_ops;
    lt->type = lt_type->type;
    lt->open = lt_type->open;
    lt->close = lt_type->close;
    lt->search = lt_type->search;
    lt->get_row = lookup_table_get_row;
    lt->release_result = lt_type->release_result;
    lt->allocator = allocator;
    lt->_lt_type = lt_type;
    lt->data = NULL;

    return lt;
}

struct ci_lookup_table *ci_lookup_table_create(const char *table)
{
    ci_mem_allocator_t *allocator;
    struct ci_lookup_table *lt;
    allocator = ci_create_os_allocator();
    if (!allocator)
        return NULL;

    lt = ci_lookup_table_create_ext(table, &ci_str_ops, &ci_str_ops, allocator);
    if (!lt)
        ci_mem_allocator_destroy(allocator);
    return lt;
}

void ci_lookup_table_destroy(struct ci_lookup_table *lt)
{
    if (!lt)
        return;

    lt->close(lt);
    free(lt->path);
    if (lt->args)
        free(lt->args);
    if (lt->allocator)
        ci_mem_allocator_destroy(lt->allocator);
    free(lt);
}

void * ci_lookup_table_open(struct ci_lookup_table *table)
{
    if (!table->_lt_type || !table->open) {
        ci_debug_printf(1, "lookup_table of type  %s is corrupted (\"open\" method missing)!\n", table->type);
        return NULL;
    }
    return table->open(table);
}

const char * ci_lookup_table_search(struct ci_lookup_table *table, const char *key, char ***vals)
{
    if (!table->_lt_type || !table->search) {
        ci_debug_printf(1, "lookup_table of type  %s is corrupted (\"search\" method missing)!\n", table->type);
        return NULL;
    }

    if (!ci_type_ops_is_string(table->key_ops) ||  !ci_type_ops_is_string(table->val_ops)) {
        ci_debug_printf(1, "lookup_table of type  %s does not support search with string like keys!\n", table->type);
        return NULL;
    }

    return table->search(table, (void *)key, (void ***)vals);
}

void  ci_lookup_table_release_result(struct ci_lookup_table *table, void **val)
{
    if (!table->_lt_type || !table->release_result) {
        ci_debug_printf(1, "lookup_table of type  %s is corrupted (\"release_result\" method missing)!\n", table->type);
        return;
    }
    return table->release_result(table, (void **)val);
}

const void * lookup_table_get_row(struct ci_lookup_table *table, const void *key, const char *columns[], void ***vals)
{
    int i;
    if (!table->_lt_type) {
        ci_debug_printf(1, "lookup_table of type  %s is corrupted!\n", table->type);
        return NULL;
    }

    if (!table->col_names || table->_lt_type->get_row) {
        ci_debug_printf(1, "lookup_table :%s does not support lookup on named columns\n", table->type);
        return NULL;
    }

    for (i = 0; i < 1024 && columns[i] != NULL; i++) {
        if (NULL == ci_str_vector_search(table->col_names, columns[i])) {
            ci_debug_printf(1, "lookup_table :%s does not has column %s\n", table->type, columns[i]);
            return NULL;
        }
    }

    return table->_lt_type->get_row(table, key, columns, vals);
}

const char * ci_lookup_table_get_row(struct ci_lookup_table *table, const char *key, const char *columns[], char ***vals)
{
    if ( !ci_type_ops_is_string(table->key_ops) ||  !ci_type_ops_is_string(table->val_ops)) {
        ci_debug_printf(1, "lookup_table of type  %s does not support search with string like keys!\n", table->type);
        return NULL;
    }

    return (const char *) lookup_table_get_row(table, (const void *)key, columns, (void ***)vals);
}

extern struct ci_lookup_table_type file_table_type;
extern struct ci_lookup_table_type hash_table_type;
extern struct ci_lookup_table_type regex_table_type;
CI_DECLARE_FUNC(void) init_internal_lookup_tables()
{
    ci_lookup_table_type_register(&file_table_type);
    ci_lookup_table_type_register(&hash_table_type);
    ci_lookup_table_type_register(&regex_table_type);
}
