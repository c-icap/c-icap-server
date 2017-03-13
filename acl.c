/*
 *  Copyright (C) 2004 Christos Tsantilas
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

#include "c-icap.h"
#include "common.h"
#include "request.h"
#include "cfg_param.h"
#include "debug.h"
#include "simple_api.h"
#include "acl.h"
#include "access.h"
#include "mem.h"
#include "filetype.h"
#include <ctype.h>
#include <time.h>

/*standard acl types */

/*Spec types:
  username: user,
  servicename: service
  requet_type: type
  port : port
  src_ip: src
  dst_ip: srvip
*/

void *get_user(ci_request_t *req, char *param)
{
    return req->user;
}

void *get_service(ci_request_t *req, char *param)
{
    return req->service;
}

void *get_reqtype(ci_request_t *req, char *param)
{
    return (void *)ci_method_string(req->type);
}

void *get_port(ci_request_t *req, char *param)
{
    return &req->connection->srvaddr.ci_sin_port;
}

void *get_client_ip(ci_request_t *req, char *param)
{
    return &(req->connection->claddr);
}

void *get_srv_ip(ci_request_t *req, char *param)
{
    return &(req->connection->srvaddr);
}

void *get_http_client_ip(ci_request_t *req, char *param)
{
    return (void *)ci_http_client_ip(req);
}

#if defined(USE_REGEX)
/*They are implemented at the bottom of this file ...*/
void *get_icap_header(ci_request_t *req, char *param);
void *get_icap_response_header(ci_request_t *req, char *param);
void *get_http_req_header(ci_request_t *req, char *param);
void *get_http_resp_header(ci_request_t *req, char *param);

void free_icap_header(ci_request_t *req,void *param);
void free_icap_response_header(ci_request_t *req, void *param);
void free_http_req_header(ci_request_t *req, void *param);
void free_http_resp_header(ci_request_t *req, void *param);

void *get_http_req_url(ci_request_t *req, char *param);
void free_http_req_url(ci_request_t *req, void *data);
void *get_http_req_line(ci_request_t *req, char *param);
void free_http_req_line(ci_request_t *req, void *data);
void *get_http_resp_line(ci_request_t *req, char *param);
void free_http_resp_line(ci_request_t *req, void *data);
#endif

void *get_http_req_method(ci_request_t *req, char *param);
void free_http_req_method(ci_request_t *req, void *param);

void *get_data_type(ci_request_t *req, char *param);
void free_data_type(ci_request_t *req,void *param);

ci_acl_type_t acl_user= {
    "user",
    get_user,
    NULL,
    &ci_str_ops
};

ci_acl_type_t acl_service= {
    "service",
    get_service,
    NULL,
    &ci_str_ops
};

ci_acl_type_t acl_req_type= {
    "type",
    get_reqtype,
    NULL,
    &ci_str_ops
};

ci_acl_type_t acl_tcp_port= {
    "port",
    get_port,
    NULL,
    &ci_int32_ops
};

ci_acl_type_t acl_tcp_src= {
    "src",
    get_client_ip,
    NULL,
    &ci_ip_sockaddr_ops
};

ci_acl_type_t acl_tcp_srvip= {
    "srvip",
    get_srv_ip,
    NULL,
    &ci_ip_sockaddr_ops
};

ci_acl_type_t acl_tcp_xclientip= {
    "http_client_ip",
    get_http_client_ip,
    NULL,
    &ci_ip_ops
};

#if defined(USE_REGEX)
ci_acl_type_t acl_icap_header = {
    "icap_header",
    get_icap_header,
    free_icap_header,
    &ci_regex_ops
};

ci_acl_type_t acl_icap_resp_header = {
    "icap_resp_header",
    get_icap_response_header,
    free_icap_response_header,
    &ci_regex_ops
};

ci_acl_type_t acl_http_req_header = {
    "http_req_header",
    get_http_req_header,
    free_http_req_header,
    &ci_regex_ops
};

ci_acl_type_t acl_http_resp_header = {
    "http_resp_header",
    get_http_resp_header,
    free_http_resp_header,
    &ci_regex_ops
};

ci_acl_type_t acl_http_req_url = {
    "http_req_url",
    get_http_req_url,
    free_http_req_url,
    &ci_regex_ops
};

ci_acl_type_t acl_http_req_line = {
    "http_req_line",
    get_http_req_line,
    free_http_req_line,
    &ci_regex_ops
};

ci_acl_type_t acl_http_resp_line = {
    "http_resp_line",
    get_http_resp_line,
    free_http_resp_line,
    &ci_regex_ops
};
#endif

ci_acl_type_t acl_http_req_method= {
    "http_req_method",
    get_http_req_method,
    free_http_req_method,
    &ci_str_ops
};

ci_acl_type_t acl_data_type= {
    "data_type",
    get_data_type,
    free_data_type,
    &ci_datatype_ops
};

struct acl_cmp_uint64_data {
    uint64_t data;
    int operator;
};

/**** Content-Length acl ****/

void *acl_cmp_uint64_dup(const char *str, ci_mem_allocator_t *allocator)
{
    return ci_uint64_ops.dup(str, allocator);
}

int acl_cmp_uint64_equal(const void *key1, const void *key2)
{
    uint64_t k1 = *(uint64_t *)key1;
    struct acl_cmp_uint64_data *data = (struct acl_cmp_uint64_data *)key2;
    ci_debug_printf(8, "Acl content length check %llu %c %llu\n",
                    (long long int)data->data, data->operator == 1 ? '>' : data->operator == 2 ? '<' : '=', (long long int)k1);
    if (data->operator == 1) { /* > */
        return data->data > k1;
    } else if (data->operator == 2) { /* < */
        return data->data < k1;
    } else {  /* = */
        return k1 == data->data;
    }
}

void acl_cmp_uint64_free(void *key, ci_mem_allocator_t *allocator)
{
    ci_uint64_ops.free(key, allocator);
}

static const ci_type_ops_t acl_cmp_uint64_ops = {
    acl_cmp_uint64_dup,
    acl_cmp_uint64_free,
    NULL, // compare, not used here
    NULL, // length, not used here
    acl_cmp_uint64_equal
};

void free_cmp_uint64_data(ci_request_t *req,void *param);
void *get_content_length(ci_request_t *req, char *param);
static ci_acl_type_t acl_content_length= {
    "content_length",
    get_content_length,
    free_cmp_uint64_data,
    &acl_cmp_uint64_ops
};

/**** Time acl  ****/
/* Acl in the form:
       [DAY[,DAY,[..]]][/][HH:MM-HH:MM]
   DAY:
   S - Sunday
   M - Monday
   T - Tuesday
   W - Wednesday
   H - Thursday
   F - Friday
   A - Saturday

   acl time WorkingDays M,T,W,H,F/9:00-18:00
   acl time ChildTime Saturday,Sunday/8:30-20:00
   acl time ParentsTime 20:00-24:00 00:00-8:30
 */

/*Acl ci_types_ops_t */
void *acl_time_dup(const char *str, ci_mem_allocator_t *allocator);
int acl_time_equal(const void *key1, const void *key2);
void acl_time_free(void *key, ci_mem_allocator_t *allocator);

static const ci_type_ops_t acl_time_ops = {
    acl_time_dup,
    acl_time_free,
    NULL, // compare, not used here
    NULL, // length, not used here
    acl_time_equal
};

/*The acl type*/
void free_time_data(ci_request_t *req,void *param);
void *get_time_data(ci_request_t *req, char *param);
static ci_acl_type_t acl_time= {
    "time",
    get_time_data,
    free_time_data,
    &acl_time_ops
};

struct acl_time_data {
    unsigned int days;
    unsigned int start_time;
    unsigned int end_time;
};

void *acl_time_dup(const char *str, ci_mem_allocator_t *allocator)
{
    struct {
        const char *day;
        int id;
    } days[] = {
        {"Sunday", 0},
        {"Monday", 1},
        {"Tuesday", 2},
        {"Wednesday", 3},
        {"Thursday", 4},
        {"Friday", 5},
        {"Saturday", 6},
        {"S", 0},
        {"M", 1},
        {"T", 2},
        {"W", 3},
        {"H", 4},
        {"F", 5},
        {"A", 6},
        {NULL, -1}
    };
    int h1, m1, h2, m2, i;
    char *s, *e;
    const char *error;
    char buf[1024];
    struct acl_time_data *tmd = allocator->alloc(allocator, sizeof(struct acl_time_data));
    tmd->days = 0;
    /*copy string in order to parse it*/
    strncpy(buf, str, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    s = buf;
    if (!isdigit(*s)) {
        do {
            if (*s == ',') ++s;
            for (i = 0; days[i].day != NULL; ++i) {
                if (strncasecmp(s, days[i].day, strlen(days[i].day)) == 0) {
                    tmd->days |= 1 << days[i].id;
                    break;
                }
            }
            if (days[i].day == NULL) {
                /*not found*/
                error = s;
                goto acl_time_dup_fail;
            }
            s += strlen(days[i].day);

        } while (*s == ',');

        if (*s != '/') {
            error = s;
            goto acl_time_dup_fail;
        }
        if (*s) ++s;
    }

    /* Time region specification*/
    /*We are expecting hour region in the form 'HH:MM-HH:MM' */
    if (!isdigit(*s)) {
        error = s;
        goto acl_time_dup_fail;
    }

    h1 = strtol(s, &e, 10);
    if (h1 < 0 || h1 > 24) {
        error = s;
        goto acl_time_dup_fail;
    }
    if (*e != ':' || !isdigit(e[1])) {
        error = e;
        goto acl_time_dup_fail;
    }
    s = e + 1;
    m1 = strtol(s, &e, 10);
    if (m1 < 0 || m1 > 59) {
        error = s;
        goto acl_time_dup_fail;
    }
    if (*e != '-' || !isdigit(e[1])) {
        error = e;
        goto acl_time_dup_fail;
    }

    s = e + 1;
    h2 = strtol(s, &e, 10);
    if (h2 < 0 || h2 > 24) {
        error = s;
        goto acl_time_dup_fail;
    }
    if (*e != ':' || !isdigit(e[1])) {
        error = e;
        goto acl_time_dup_fail;
    }
    s = e + 1;
    m2 = strtol(s, &e, 10);
    if (m2 < 0 || m2 > 59) {
        error = s;
        goto acl_time_dup_fail;
    }
    tmd->start_time = h1 * 60 + m1;
    tmd->end_time = h2 * 60 + m2;

    if (tmd->start_time <= tmd->end_time) {
        ci_debug_printf(5, "Acl time, adding days: %x,  start time %d, end time: %d!\n", tmd->days, tmd->start_time, tmd->end_time);
        return tmd;
    }

    ci_debug_printf(1, "Acl '%s': end time is smaller than the start time!\n", str);
    error = str;

acl_time_dup_fail:
    ci_debug_printf(1, "Failed to parse acl time: %s (error on pos '...%s')\n", str, error);
    allocator->free(allocator, (void *)tmd);
    return NULL;
}

int acl_time_equal(const void *key1, const void *key2)
{
    struct acl_time_data *tmd_acl = (struct acl_time_data *)key1;
    struct acl_time_data *tmd_request = (struct acl_time_data *)key2;
    ci_debug_printf(9, "acl_time_equal(key1=%p, key2=%p)\n", key1, key2);
    int matches = (tmd_acl->days & tmd_request->days) &&
                  (tmd_request->start_time >= tmd_acl->start_time) &&
                  (tmd_request->start_time <= tmd_acl->end_time);
    ci_debug_printf(8, "acl_time_equal: %x/%d-%d <> %x/%d-%d -> %d\n",
                    tmd_acl->days, tmd_acl->start_time, tmd_acl->end_time,
                    tmd_request->days, tmd_request->start_time, tmd_request->end_time,
                    matches
                   );
    return matches;
}

void acl_time_free(void *tmd, ci_mem_allocator_t *allocator)
{
    allocator->free(allocator, (void *)tmd);
}

void free_time_data(ci_request_t *req,void *param)
{
    /*Nothing to do*/
    ci_debug_printf(5, "free_time_data(req=%p, param=%p)", req, param);
    ci_buffer_free(param);
}

void *get_time_data(ci_request_t *req, char *param)
{
    struct acl_time_data *tmd_req = ci_buffer_alloc(sizeof(struct acl_time_data));
    struct tm br_tm;
    time_t tm;
    time(&tm);
    localtime_r(&tm, &br_tm);
    tmd_req->days = 0;
    tmd_req->days |= (1 << br_tm.tm_wday);
    tmd_req->start_time = (br_tm.tm_hour) * 60 + br_tm.tm_min;
    tmd_req->end_time = 0;
    return (void *)tmd_req;
}


/********************************************************************************/
/*   ci_access_entry api   functions                                            */

ci_access_entry_t *ci_access_entry_new(ci_access_entry_t **list, int type)
{
    ci_access_entry_t *access_entry, *cur;

    if (list == NULL)
        return NULL;

    if (!(access_entry = malloc(sizeof(ci_access_entry_t))))
        return NULL;

    access_entry->type = type;
    access_entry->spec_list = NULL;
    access_entry->next = NULL;

    if (*list == NULL) {
        *list = access_entry;
    } else {
        cur = *list;
        while (cur->next!=NULL)
            cur = cur->next;
        cur->next = access_entry;
    }
    return access_entry;
}

void ci_access_entry_release(ci_access_entry_t *list)
{
    ci_access_entry_t *access_entry;
    ci_specs_list_t *spec_list, *cur;
    if (!list)
        return;

    access_entry = list;
    while (list) {
        access_entry = list;
        list = list->next;
        spec_list = access_entry->spec_list;

        while (spec_list) {
            cur = spec_list;
            spec_list = spec_list->next;
            free(cur);
        }
        free(access_entry);
    }

}

const ci_acl_spec_t *ci_access_entry_add_acl(ci_access_entry_t *access_entry, const ci_acl_spec_t *acl, int negate)
{
    struct ci_specs_list *spec_list,*spec_entry;
    if (access_entry == NULL)
        return NULL;

    spec_entry = malloc(sizeof(struct ci_specs_list));
    if (spec_entry == NULL)
        return NULL;

    spec_entry->next = NULL;
    spec_entry->negate = negate;
    spec_entry->spec = acl;
    if (access_entry->spec_list == NULL) {
        access_entry->spec_list = spec_entry;
    } else {
        spec_list = access_entry->spec_list;
        while (spec_list->next != NULL)
            spec_list = spec_list->next;
        spec_list->next = spec_entry;
    }
    return acl;
}

int ci_access_entry_add_acl_by_name(ci_access_entry_t *access_entry, const char *acl_name)
{
    const ci_acl_spec_t *acl;
    int negate = 0;
    if (acl_name[0] == '!') {
        negate = 1;
        acl_name = acl_name + 1;
    }
    acl = ci_acl_search(acl_name);
    if (!acl) {
        ci_debug_printf(1, "The acl spec %s does not exists!\n", acl_name);
        return 0;
    }
    if (ci_access_entry_add_acl(access_entry, acl, negate) == NULL) {
        ci_debug_printf(1, "Error adding acl spec %s to the access list!\n", acl_name);
        return 0;
    }
    return 1;
}

/*********************************************************************************/
/*ci_acl_spec functions                                                          */

ci_acl_spec_t *  ci_acl_spec_new(const char *name, const char *type, const char *param, struct ci_acl_type_list *list, ci_acl_spec_t **spec_list)
{
    ci_acl_spec_t *spec,*cur;
    const ci_acl_type_t *acl_type;
    acl_type = ci_acl_typelist_search(list, type);
    if (!acl_type)
        return NULL;

    if (!(spec = malloc(sizeof( ci_acl_spec_t))))
        return NULL;

    strncpy(spec->name, name, MAX_NAME_LEN);
    spec->name[MAX_NAME_LEN] = '\0';
    if (param) {
        if (!(spec->parameter = strdup(param))) {
            free(spec);
            return NULL;
        }
    } else
        spec->parameter = NULL;
    spec->type = acl_type;
    spec->data = NULL;
    spec->next = NULL;

    if (spec_list!=NULL) {
        if (*spec_list!=NULL) {
            cur = *spec_list;
            while (cur->next!=NULL)
                cur = cur->next;
            cur->next = spec;
        } else
            *spec_list = spec;
    }
    return spec;
}

ci_acl_data_t *ci_acl_spec_new_data(ci_acl_spec_t *spec, const char *val)
{
    ci_acl_data_t *new_data, *list;
    const ci_type_ops_t *ops;
    void *data;

    if (!spec)
        return NULL;

    ops = spec->type->type;
    data = ops->dup(val, default_allocator);
    if (!data)
        return NULL;

    new_data = malloc(sizeof(ci_acl_data_t));
    if (!new_data) {
        ops->free(data, default_allocator);
        return NULL;
    }
    new_data->data = data;
    new_data->next = NULL;
    if ((list=spec->data) != NULL) {
        while (list->next != NULL)
            list = list->next;
        list->next = new_data;
    } else
        spec->data = new_data;
    return new_data;
}

ci_acl_spec_t *ci_acl_spec_search(ci_acl_spec_t *list, const char *name)
{
    ci_acl_spec_t *spec;
    ci_debug_printf(9,"In search specs list %p,name %s\n", list, name);
    if (!list || !name)
        return NULL;
    spec = list;
    while (spec!= NULL) {
        ci_debug_printf(9,"Checking name:%s with specname %s\n", name, spec->name);
        if (strcmp(spec->name, name) == 0 ) {
            return spec;
        }
        spec = spec->next;
    }
    return NULL;
}


void ci_acl_spec_release(ci_acl_spec_t *cur)
{
    ci_acl_data_t *dhead, *dtmp;
    const ci_type_ops_t *ops;
    dhead = cur->data;
    ops = cur->type->type;
    while (dhead) {
        dtmp = dhead;
        dhead = dhead->next;
        ops->free(dtmp->data, default_allocator);
        free(dtmp);
    }
}

void ci_acl_spec_list_release(ci_acl_spec_t *spec)
{
    ci_acl_spec_t *cur;
    while (spec) {
        cur=spec;
        spec=spec->next;
        ci_acl_spec_release(cur);
    }
}


/*******************************************************************/
/* ci_acl_type functions                                           */
#define STEP 32

int ci_acl_typelist_init(struct ci_acl_type_list *list)
{
    list->acl_type_list = malloc(STEP*sizeof(ci_acl_type_t));
    list->acl_type_list_size = STEP;
    list->acl_type_list_num = 0;
    return 1;
}

int ci_acl_typelist_add(struct ci_acl_type_list *list, const ci_acl_type_t *type)
{
    ci_acl_type_t *cur;
    ci_acl_type_t *nl = NULL;

    if (!type->name)
        return 0;

    if (ci_acl_typelist_search(list, type->name) != NULL) {
        ci_debug_printf(3, "The acl type %s already defined\n", type->name);
        return 0;
    }

    if (list->acl_type_list_num == list->acl_type_list_size) {
        list->acl_type_list_size += STEP;
        nl = realloc((void *)list->acl_type_list,
                     list->acl_type_list_size*sizeof(ci_acl_type_t));
        if (!nl) {
            ci_debug_printf(1, "Failed to allocate more space for new ci_acl_typr_t\n");
            return 0;
        }
        list->acl_type_list = nl;
    }

    cur = &(list->acl_type_list[list->acl_type_list_num]);
    strncpy(cur->name, type->name, MAX_NAME_LEN);
    cur->name[MAX_NAME_LEN] = '\0';
    cur->type = type->type;
    cur->get_test_data = type->get_test_data;
    list->acl_type_list_num++;
    return 1;
}

const ci_acl_type_t *ci_acl_typelist_search(struct ci_acl_type_list *list,const char *name)
{
    int i;
    for (i=0; i<list->acl_type_list_num; i++) {
        if (strcmp(list->acl_type_list[i].name,name)==0)
            return (const ci_acl_type_t *)&list->acl_type_list[i];
    }
    return NULL;
}

int ci_acl_typelist_release(struct ci_acl_type_list *list)
{
    free(list->acl_type_list);
    list->acl_type_list_size = 0;
    list->acl_type_list_num = 0;
    return 1;
}

int ci_acl_typelist_reset(struct ci_acl_type_list *list)
{
    list->acl_type_list_num = 0;
    return 1;
}



/*********************************************************************************/

int spec_data_check(const ci_acl_spec_t *spec, const void *req_raw_data)
{
//    int (*comp)(void *req_spec, void *acl_spec);
    struct ci_acl_data *spec_data=spec->data;
    const ci_type_ops_t *ops = spec->type->type;

    ci_debug_printf(9,"Check request with ci_acl_spec_t:%s\n", spec->name);
    while (spec_data!=NULL) {
        if (ops->equal(spec_data->data, (void *)req_raw_data)) {
            ci_debug_printf(9,"The ci_acl_spec_t:%s matches\n", spec->name);
            return 1;
        }
        spec_data=spec_data->next;
    }
    return 0;
}

int request_match_specslist(ci_request_t *req, const struct ci_specs_list *spec_list)
{
    const ci_acl_spec_t *spec;
    const ci_acl_type_t *type;
    int ret, negate, check_result;
    void *test_data;

    ret = 1;
    while (spec_list!=NULL) {
        spec = spec_list->spec;
        negate = spec_list->negate;
        type = spec->type;
        test_data = type->get_test_data(req, spec->parameter);
        if (!test_data) {
            ci_debug_printf(9,"No data to test for %s/%s, ignore\n", type->name, spec->parameter);
            return 0;
        }

        check_result = spec_data_check(spec, test_data);
        if (check_result==0 && negate==0)
            ret = 0;
        else if (check_result!=0 && negate!=0)
            ret = 0;

        if (type->free_test_data)
            type->free_test_data(req, test_data);

        if (ret == 0)
            return 0;

        spec_list=spec_list->next;
    }
    return 1;
}

int ci_access_entry_match_request(ci_access_entry_t *access_entry, ci_request_t *req)
{
    struct ci_specs_list *spec_list;

    if (!access_entry)
        return CI_ACCESS_ALLOW;

    while (access_entry) {
        ci_debug_printf(9,"Check request with an access entry\n");
        spec_list = access_entry->spec_list;
        if (spec_list && spec_list->spec && request_match_specslist(req, spec_list))
            return access_entry->type;

        access_entry=access_entry->next;
    }
    return CI_ACCESS_UNKNOWN;
}


/**********************************************************************************/
/* acl library functions                                                          */

static struct ci_acl_type_list types_list;
static struct ci_acl_spec *specs_list;

static int acl_load_defaults()
{
    ci_acl_typelist_add(&types_list, &acl_tcp_port);
    ci_acl_typelist_add(&types_list, &acl_service);
    ci_acl_typelist_add(&types_list, &acl_req_type);
    ci_acl_typelist_add(&types_list, &acl_user);
    ci_acl_typelist_add(&types_list, &acl_tcp_src);
    ci_acl_typelist_add(&types_list, &acl_tcp_srvip);
#if defined(USE_REGEX)
    ci_acl_typelist_add(&types_list, &acl_icap_header);
    ci_acl_typelist_add(&types_list, &acl_icap_resp_header);
    ci_acl_typelist_add(&types_list, &acl_http_req_header);
    ci_acl_typelist_add(&types_list, &acl_http_resp_header);
    ci_acl_typelist_add(&types_list, &acl_http_req_url);
    ci_acl_typelist_add(&types_list, &acl_http_req_line);
    ci_acl_typelist_add(&types_list, &acl_http_resp_line);
#endif
    ci_acl_typelist_add(&types_list, &acl_http_req_method);
    ci_acl_typelist_add(&types_list, &acl_data_type);
    ci_acl_typelist_add(&types_list, &acl_content_length);
    ci_acl_typelist_add(&types_list, &acl_time);
    ci_acl_typelist_add(&types_list, &acl_tcp_xclientip);

    return 1;
}

void ci_acl_init()
{
    ci_acl_typelist_init(&types_list);
    acl_load_defaults();
    specs_list = NULL;
}

void ci_acl_reset()
{
    ci_acl_spec_list_release(specs_list);
    specs_list = NULL;
    ci_acl_typelist_reset(&types_list);
    acl_load_defaults();
}

const ci_acl_spec_t *ci_acl_search(const char *name)
{
    return (const ci_acl_spec_t *)ci_acl_spec_search(specs_list, name);
}

const ci_acl_type_t *ci_acl_type_search(const char *name)
{
    return ci_acl_typelist_search(&types_list, name);
}

int ci_acl_type_add(const ci_acl_type_t *type)
{
    return ci_acl_typelist_add(&types_list, type);
}

int ci_acl_add_data(const char *name, const char *type, const char *data)
{
    ci_acl_spec_t *spec;
    const ci_acl_type_t *spec_type;
    char *s, *param = NULL, *acl_type;

    acl_type = strdup(type);
    if (!acl_type) {
        ci_debug_printf(1, "cfg_acl_add: error strduping!\n");
        return 0;
    }

    s = acl_type;
    if ((s=strchr(s,'{')) != NULL) {
        *s='\0';
        param=s+1;
        if ((s=strchr(param,'}')) != NULL)
            *s= '\0';
    }

    if ((spec = ci_acl_spec_search(specs_list, name)) != NULL) {
        spec_type = ci_acl_type_search(acl_type);
        if (spec_type != spec->type) {
            ci_debug_printf(1, "The acl type:%s does not match with type of existing acl \"%s\"",
                            acl_type, name);
            free(acl_type);
            return 0;
        }
    } else {
        spec = ci_acl_spec_new(name, acl_type, param, &types_list, &specs_list);
    }
    free(acl_type);

    if (!spec) {
        ci_debug_printf(1, "Error in acl:%s! Maybe the acl type \"%s\" does not exists!\n",
                        name, acl_type);
        return 0;
    }
    ci_acl_spec_new_data(spec, data);

    return 1;
}

/******************************************************/
/* Some acl_type methods implementation               */
#if defined(USE_REGEX)

const char *get_header(ci_headers_list_t *headers, char *head)
{
    const char *val;
    char *buf;
    size_t value_size = 0;

    if (!headers || !head)
        return NULL;

    if (!(val = ci_headers_value2(headers, head, &value_size)))
        return NULL;

    if (!headers->packed) /*The headers are not packed, so it is NULL terminated*/
        return val;

    /*assume that an 8k buffer is enough for a header value*/
    if (!(buf = ci_buffer_alloc(value_size + 1)))
        return NULL;

    memcpy(buf, val, value_size);
    buf[value_size] = '\0';

    return buf;
}

void release_header_value(ci_headers_list_t *headers, char *head)
{
    if (headers && headers->packed && head) /*The headers are packed, we have allocated buffer*/
        ci_buffer_free(head);
}

void *get_icap_header(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    heads = req->request_header;
    return (void *)get_header(heads, param);
}

void free_icap_header(ci_request_t *req, void *param)
{
    ci_headers_list_t *heads;
    heads = req->request_header;
    release_header_value(heads, param);
}

void *get_icap_response_header(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    heads = req->response_header;
    return (void *)get_header(heads, param);
}

void free_icap_response_header(ci_request_t *req, void *param)
{
    ci_headers_list_t *heads;
    heads = req->response_header;
    release_header_value(heads, param);
}

void *get_http_req_header(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    heads = ci_http_request_headers(req);
    return (void *)get_header(heads, param);
}
void free_http_req_header(ci_request_t *req, void *param)
{
    ci_headers_list_t *heads;
    heads = ci_http_request_headers(req);
    release_header_value(heads, param);
}

void *get_http_resp_header(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    heads = ci_http_response_headers(req);
    return (void *)get_header(heads, param);
}

void free_http_resp_header(ci_request_t *req, void *param)
{
    ci_headers_list_t *heads;
    heads = ci_http_response_headers(req);
    release_header_value(heads, param);
}

void *get_http_req_url(ci_request_t *req, char *param)
{
    char *buf;
    ci_headers_list_t *heads;
    heads = ci_http_request_headers(req);
    if (!heads)
        return NULL;
    buf = ci_buffer_alloc(8192);
    ci_http_request_url(req, buf, 8192);
    return buf;
}
void free_http_req_url(ci_request_t *req, void *data)
{
    ci_buffer_free((char *)data);
}

void *get_http_req_line(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    size_t first_line_size;
    const char *first_line;
    char *buf;
    heads = ci_http_request_headers(req);
    if (!heads)
        return NULL;

    first_line = ci_headers_first_line2(heads, &first_line_size);

    if (!first_line || first_line_size == 0)
        return NULL;

    if (!heads->packed) /*The headers are not packed, so it is NULL terminated*/
        return (void *)first_line;

    buf = ci_buffer_alloc(first_line_size + 1);
    memcpy(buf, first_line, first_line_size);
    buf[first_line_size] = '\0';
    return buf;
}

void free_http_req_line(ci_request_t *req, void *data)
{
    ci_headers_list_t *heads;
    heads = ci_http_request_headers(req);
    if (heads->packed)
        ci_buffer_free(data);
}

void *get_http_resp_line(ci_request_t *req, char *param)
{
    size_t first_line_size;
    const char *first_line;
    char *buf;
    ci_headers_list_t *heads;
    heads = ci_http_response_headers(req);
    if (!heads)
        return NULL;

    first_line = ci_headers_first_line2(heads, &first_line_size);

    if (!first_line || first_line_size == 0)
        return NULL;

    if (!heads->packed) /*The headers are not packed, so it is NULL terminated*/
        return (void *)first_line;

    buf = ci_buffer_alloc(first_line_size + 1);
    memcpy(buf, first_line, first_line_size);
    buf[first_line_size] = '\0';
    return buf;
}

void free_http_resp_line(ci_request_t *req, void *data)
{
    ci_headers_list_t *heads;
    heads = ci_http_response_headers(req);
    if (heads->packed)
        ci_buffer_free(data);
}
#endif

void *get_http_req_method(ci_request_t *req, char *param)
{
    ci_headers_list_t *heads;
    size_t found_size;
    const char *first_line, *e, *eol;
    char *buf;
    heads = ci_http_request_headers(req);
    if (!heads)
        return NULL;

    first_line = ci_headers_first_line2(heads, &found_size);
    if (!first_line || found_size == 0)
        return NULL;
    eol = first_line + found_size;

    for (e = first_line; e < eol && !isspace(*e); e++);
    if (e == eol) /*No method found*/
        return NULL;
    found_size = e - first_line;
    if ((buf = ci_buffer_alloc(found_size + 1))) {
        memcpy(buf, first_line, found_size);
        buf[found_size] = '\0';
    }
    return buf;
}

void free_http_req_method(ci_request_t *req, void *data)
{
    if (data)
        ci_buffer_free(data);
}

void *get_data_type(ci_request_t *req, char *param)
{
    int type, isenc;
    int *ret_type;
    type = ci_magic_req_data_type(req, &isenc);
    if (type < 0)
        return NULL;

    ret_type = malloc(sizeof(unsigned int));
    if (!ret_type)
        return NULL;

    *ret_type = type;
    return (void *)ret_type;
}

void free_data_type(ci_request_t *req,void *param)
{
    free(param);
}

void *get_content_length(ci_request_t *req, char *param)
{
    struct acl_cmp_uint64_data *clen_p = (struct acl_cmp_uint64_data *)ci_buffer_alloc(sizeof(struct acl_cmp_uint64_data));
    ci_off_t clen = ci_http_content_length(req);
    if (clen < 0)
        return NULL;
    clen_p->data = (uint64_t)clen;
    if (param[0] == '=') {
        clen_p->operator = 0;
    } else if (param[0] == '>') {
        clen_p->operator = 1;
    } else if (param[0] == '<') {
        clen_p->operator = 2;
    }
    return clen_p;
}

void free_cmp_uint64_data(ci_request_t *req, void *param)
{
    ci_buffer_free(param);
}

