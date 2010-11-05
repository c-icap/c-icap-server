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


#ifndef __MODULE_H
#define __MODULE_H

#include <stdarg.h>
#include "request.h"
#include "header.h"
#include "service.h"
#include "cfg_param.h"

struct ci_request;

enum module_type{
     UNKNOWN,
     SERVICE_HANDLER, 
     LOGGER,
     ACCESS_CONTROLLER,
     AUTH_METHOD,
     AUTHENTICATOR,
     COMMON,
     MODS_TABLE_END
};

typedef struct  service_handler_module{
     char *name;
     char *extensions;
     int (*init_service_handler)(struct ci_server_conf *server_conf);
     int (*post_init_service_handler)(struct ci_server_conf *server_conf);
     void (*release_service_handler)();
     ci_service_module_t *(*create_service)(char *service_file);
     struct ci_conf_entry *conf_table;
} service_handler_module_t;

typedef struct common_module{
     char *name;
     int (*init_module)(struct ci_server_conf *server_conf);
     int (*post_init_module)(struct ci_server_conf *server_conf);
     void (*close_module)();
     struct ci_conf_entry *conf_table;
} common_module_t;

typedef struct  logger_module{
     char *name;
     int  (*init_logger)(struct ci_server_conf *server_conf);
     int  (*log_open)(); /*Or better post_init_logger .......*/
     void (*log_close)();
     void  (*log_access)(ci_request_t *req);
     void  (*log_server)(char *server, const char *format, va_list ap);
     struct ci_conf_entry *conf_table;
} logger_module_t;

typedef struct  access_control_module{
     char *name;
     int (*init_access_controller)(struct ci_server_conf *server_conf);
     int (*post_init_access_controller)(struct ci_server_conf *server_conf);
     void (*release_access_controller)();
     int (*client_access)(ci_request_t *req);
     int (*request_access)(ci_request_t *req);
     struct ci_conf_entry *conf_table;
} access_control_module_t;


typedef struct http_auth_method{
     char *name;
     int (*init_auth_method)(struct ci_server_conf *server_conf);
     int (*post_init_auth_method)(struct ci_server_conf *server_conf);
     void (*close_auth_method)();
     void *(*create_auth_data)(char *authorization_header,char **username);
     void (*release_auth_data)(void *data);
     char *(*authentication_header)();
     void (*release_authentication_header)();
     struct ci_conf_entry *conf_table;
} http_auth_method_t;


typedef struct authenticator_module{
     char *name;
     char *method;
     int (*init_authenticator)(struct ci_server_conf *server_conf);
     int (*post_init_authenticator)(struct ci_server_conf *server_conf);
     void (*close_authenticator)();
    int (*authenticate)(void *data, char *usedb);
     struct ci_conf_entry *conf_table;
} authenticator_module_t;



int init_modules();
int post_init_modules();
void * register_module(char *module_file,char *type);

logger_module_t *find_logger(char *name);
access_control_module_t *find_access_controller(char *name);
service_handler_module_t *find_servicehandler(char *name);
service_handler_module_t *find_servicehandler_by_ext(char *extension);
http_auth_method_t *find_auth_method_n( char *method,int len,int *method_id);
http_auth_method_t * get_authentication_schema(char *method_name,authenticator_module_t ***authenticators);

void *find_module(char *name,int *type);

int set_method_authenticators(char *method_name,char **argv);

#endif
