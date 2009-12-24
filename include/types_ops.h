#ifndef __TYPES_OPS_H
#define __TYPES_OPS_H

#include "c-icap.h"
#include "mem.h"

typedef struct ci_type_ops {
    void *(*dup)(const char *, ci_mem_allocator_t *);
    void (*free)(void *key, ci_mem_allocator_t *);
    int (*compare)(void *ref_key,void *check_key);
    size_t (*size)(void *key);
    int (*equal)(void *ref_key,void *check_key);
} ci_type_ops_t;

CI_DECLARE_DATA extern ci_type_ops_t ci_str_ops;
CI_DECLARE_DATA extern ci_type_ops_t ci_str_ext_ops;
CI_DECLARE_DATA extern ci_type_ops_t ci_int32_ops;
CI_DECLARE_DATA extern ci_type_ops_t ci_ip_ops;
CI_DECLARE_DATA extern ci_type_ops_t ci_ip_sockaddr_ops;
CI_DECLARE_DATA extern ci_type_ops_t  ci_datatype_ops;
#ifdef USE_REGEX
CI_DECLARE_DATA extern ci_type_ops_t  ci_regex_ops;
#endif
#endif
