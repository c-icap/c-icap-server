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


#ifndef __MODULE_H
#define __MODULE_H

#include <stdarg.h>
#include "request.h"
#include "header.h"
#include "service.h"


struct request;

enum module_type{
     UNKNOWN,
     SERVICE_HANDLER, 
     LOGGER,
     AUTH,
};

typedef struct  service_handler_module{
     char *name;
     char *extensions;
     int (*init_service_handler)();
     service_module_t *(*create_service)(char *service_file);
     struct conf_entry *conf_table;
} service_handler_module_t;


typedef struct  logger_module{
     char *name;
     int  (*init_logger)();
     int  (*log_open)();
     void (*log_close)();
     void  (*log_access)(char *server,char *clientname,char *method,
			 char *request, char *args, char *status);
     void  (*log_server)(char *server, const char *format, va_list ap);
     struct conf_entry *conf_table;
} logger_module_t;

typedef struct  auth_module{
     char *name;
     int (*init_authenticator)();
     int (*auth_client)(struct sockaddr_in *client_address, struct sockaddr_in *server_address);
     int (*auth_request)(/*I do not now yet.......*/);
     struct conf_entry *conf_table;
} auth_module_t;



int init_modules();
void * register_module(char *module_file,char *type);

logger_module_t *find_logger(char *name);
auth_module_t *find_authenticator(char *name);
service_handler_module_t *find_servicehandler(char *name);
service_handler_module_t *find_servicehandler_by_ext(char *extension);

void *find_module(char *name,int *type);

#endif
