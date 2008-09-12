#ifndef __LOOKUP_TABLE_H
#define __LOOKUP_TABLE_H

#include "c-icap.h"
#include "mem.h"

struct ci_lookup_table;
struct ci_lookup_table_type {
    void *(*open)(struct ci_lookup_table *table); 
    void  (*close)(struct ci_lookup_table *table);
    void *(*search)(struct ci_lookup_table *table, void *key, void ***vals);
    void  (*release_result)(struct ci_lookup_table *table_data, void **val);
    char *type;
};

struct ci_lookup_table {
    void *(*open)(struct ci_lookup_table *table); 
    void  (*close)(struct ci_lookup_table *table);
    void *(*search)(struct ci_lookup_table *table, void *key, void ***vals);
    void  (*release_result)(struct ci_lookup_table *table, void **val);
    char *type;
    char *path;
    char *args;
    int cols;
    void *(*keydup)(const char *, ci_mem_allocator_t *);
    void *(*valdup)(const char *, ci_mem_allocator_t *);
    int (*keycomp)(void *key1,void *key2);
    ci_mem_allocator_t *allocator;
    void *data;
};

CI_DECLARE_FUNC(struct ci_lookup_table_type *) ci_lookup_table_type_add(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(const struct ci_lookup_table_type *) ci_lookup_table_type_search(const char *type);
CI_DECLARE_FUNC(struct ci_lookup_table *) ci_lookup_table_create(const char *table);
CI_DECLARE_FUNC(void) ci_lookup_table_destroy(struct ci_lookup_table *lt);

#endif
