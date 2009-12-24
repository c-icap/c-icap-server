#ifndef __LOOKUP_TABLE_H
#define __LOOKUP_TABLE_H

#include "c-icap.h"
#include "mem.h"
#include "types_ops.h"

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
    ci_type_ops_t *key_ops;
    ci_type_ops_t *val_ops;
    ci_mem_allocator_t *allocator;
    void *data;
};

CI_DECLARE_FUNC(struct ci_lookup_table_type *) ci_lookup_table_type_register(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(void) ci_lookup_table_type_unregister(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(const struct ci_lookup_table_type *) ci_lookup_table_type_search(const char *type);
CI_DECLARE_FUNC(struct ci_lookup_table *) ci_lookup_table_create(const char *table);
CI_DECLARE_FUNC(void) ci_lookup_table_destroy(struct ci_lookup_table *lt);

#endif
