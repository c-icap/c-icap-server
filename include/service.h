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

#include "header.h"
#include "cfg_param.h"
#include "ci_threads.h"


#define CI_MOD_NOT_READY  0
#define CI_MOD_DONE       1
#define CI_MOD_CONTINUE 100
#define CI_MOD_ALLOW204 204
#define CI_MOD_ERROR     -1

#define MAX_SERVICE_NAME  63
#define MAX_SERVICE_ARGS 255
#define SRV_ISTAG_SIZE    39 /* contains the ISTag: field, the istag part 
				 of server and the istag part of service (32+7) */
#define SRV_ISTAG_POS     13 /* strlen("ISTAG: ")+6, 6 is the size of server 
				 part of istag */
#define SERVICE_ISTAG_SIZE 26 
#define XINCLUDES_SIZE    511 /* it is enough I think ....*/

#define CI_XClientIP              1
#define CI_XServerIP              2
#define CI_XSubscriberID          4
#define CI_XAuthenticatedUser     8
#define CI_XAuthenticatedGroups  16

struct ci_request;

typedef struct  ci_service_module ci_service_module_t;

typedef struct ci_service_xdata {
     char ISTag[SRV_ISTAG_SIZE+1];
     char xincludes[XINCLUDES_SIZE+1];
     char TransferPreview[MAX_HEADER_SIZE+1];
     char TransferIgnore[MAX_HEADER_SIZE+1];
     char TransferComplete[MAX_HEADER_SIZE+1];
     int preview_size;
     int allow_204;
     unsigned int xopts;
     ci_thread_rwlock_t lock;
} ci_service_xdata_t;


struct  ci_service_module{
     char *mod_name;
     char *mod_short_descr;
     int  mod_type;

     int (*mod_init_service)(ci_service_xdata_t *srv_xdata,struct icap_server_conf *server_conf);
     int (*mod_post_init_service)(ci_service_xdata_t *srv_xdata,struct icap_server_conf *server_conf);
     void (*mod_close_service)();
     void *(*mod_init_request_data)(struct ci_request *);
     void (*mod_release_request_data)(void *module_data);

     int (*mod_check_preview_handler)(char *preview_data,int preview_data_len,struct ci_request*);
     int (*mod_end_of_data_handler)(struct ci_request*);
     int (*mod_service_io)(char *rbuf,int *rlen,char *wbuf,int *wlen,int iseof, struct ci_request *);

     struct conf_entry *mod_conf_table;
     void *mod_data;
};

typedef struct service_alias {
     char alias[MAX_SERVICE_NAME+1];
     char args[MAX_SERVICE_ARGS+1];
     ci_service_module_t *service;
} service_alias_t;

/*Internal function */
ci_service_module_t * register_service(char *module_file);
service_alias_t *add_service_alias(char *service_alias,char *service_name,char *args);
ci_service_module_t *find_service(char *service_name);
service_alias_t *find_service_alias(char *service_name);
ci_service_xdata_t *service_data(ci_service_module_t *srv);
int post_init_services();
int release_services();

/*Library functions */
CI_DECLARE_FUNC(void) ci_service_data_read_lock(ci_service_xdata_t *srv_xdata);
CI_DECLARE_FUNC(void) ci_service_data_read_unlock(ci_service_xdata_t *srv_xdata);
CI_DECLARE_FUNC(void) ci_service_set_istag(ci_service_xdata_t *srv_xdata,char *istag);
CI_DECLARE_FUNC(void) ci_service_set_xopts(ci_service_xdata_t *srv_xdata, int xopts);
CI_DECLARE_FUNC(void) ci_service_add_xopts(ci_service_xdata_t *srv_xdata, int xopts);
CI_DECLARE_FUNC(void) ci_service_set_transfer_preview(ci_service_xdata_t *srv_xdata,char *preview);
CI_DECLARE_FUNC(void) ci_service_set_transfer_ignore(ci_service_xdata_t *srv_xdata,char *ignore);
CI_DECLARE_FUNC(void) ci_service_set_transfer_complete(ci_service_xdata_t *srv_xdata,char *complete);
CI_DECLARE_FUNC(void) ci_service_set_preview(ci_service_xdata_t *srv_xdata, int preview);
CI_DECLARE_FUNC(void) ci_service_enable_204(ci_service_xdata_t *srv_xdata);
CI_DECLARE_FUNC(void) ci_service_add_xincludes(ci_service_xdata_t *srv_xdata, char **xincludes);

#ifdef __CI_COMPAT
#define service_module_t ci_service_module_t
#define service_extra_data_t ci_service_xdata_t

#endif


#endif
