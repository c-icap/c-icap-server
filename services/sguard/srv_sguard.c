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



int sguard_init_service(service_module_t *serv);
void *sguard_init_request_data(service_module_t *serv,request_t *req);
void sguard_end_of_headers_handler(void *data,request_t *req);
int sguard_process(void *data,request_t *);
int sguard_check_preview(void *data, request_t *);


char *sguard_options[]={
     "Allow: 204",
     "Transfer-Preview: *",
     "Encapsulated: null-body=0",
     NULL
};


//service_module echo={
CI_DECLARE_MOD_DATA service_module_t service={
     "sguard",
     "Sguard demo service",
     ICAP_REQMOD,
     sguard_options,
     NULL,/* Options body*/
     sguard_init_service, /* Init_service*/
     NULL, /*close_Service*/
     sguard_init_request_data,/* init_request_data*/
     (void (*)(void *))freemembody, /*Release request data*/

     sguard_end_of_headers_handler,
     NULL,
     sguard_process,
     (int (*)(void *, char *,int,int))writememdata,
     (int (*)(void *,char *,int))readmemdata,
     NULL,
     NULL
};


int sguard_init_service(service_module_t *serv){
     printf("Initialization of sguard module......\n");
}


void *sguard_init_request_data(service_module_t *serv,request_t *req){
     if(ci_req_hasbody(req))
	  return newmembody();
     return NULL;
}




void get_http_info(request_t *req,ci_header_list_t *req_header /*, struct httpInfo *s */){
}


void sguard_end_of_headers_handler(void *data,request_t *req){

     /*  struct httpInfo httpinf;*/
     ci_header_list_t* req_header;

     ci_req_reqmod_add_header(req,"Via: C-ICAP  0.01/sguard");
     if((req_header=ci_req_reqmod_headers(req))!=NULL){
	  get_http_info(req,req_header /*,&httpinf*/);
     }
     
     unlock_data(req);
}



int sguard_check_preview(void *b, request_t *req){
     return EC_100;
}



int sguard_process(void *b,request_t *req){

/*
     if(b){
	  markendofdata((struct mem_body *)b);
	  printf("Buffer size=%d, Data size=%d\n ",
		 ((struct mem_body *)b)->bufsize,((struct mem_body *)b)->endpos);
     }
*/  
     return CI_MOD_DONE;     
}




