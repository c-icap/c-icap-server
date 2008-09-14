#ifndef __LOOKUP_TABLE_H
#define __LOOKUP_TABLE_H

#include "c-icap.h"
#include "mem.h"

typedef struct ci_type_ops {
    void *(*dup)(const char *, ci_mem_allocator_t *);
    int (*compare)(void *key1,void *key2);
    size_t (*length)(void *key);
} ci_type_ops_t;


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

CI_DECLARE_FUNC(struct ci_lookup_table_type *) ci_lookup_table_type_add(struct ci_lookup_table_type *lt_type);
CI_DECLARE_FUNC(const struct ci_lookup_table_type *) ci_lookup_table_type_search(const char *type);
CI_DECLARE_FUNC(struct ci_lookup_table *) ci_lookup_table_create(const char *table);
CI_DECLARE_FUNC(void) ci_lookup_table_destroy(struct ci_lookup_table *lt);

CI_DECLARE_DATA extern ci_type_ops_t ci_str_ops;
/*Todo:
extern ci_table_type_ops_t ci_int32_ops;
extern ci_table_type_ops_t ci_ip_ops;
*/
#endif
