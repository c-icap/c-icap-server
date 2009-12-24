/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ACL_H
#define __ACL_H

#include "c-icap.h"
#include "net_io.h"
#include "types_ops.h"

#define MAX_NAME_LEN 31  


/*ACL type structures and functions */
struct ci_request;
typedef struct ci_acl_type{
     char name[MAX_NAME_LEN+1];
     void *(*get_test_data)(struct ci_request *req, char *param); 
     void (*free_test_data)(struct ci_request *req, void *data);
     ci_type_ops_t *type;
} ci_acl_type_t;

struct ci_acl_type_list{
     ci_acl_type_t *acl_type_list;
     int acl_type_list_size;
     int acl_type_list_num;
};

int ci_acl_typelist_init(struct ci_acl_type_list *list);
int ci_acl_typelist_add(struct ci_acl_type_list *list, const ci_acl_type_t *type);
int ci_acl_typelist_release(struct ci_acl_type_list *list);
int ci_acl_typelist_reset(struct ci_acl_type_list *list);
const ci_acl_type_t *ci_acl_typelist_search(struct ci_acl_type_list *list, const char *name);


/*ACL specs structures and functions */

typedef struct ci_acl_data ci_acl_data_t;
struct ci_acl_data{
     void *data;
     ci_acl_data_t *next;
};

typedef struct ci_acl_spec ci_acl_spec_t;
struct ci_acl_spec {
     char name[MAX_NAME_LEN + 1];
     const ci_acl_type_t *type;
     char *parameter;
     ci_acl_data_t *data;
     ci_acl_spec_t *next;
};

/*Specs lists and access entries structures and functions */
typedef struct ci_specs_list ci_specs_list_t;
struct ci_specs_list{
     const ci_acl_spec_t *spec;
     int negate;
     ci_specs_list_t *next;
};

typedef struct ci_access_entry ci_access_entry_t;
struct ci_access_entry {
     int type;   /*CI_ACCESS_DENY or CI_ACCESS_ALLOW or CI_ACCESS_AUTH */
     ci_specs_list_t *spec_list;
     ci_access_entry_t *next;
};

CI_DECLARE_FUNC(ci_access_entry_t *) ci_access_entry_new(ci_access_entry_t **list, int type);
CI_DECLARE_FUNC(void) ci_access_entry_release(ci_access_entry_t *list);
CI_DECLARE_FUNC(const ci_acl_spec_t *) ci_access_entry_add_acl(ci_access_entry_t *access_entry, const ci_acl_spec_t *acl, int negate);
CI_DECLARE_FUNC(int) ci_access_entry_add_acl_by_name(ci_access_entry_t *access_entry, char *aclname);
CI_DECLARE_FUNC(int) ci_access_entry_match_request(ci_access_entry_t *access_entry, ci_request_t *req);


/*Inititalizing, reseting and tools acl library functions */
CI_DECLARE_FUNC(void) ci_acl_init();
CI_DECLARE_FUNC(void) ci_acl_reset();
CI_DECLARE_FUNC(const ci_acl_spec_t *) ci_acl_search(const char *name);
CI_DECLARE_FUNC(const ci_acl_type_t *) ci_acl_type_search(const char *name);
CI_DECLARE_FUNC(int) ci_acl_type_add(const ci_acl_type_t *type);

#endif/* __ACL_H*/
