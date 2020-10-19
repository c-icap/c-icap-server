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

#include "common.h"
#include "c-icap.h"
#include "service.h"
#include "module.h"
#include "header.h"
#include "body.h"
#include "debug.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"



struct perl_data {
    PerlInterpreter *perl;
};

int init_perl_handler(struct ci_server_conf *server_conf);
ci_service_module_t *load_perl_module(const char *service_file, const char **args);


CI_DECLARE_DATA service_handler_module_t module = {
    "perl_handler",
    ".pl,.pm,.plx",
    init_perl_handler,
    NULL,                      /*post_init .... */
    NULL,                      /*release handler */
    load_perl_module,
    NULL
};



int perl_init_service(ci_service_xdata_t *srv_xdata,
                      struct ci_server_conf *server_conf);
void perl_close_service();
void *perl_init_request_data(ci_request_t *);
void perl_release_request_data(void *data);


int perl_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *);
int perl_end_of_data_handler(ci_request_t *);
int perl_service_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                    ci_request_t *req);


int init_perl_handler(struct ci_server_conf *server_conf)
{
    return 0;
}


ci_service_module_t *load_perl_module(const char *service_file, const char **args)
{
    ci_service_module_t *service = NULL;
    struct perl_data *perl_data;
    char *argv[2];
    argv[0] = NULL;
    argv[1] = (char *)service_file;
    service = malloc(sizeof(ci_service_module_t));
    perl_data = malloc(sizeof(struct perl_data));

    perl_data->perl = perl_alloc();    /*Maybe it is better to allocate a perl interpreter per request */
    perl_construct(perl_data->perl);
    perl_parse(perl_data->perl, NULL, 2, argv, NULL);
    perl_run(perl_data->perl);

    service->mod_data = perl_data;

    service->mod_init_service = perl_init_service;
    service->mod_post_init_service = NULL;
    service->mod_close_service = perl_close_service;
    service->mod_init_request_data = perl_init_request_data;
    service->mod_release_request_data = perl_release_request_data;
    service->mod_check_preview_handler = perl_check_preview_handler;
    service->mod_end_of_data_handler = perl_end_of_data_handler;
    service->mod_service_io = perl_service_io;


    service->mod_name = strdup("perl_test");
    service->mod_type = ICAP_REQMOD | ICAP_RESPMOD;

    ci_debug_printf(1, "OK service %s loaded\n", service_file);
    return service;
}




int perl_init_service(ci_service_xdata_t *srv_xdata,
                      struct ci_server_conf *server_conf)
{
    return 0;
}

void perl_close_service()
{

}


void *perl_init_request_data(ci_request_t *req)
{
    return NULL;
}

void perl_release_request_data(void *data)
{

}


int perl_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *req)
{
    return EC_500;
}

int perl_end_of_data_handler(ci_request_t *req)
{
    return 0;
}


int perl_service_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                    ci_request_t *req)
{
    *rlen = 0;
    *wlen = 0;
    return CI_OK;
}
