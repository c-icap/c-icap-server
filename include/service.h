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


#ifndef __SERVICE_H
#define __SERVICE_H

#include "request.h"
#include "header.h"


#define CI_MOD_NOT_READY 0
#define CI_MOD_DONE 1
#define CI_MOD_ERROR -1

struct request;

typedef struct  service_module service_module_t;

struct  service_module{
     char *mod_name;
     char *mod_short_descr;
     int  mod_type;
     char **mod_options_header;
     char *mod_options_body;

     int (*mod_init_service)(service_module_t *this);
     void (*mod_close_service)(service_module_t *this);
     void *(*mod_init_request_data)(service_module_t *this,struct request *);
     void (*mod_release_request_data)(void *module_data);

     void (*mod_end_of_headers_handler)(void *module_data,struct request *);
     int (*mod_check_preview_handler)(void *module_data,struct request*);
     int (*mod_end_of_data_handler)(void *module_data,struct request*);
     
     int (*mod_writedata)(void *module_data, char *buf,int len,int iseof);
     int (*mod_readdata)(void *module_data,char *buf,int len);
     struct conf_entry *mod_conf_table;
     void *mod_data;
};


service_module_t * register_service(char *module_file);
service_module_t *find_service(char *service_name);

#endif
