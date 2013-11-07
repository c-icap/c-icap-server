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

#ifndef __TYPES_OPS_H
#define __TYPES_OPS_H

#include "c-icap.h"
#include "mem.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ci_type_ops {
    void *(*dup)(const char *, ci_mem_allocator_t *);
    void (*free)(void *key, ci_mem_allocator_t *);
    int (*compare)(const void *ref_key,const void *check_key);
    size_t (*size)(const void *key);
    int (*equal)(const void *ref_key,const void *check_key);
} ci_type_ops_t;

CI_DECLARE_DATA extern const ci_type_ops_t ci_str_ops;
CI_DECLARE_DATA extern const ci_type_ops_t ci_str_ext_ops;
CI_DECLARE_DATA extern const ci_type_ops_t ci_int32_ops;
CI_DECLARE_DATA extern const ci_type_ops_t ci_uint64_ops;
CI_DECLARE_DATA extern const ci_type_ops_t ci_ip_ops;
CI_DECLARE_DATA extern const ci_type_ops_t ci_ip_sockaddr_ops;
CI_DECLARE_DATA extern const ci_type_ops_t  ci_datatype_ops;
#ifdef USE_REGEX
CI_DECLARE_DATA extern const ci_type_ops_t  ci_regex_ops;
#define ci_type_ops_is_string(tops) ((tops) == &ci_str_ops || (tops) == &ci_str_ext_ops || (tops) == &ci_regex_ops)
#else
#define ci_type_ops_is_string(tops) ((tops) == &ci_str_ops || (tops) == &ci_str_ext_ops)
#endif


#ifdef __cplusplus
}
#endif

#endif
