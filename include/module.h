/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
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


#ifndef __C_ICAP_MODULE_H
#define __C_ICAP_MODULE_H

#include <stdarg.h>
#include "c-icap.h"
#include "request.h"
#include "header.h"
#include "service.h"
#include "cfg_param.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct ci_request;

#define CI_MOD_DISABLE_FORCE_UNLOAD_STR "__c_icap_module_disable_force_unload"
#define CI_MOD_DISABLE_FORCE_UNLOAD() CI_DECLARE_MOD_DATA const char * __c_icap_module_disable_force_unload = "off;"

enum module_type {
    UNKNOWN,
    SERVICE_HANDLER,
    LOGGER,
    ACCESS_CONTROLLER,
    AUTH_METHOD,
    AUTHENTICATOR,
    COMMON,
    MODS_TABLE_END
};

typedef struct  service_handler_module {
    const char *name;
    const char *extensions;
    int (*init_service_handler)(struct ci_server_conf *server_conf);
    int (*post_init_service_handler)(struct ci_server_conf *server_conf);
    void (*release_service_handler)();
    ci_service_module_t *(*create_service)(const char *service_file, const char *argv[]);
    struct ci_conf_entry *conf_table;
} service_handler_module_t;

typedef struct common_module {
    const char *name;
    int (*init_module)(struct ci_server_conf *server_conf);
    int (*post_init_module)(struct ci_server_conf *server_conf);
    void (*close_module)();
    struct ci_conf_entry *conf_table;
} common_module_t;

typedef struct  logger_module {
    const char *name;
    int  (*init_logger)(struct ci_server_conf *server_conf);
    int  (*log_open)(); /*Or better post_init_logger .......*/
    void (*log_close)();
    void  (*log_access)(ci_request_t *req);
    void  (*log_server)(const char *server, const char *format, va_list ap);
    struct ci_conf_entry *conf_table;
} logger_module_t;

typedef struct  access_control_module {
    const char *name;
    int (*init_access_controller)(struct ci_server_conf *server_conf);
    int (*post_init_access_controller)(struct ci_server_conf *server_conf);
    void (*release_access_controller)();
    int (*client_access)(ci_request_t *req);
    int (*request_access)(ci_request_t *req);
    struct ci_conf_entry *conf_table;
} access_control_module_t;


typedef struct http_auth_method {
    const char *name;
    int (*init_auth_method)(struct ci_server_conf *server_conf);
    int (*post_init_auth_method)(struct ci_server_conf *server_conf);
    void (*close_auth_method)();
    void *(*create_auth_data)(const char *authorization_header,const char **username);
    void (*release_auth_data)(void *data);
    char *(*authentication_header)(ci_request_t *req);
    void (*release_authentication_header)(const char *auth_header);
    struct ci_conf_entry *conf_table;
} http_auth_method_t;


typedef struct authenticator_module {
    const char *name;
    const char *method;
    int (*init_authenticator)(struct ci_server_conf *server_conf);
    int (*post_init_authenticator)(struct ci_server_conf *server_conf);
    void (*close_authenticator)();
    int (*authenticate)(void *data, const char *usedb);
    struct ci_conf_entry *conf_table;
} authenticator_module_t;

CI_DECLARE_FUNC(common_module_t *) ci_common_module_build( const char *name, int (*init_module)(struct ci_server_conf *server_conf), int (*post_init_module)(struct ci_server_conf *server_conf), void (*close_module)(), struct ci_conf_entry *conf_table);

static inline common_module_t *ci_common_module_t_dup_to(common_module_t *mod, common_module_t **newmod)
{
    _CI_ASSERT(mod);
    if (!(*newmod))
        *newmod = ci_common_module_build(mod->name, mod->init_module, mod->post_init_module, mod->close_module, ci_cfg_conf_table_dup(mod->conf_table));
    return *newmod;
}

#define _CI_DECLARE_MODULE(mod, module_type)                            \
    CI_DECLARE_MOD_DATA  const uint64_t __ci_build_for = C_ICAP_HEX_VERSION; \
    static module_type *__ci_module_handle = NULL;                      \
    CI_DECLARE_MOD_DATA module_type *__ci_module_build() {              \
        return ci_ ## module_type ## _dup_to(&mod, &__ci_module_handle); \
    }

#define _CI_DECLARE_COMMON_MODULE(mod) _CI_DECLARE_MODULE(mod, common_module_t)

int init_modules();
int post_init_modules();
void * register_module(const char *module_file, const char *type, const char *argv[]);

logger_module_t *find_logger(const char *name);
access_control_module_t *find_access_controller(const char *name);
service_handler_module_t *find_servicehandler(const char *name);
service_handler_module_t *find_servicehandler_by_ext(const char *extension);
http_auth_method_t *find_auth_method_n(const char *method, int len, int *method_id);
http_auth_method_t * get_authentication_schema(const char *method_name, authenticator_module_t ***authenticators);

void *find_module(const char *name,int *type);

int set_method_authenticators(const char *method_name, const char **argv);

#ifdef __cplusplus
}
#endif

#endif
