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


#include "c-icap.h"
#include "service.h"
#include "module.h"
#include "header.h"
#include "body.h"
#include "debug.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"



struct perl_data{
     PerlInterpreter *perl;
};

int init_perl_handler(struct icap_server_conf *server_conf);
service_module_t *load_perl_module(char *service_file);


CI_DECLARE_DATA service_handler_module_t module={
     "perl_handler",
     ".pl,.pm,.plx",
     init_perl_handler,
     load_perl_module,
     NULL 
};



int perl_init_service(service_module_t *this,struct icap_server_conf *server_conf);
void perl_close_service(service_module_t *this);
void *perl_init_request_data(service_module_t *this,struct request *);
void *perl_release_request_data(void *data);


int perl_check_preview_handler(void *data,struct request*);
int perl_end_of_data_handler(void *data,struct request*);
     

int perl_writedata(void *data, char *,int,int,struct request*);
int perl_readdata(void *data,char *,int,struct request*);


int init_perl_handler(struct icap_server_conf *server_conf){
     
}


service_module_t *load_perl_module(char *service_file){
     service_module_t *service=NULL;
     struct perl_data *perl_data;
     char *argv[2];
     argv[0]=NULL;
     argv[1]=service_file;
     service=malloc(sizeof(service_module_t));
     perl_data=malloc(sizeof(struct perl_data));
     
     perl_data->perl=perl_alloc();/*Maybe it is better to allocate a perl interpreter per request*/
     perl_construct(perl_data->perl);
     perl_parse(perl_data->perl, NULL, 2, argv, NULL);
     perl_run(perl_data->perl);

     service->mod_data=perl_data;

     service->mod_init_service=perl_init_service;
     service->mod_close_service=perl_close_service;
     service->mod_init_request_data=perl_init_request_data;
     service->mod_release_request_data=perl_release_request_data;


     service->mod_check_preview_handler= perl_check_preview_handler;
     service->mod_end_of_data_handler=perl_end_of_data_handler;

     service->mod_writedata=perl_writedata;
     service->mod_readdata=perl_readdata;

     service->mod_name=strdup("perl_test");
     service->mod_type=ICAP_REQMOD|ICAP_RESPMOD;
     
     ci_debug_printf(1,"OK service %s loaded\n",service_file);
     return service;
}




int perl_init_service(service_module_t *this,struct icap_server_conf *server_conf){
     
}

void perl_close_service(service_module_t *this){
     
}


void *perl_init_request_data(service_module_t *this,struct request *req){

}

void *perl_release_request_data(void *data){

}


int perl_check_preview_handler(void *data,struct request *req){

}

int perl_end_of_data_handler(void *data,struct request *req){

}
     

int perl_writedata(void *data, char *buf,int len,int iseof,struct request *req){
}

int perl_readdata(void *data,char *buf,int len,struct request *req){

}
