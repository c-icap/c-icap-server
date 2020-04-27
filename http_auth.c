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
#include "c-icap.h"
#include "request.h"
#include "access.h"
#include "encoding.h"
#include "module.h"
#include "acl.h"
#include "lookup_table.h"
#include "debug.h"

char *DEFAULT_AUTH_METHOD="basic";
char *REMOTE_PROXY_USER_HEADER = "X-Authenticated-User";
int ALLOW_REMOTE_PROXY_USERS = 0;
int REMOTE_PROXY_USER_HEADER_ENCODED = 1;

/*To be repleced by http_authenticate .......*/
int http_authorize(ci_request_t * req, char *method)
{
    return http_authenticate(req, method);
    ci_debug_printf(1, "Allowing http_access.....\n");
    return CI_ACCESS_ALLOW;
}


int call_authenticators(authenticator_module_t ** authenticators,
                        void *method_data)
{
    int i, res;
    for (i = 0; authenticators[i] != NULL; i++) {
        if ((res =
                    authenticators[i]->authenticate(method_data, NULL)) !=
                CI_ACCESS_UNKNOWN) {
            return res;
        }
    }
    return CI_ACCESS_DENY;
}


int http_authenticate(ci_request_t * req, char *use_method)
{
    struct http_auth_method *auth_method;
    authenticator_module_t **authenticators;
    void *method_data;
    const char *auth_str, *method_str, *username;
    char *auth_header = NULL;
    int len, res;

    if (ALLOW_REMOTE_PROXY_USERS && !use_method) {
        username = ci_headers_value(req->request_header, REMOTE_PROXY_USER_HEADER);
        if (username) {
            if (REMOTE_PROXY_USER_HEADER_ENCODED)
                ci_base64_decode(username, req->user, MAX_USERNAME_LEN);
            else {
                strncpy(req->user, username, MAX_USERNAME_LEN);
                req->user[MAX_USERNAME_LEN] = '\0';;
            }
            return CI_ACCESS_ALLOW;
        } else {
            ci_debug_printf(3, "No user name found in ICAP header '%s'\n", REMOTE_PROXY_USER_HEADER ? REMOTE_PROXY_USER_HEADER : "-");
            return CI_ACCESS_DENY;
        }
    }

    if (!use_method)
        use_method = DEFAULT_AUTH_METHOD;

    auth_method = get_authentication_schema(use_method, &authenticators);

    if (auth_method == NULL) {
        ci_debug_printf(1, "Authentication method not found ...\n");
        return CI_ACCESS_DENY;
    }

    res = CI_ACCESS_DENY;
    if ((method_str =
                ci_headers_value(req->request_header, "Proxy-Authorization")) != NULL) {
        ci_debug_printf(5, "Str is %s ....\n", method_str);
        if ((auth_str = strchr(method_str, ' ')) == NULL)
            return CI_ACCESS_DENY;
        len = auth_str - method_str;
        if (strncmp(method_str, use_method, len) != 0)
            return CI_ACCESS_DENY;
        ci_debug_printf(5, "Method is %s ....\n", method_str);

        auth_str++;
        method_data = auth_method->create_auth_data(auth_str, &username);
        strncpy(req->user, username, MAX_USERNAME_LEN);
        req->user[MAX_USERNAME_LEN] = '\0';
        /*Call an authenticator now ........ */
        res = call_authenticators(authenticators, method_data);
        /*And release data ..... */
        auth_method->release_auth_data(method_data);
    }

    if (res == CI_ACCESS_DENY) {
        auth_header = auth_method->authentication_header();
        ci_headers_add(req->xheaders, auth_header);
        if (auth_method->release_authentication_header)
            auth_method->release_authentication_header(auth_header);
        req->auth_required = 1;
        ci_debug_printf(3, "Access denied. Authentication required!!!!!\n");
    }

    return res;
}

/******************************************************/
/* Group handling                                     */
enum {INDEX_BY_GROUP, INDEX_BY_USER};
struct group_source {
    char *name;
    int type;
    struct ci_lookup_table *db;
    struct group_source *next;
};

struct group_source *GROUPS_SOURCE = NULL;

int group_source_add(const char *table_name, int type)
{
    struct group_source *gsrc_indx, *gsrc;
    if (type<0 || type >INDEX_BY_USER) {
        ci_debug_printf(1, "Non valid group lookup DB type for DB %s! (BUG?)", table_name);
        return 0;
    }
    gsrc = malloc(sizeof(struct group_source));
    if (!gsrc) {
        ci_debug_printf(1, "Error allocating memory/add_group_source!\n");
        return 0;
    }
    gsrc->name = strdup(table_name);
    if (!gsrc->name) {
        ci_debug_printf(1, "Error strduping/add_group_source!\n");
        free(gsrc);
        return 0;
    }
    gsrc->type = type;
    gsrc->db = NULL;
    gsrc->next = NULL;

    gsrc->db = ci_lookup_table_create(gsrc->name);
    if (!gsrc->db) {
        ci_debug_printf(1, "Error creating lookup table:%s!\n", gsrc->name);
        free(gsrc->name);
        free(gsrc);
        return 0;
    }
    if (!gsrc->db->open(gsrc->db)) {
        ci_debug_printf(1, "Error opening lookup table:%s!\n", gsrc->name);
        ci_lookup_table_destroy(gsrc->db);
        free(gsrc->name);
        free(gsrc);
        return 0;
    }


    if (GROUPS_SOURCE == NULL)
        GROUPS_SOURCE = gsrc;
    else {
        gsrc_indx = GROUPS_SOURCE;
        while (gsrc_indx->next != NULL)
            gsrc_indx = gsrc_indx->next;

        gsrc_indx->next = gsrc;
    }
    return 1;
}

int group_source_add_by_group(const char *table_name)
{
    return group_source_add(table_name, INDEX_BY_GROUP);
}

int group_source_add_by_user(const char *table_name)
{
    return group_source_add(table_name, INDEX_BY_USER);
}

void group_source_release()
{
    struct group_source *gsrc;
    while (GROUPS_SOURCE != NULL) {
        gsrc = GROUPS_SOURCE;
        GROUPS_SOURCE= GROUPS_SOURCE->next;
        free(gsrc->name);
        if (gsrc->db) {
            ci_lookup_table_destroy(gsrc->db);
        }
        free(gsrc);
    }
}



int check_user_group(const char *user, const char *group)
{
    struct group_source *gsrc;
    char **users, **groups;
    void *ret;
    int i;
    gsrc = GROUPS_SOURCE;
    while (gsrc != NULL) {
        if (!gsrc->db) {
            ci_debug_printf(1,"The lookup-table in group source %s is not open!", gsrc->name);
            return 0;
        }
        if (gsrc->type == INDEX_BY_USER) {
            ret = gsrc->db->search(gsrc->db, (char *)user, (void ***)&groups);
            if (ret) {
                for (i = 0; groups[i] != NULL; i++) {
                    if (strcmp(group, groups[i]) == 0) {
                        gsrc->db->release_result(gsrc->db, (void **)groups);
                        return 1;
                    }
                }
                gsrc->db->release_result(gsrc->db, (void **)groups);
                return 0;  /*user found but does not attached to group */
            }
        } else {
            ret = gsrc->db->search(gsrc->db, (char *)group, (void ***)&users);
            if (ret) {
                for (i = 0; users[i] != NULL; i++) {
                    if (strcmp(user, users[i]) == 0) {
                        gsrc->db->release_result(gsrc->db, (void **)users);
                        return 1;
                    }
                }
                gsrc->db->release_result(gsrc->db, (void **)users);
                return 0; /*group found but user does not belong to this group*/
            }
        }
        gsrc = gsrc->next;
    }
    return 0;
}

/******************************************************/
/* The group acl implementation                       */

/*defined in types_ops*/
static void *group_dup(const char *str, ci_mem_allocator_t *allocator)
{
    const size_t str_size = strlen(str) + 1;
    char *new_s = allocator->alloc(allocator, str_size);
    if (new_s) {
        strncpy(new_s, str, str_size);
        new_s[str_size] = '\0';
    }
    return new_s;
}

static void group_free(void *key, ci_mem_allocator_t *allocator)
{
    allocator->free(allocator, key);
}

static size_t group_len(const void *key)
{
    return strlen((const char *)key)+1;
}

int group_cmp(const void *key1,const void *key2)
{
    const char *group, *user;
    if (!key2)
        return -1;

    group = (const char *)key1;
    user = (const char *)key2;
    return check_user_group(user, group);
}
int group_equal(const void *key1,const void *key2)
{
    const char *group, *user;
    if (!key2)
        return 0;

    group = (const char *)key1;
    user = (const char *)key2;
    return check_user_group(user, group);
}

ci_type_ops_t  ci_group_ops = {
    group_dup,
    group_free,
    group_cmp,
    group_len,
    group_equal,
};

void *get_auth(ci_request_t *req, char *param);
void free_user(ci_request_t *req, void *data)
{

}


ci_acl_type_t acl_group = {
    "group",
    get_auth,
    free_user,
    &ci_group_ops
};



/******************************************************/
/* The auth acl implementation                        */

void *get_auth(ci_request_t *req, char *param);
void free_auth(ci_request_t *req,void *data);

ci_acl_type_t acl_auth = {
    "auth",
    get_auth,
    free_auth,
    &ci_str_ext_ops
};


void *get_auth(ci_request_t *req, char *param)
{
    int res;
    if ((res = http_authorize(req, param)) != CI_ACCESS_ALLOW) {
        return NULL;
    }

    ci_debug_printf(5, "Authenticated user: %s\n", req->user);
    return req->user;
}

void free_auth(ci_request_t *req, void *data)
{

}


void init_http_auth()
{
    ci_acl_type_add(&acl_auth);
    ci_acl_type_add(&acl_group);
}

void reset_http_auth()
{
    group_source_release();
    ci_acl_type_add(&acl_auth);
    ci_acl_type_add(&acl_group);
}

/**********************************************************************************/
/* basic auth method implementation                                               */

static char *basic_realm = "Basic authentication";
static char *basic_authentication = NULL;

/*Configuration Table .....*/
static struct ci_conf_entry basic_conf_params[] = {
    {"Realm", &basic_realm, ci_cfg_set_str, NULL},
    {NULL, NULL, NULL, NULL}
};

int basic_post_init(struct ci_server_conf *server_conf);
void basic_close();
struct http_basic_auth_data *basic_create_auth_data(const char *auth_line,
        const char **username);
void basic_release_auth_data(struct http_basic_auth_data *data);
char *basic_authentication_header();

http_auth_method_t basic_auth = {
    "basic",
    NULL,                      /*init */
    basic_post_init,
    basic_close,
    (void *(*)(const char *, const char **)) basic_create_auth_data,
    (void (*)(void *)) basic_release_auth_data,
    basic_authentication_header,
    NULL,   /*release_authentication_header*/
    basic_conf_params
};

#define BASIC_HEAD_PREFIX  "Proxy-Authenticate: Basic realm="
int basic_post_init(struct ci_server_conf *server_conf)
{
    int size = strlen(BASIC_HEAD_PREFIX)+strlen(basic_realm)+3;
    basic_authentication = malloc((size+1)*sizeof(char));

    if (!basic_authentication)
        return 0;

    snprintf(basic_authentication, size, "%s\"%s\"", BASIC_HEAD_PREFIX, basic_realm);
    return 1;
}

void basic_close()
{
    if (basic_authentication) {
        free(basic_authentication);
        basic_authentication = NULL;
    }
}

struct http_basic_auth_data *basic_create_auth_data(const char *auth_line,
        const char **username)
{
    struct http_basic_auth_data *data;
    char dec_http_user[MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2], *str;
    int max_decode_len = MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2;

    data = malloc(sizeof(struct http_basic_auth_data));

    ci_base64_decode(auth_line, dec_http_user, max_decode_len);
    ci_debug_printf(5, "The proxy user is:%s (basic method) \n", dec_http_user);

    if ((str = strchr(dec_http_user, ':')) != NULL) {
        *str = '\0';
        str++;
        strncpy(data->http_pass, str, HTTP_MAX_PASS_LEN);
        data->http_pass[HTTP_MAX_PASS_LEN - 1] = '\0';
    }
    strncpy(data->http_user, dec_http_user, MAX_USERNAME_LEN);
    data->http_user[MAX_USERNAME_LEN] = '\0';
    if (username)
        *username = data->http_user;
    return data;
}


void basic_release_auth_data(struct http_basic_auth_data *data)
{
    free(data);
}

char *basic_authentication_header()
{
    return basic_authentication;
}

/***********************************************************************/
/*file_basic authenticator .......                                     */

char *USERS_DB_PATH = NULL;
int PASSWORDS_ENCRYPTED = 1;

struct ci_lookup_table *users_db = NULL;

/*Configuration Table .....*/
static struct ci_conf_entry basic_simple_db_conf_variables[] = {
    {"UsersDB", &USERS_DB_PATH, ci_cfg_set_str, NULL},
    {NULL, NULL, NULL, NULL}
};

int basic_simple_db_post_init(struct ci_server_conf *server_conf);
void basic_simple_db_close();
int basic_simple_db_athenticate(struct http_basic_auth_data *data, const char *usedb);


authenticator_module_t basic_simple_db = {
    "basic_simple_db",              /* char *name; */
    "basic",                   /* char *method; */
    NULL,                      /* int (*init_authenticator)(); */
    basic_simple_db_post_init,      /* int (*post_init_authenticator)(); */
    basic_simple_db_close,          /* void (*close_authenticator)(); */
    (int (*)(void *, const char *)) basic_simple_db_athenticate,  /* int (*authenticate)(void *data); */
    basic_simple_db_conf_variables  /* struct ci_conf_entry *conf_table; */
};


int basic_simple_db_post_init(struct ci_server_conf *server_conf)
{
    if (USERS_DB_PATH) {
        users_db = ci_lookup_table_create(USERS_DB_PATH);
        if (!users_db->open(users_db)) {
            ci_lookup_table_destroy(users_db);
            users_db = NULL;
        }
    }
    return 1;
}

void basic_simple_db_close()
{
    if (users_db) {
        ci_lookup_table_destroy(users_db);
        users_db = NULL;
    }
}

/*
 This function is under construction ........
*/

int basic_simple_db_athenticate(struct http_basic_auth_data *data, const char *usedb)
{
    char **pass = NULL;
#ifdef HAVE_CRYPT_R
    char *pass_enc;
    struct crypt_data crypt_data;
#endif
    void *user_ret;
    int ret = CI_ACCESS_ALLOW;
    ci_debug_printf(1, "Going to authenticate user %s:%s\n", data->http_user,
                    data->http_pass);

    if (!users_db)
        return CI_ACCESS_DENY;

    user_ret = users_db->search(users_db, data->http_user, (void ***)&pass);
    if (!user_ret)
        return CI_ACCESS_DENY;

    if (!pass || !pass[0]) {
        if (data->http_pass[0] != '\0')
            ret = CI_ACCESS_DENY;
        else
            ret = CI_ACCESS_ALLOW;
    }
#ifdef HAVE_CRYPT_R
    else if (PASSWORDS_ENCRYPTED) {
        pass_enc = crypt_r(data->http_pass, pass[0], &crypt_data);
        if (strcmp(pass_enc, pass[0]) != 0)
            ret = CI_ACCESS_DENY;
    }
#endif
    else {
        if (strcmp(data->http_pass, pass[0]) != 0)
            ret = CI_ACCESS_DENY;
    }

    users_db->release_result(users_db, (void **)pass);

    ci_debug_printf(4, "User %s, %s\n", data->http_user,
                    ret == CI_ACCESS_ALLOW?"authenticated":"authentication/authorization fails");

    return ret;

    /*
         if (strcmp(data->http_user, "squiduser") != 0)
              return CI_ACCESS_UNKNOWN;
         if (strcmp(data->http_pass, "123") != 0)
              return CI_ACCESS_DENY;

         ci_debug_printf(1, "User authenticated ........\n");
    */
    return CI_ACCESS_ALLOW;
}
