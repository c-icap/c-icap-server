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
#include "cfg_param.h"


#define CI_MOD_NOT_READY  0
#define CI_MOD_DONE       1
#define CI_MOD_CONTINUE 100
#define CI_MOD_ALLOW204 204
#define CI_MOD_ERROR     -1

struct request;

typedef struct  service_module service_module_t;

struct  service_module{
     char *mod_name;
     char *mod_short_descr;
     int  mod_type;
     char **mod_options_header;
     char *mod_options_body;

     int (*mod_init_service)(service_module_t *this,struct icap_server_conf *server_conf);
     int (*mod_post_init_service)(service_module_t *this,struct icap_server_conf *server_conf);
     void (*mod_close_service)(service_module_t *this);
     void *(*mod_init_request_data)(service_module_t *this,struct request *);
     void (*mod_release_request_data)(void *module_data);

     int (*mod_check_preview_handler)(char *preview_data,int preview_data_len,struct request*);
     int (*mod_end_of_data_handler)(struct request*);
     int (*mod_service_io)(char *rbuf,int *rlen,char *wbuf,int *wlen,int iseof, struct request *);

     struct conf_entry *mod_conf_table;
     void *mod_data;
};


service_module_t * register_service(char *module_file);
service_module_t *add_service_alias(char *service_alias,char *service_name);
service_module_t *find_service(char *service_name);
int post_init_services();

#endif
