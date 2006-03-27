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
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"

//int echo_options_responce(service_module_t *,request_t *,ci_header_list_t *);
int echo_init_service(service_module_t *serv,struct icap_server_conf *server_conf);
int echo_check_preview_handler(void *data,char *preview_data,int preview_data_len, request_t *);
int echo_end_of_data_handler(void *b,request_t *req);
void *echo_init_request_data(service_module_t *serv,request_t *req);
int echo_write(void *data, char *buf,int len ,int iseof,request_t *req);
int echo_read(void *data,char *buf,int len,request_t *req);


char *echo_options[]={
     "Preview: 1024",
     "Allow: 204",
     "Transfer-Preview: *",
     NULL
};


CI_DECLARE_MOD_DATA service_module_t service={
     "echo",  /*Module name*/
     "Echo demo service", /*Module short description*/
     ICAP_RESPMOD|ICAP_REQMOD, /*Service type responce or request modification*/
     echo_options, /*Extra options headers*/
     NULL,/* Options body*/
     echo_init_service, /*init_service.*/
     NULL,/*post_init_service*/
     NULL,/*close_service*/
     echo_init_request_data,/*init_request_data. */
     (void (*)(void *))ci_membuf_free, /*release request data*/
     echo_check_preview_handler,
     echo_end_of_data_handler,
     echo_write, 
     echo_read,
     NULL,
     NULL
};


int echo_init_service(service_module_t *serv,struct icap_server_conf *server_conf){
     printf("Initialization of echo module......\n");
     return CI_OK;
}


void *echo_init_request_data(service_module_t *serv,request_t *req){

     if(ci_req_hasbody(req))
	  return ci_membuf_new();
     return NULL;
}



static int whattodo=0;
int echo_check_preview_handler(void *data,char *preview_data,int preview_data_len, request_t *req){
     int content_len;
     content_len=ci_content_lenght(req);
     ci_debug_printf(10,"We expect to read :%d body data\n",content_len);

     if(ci_req_type(req)==ICAP_RESPMOD){
	  ci_respmod_add_header(req,"Via: C-ICAP  0.01/echo");
     }
     else if(ci_req_type(req)==ICAP_REQMOD){
	  ci_reqmod_add_header(req,"Via: C-ICAP  0.01/echo");
     }
     ci_req_unlock_data(req); /*Icap server can send data before all body has received*/
     if(!preview_data_len)
	  return EC_100;

    if(whattodo==0){
 	    whattodo=1;
	    ci_membuf_write(data,preview_data,preview_data_len,ci_req_hasalldata(req));
            return EC_100;
    }
    else{
        whattodo=0;
        return EC_204;
    }
}


int echo_end_of_data_handler(void *b,request_t *req){

     return CI_MOD_DONE;     
}


int echo_write(void *data, char *buf,int len ,int iseof,request_t *req){
     return ci_membuf_write(data,buf,len,iseof);
}

int echo_read(void *data,char *buf,int len,request_t *req){
     return ci_membuf_read(data,buf,len);
}


