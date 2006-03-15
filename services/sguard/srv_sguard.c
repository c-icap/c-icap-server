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



int    url_check_init_service(service_module_t *serv,struct icap_server_conf *server_conf);
void * url_check_init_request_data(service_module_t *serv,request_t *req);
void   url_check_release_data(void *data);
int    url_check_process(void *data,request_t *);
int    url_check_check_preview(void *data,char *preview_data,int preview_data_len, request_t *);
int    url_check_write(void *data, char *buf,int len ,int iseof,request_t *req);
int    url_check_read(void *data,char *buf,int len,request_t *req);


char *url_check_options[]={
     "Allow: 204",
     "Transfer-Preview: *",
     "Encapsulated: null-body=0",
     NULL
};


//service_module echo={
CI_DECLARE_MOD_DATA service_module_t service={
     "url_check",
     "Url_Check demo service",
     ICAP_REQMOD,
     url_check_options,
     NULL,/* Options body*/
     url_check_init_service, /* init_service*/
     NULL,/*post_init_service*/
     NULL, /*close_Service*/
     url_check_init_request_data,/* init_request_data*/
     url_check_release_data, /*Release request data*/
     url_check_check_preview,
     url_check_process,
     url_check_write,
     url_check_read,
     NULL,
     NULL
};


struct url_check_data{
    struct ci_membuf *body;
};

enum http_methods {HTTP_UNKNOWN=0,HTTP_GET, HTTP_POST};

struct http_info{
    int http_major;
    int http_minor;
    int method;
    char site[CI_MAXHOSTNAMELEN+1];
    char page[1024]; /*I think it is enough*/
};


int url_check_init_service(service_module_t *serv,struct icap_server_conf *server_conf){
     printf("Initialization of url_check module......\n");
     return CI_OK;
}


void *url_check_init_request_data(service_module_t *serv,request_t *req){
    struct url_check_data *uc=malloc(sizeof(struct url_check_data));
    uc->body=NULL;
    return uc; /*Get from a pool of pre-allocated structs better......*/
}


void url_check_release_data(void *data){
    struct url_check_data *uc=data;
    if(uc->body)
	ci_membuf_free(uc->body);
    free(uc); /*Return object to pool.....*/
}


int get_http_info(request_t *req,ci_header_list_t *req_header , struct http_info *httpinf){
    char *str;
    int i;
    str=req_header->headers[0];
    if(str[0]=='g' || str[0]=='G') /*Get request....*/
	httpinf->method=HTTP_GET;
    else if(str[0]=='p' || str[0]=='P') /*post request....*/
	httpinf->method=HTTP_POST;
    else{
	httpinf->method=HTTP_UNKNOWN;
	return 0;
    }
    if((str=strchr(str,' '))==NULL) /*The request must have the form:GETPOST page HTTP/X.X */
	return 0;
    i=0;
    while(*str!=' ' && *str!='\0' && i<1022) /*copy page to the struct.*/
	httpinf->page[i++]=*str++;
    httpinf->page[i]='\0';

    if(*str!=' ') /*Where is the protocol info?????*/
	return 0;
    str++;
    if(*str!='H' || *(str+4)!='/') /*Not in HTTP/X.X form*/
	return 0;
    str+=5;
    httpinf->http_major=strtol(str,&str,10);
    if(*str!='.')
	return 0;
    str++;
    httpinf->http_minor=strtol(str,&str,10);

    /*Now get the site name*/
    str=get_header_value(req_header,"Host");
    strncpy(httpinf->site,str,CI_MAXHOSTNAMELEN);
    httpinf->site[CI_MAXHOSTNAMELEN]='\0';
    
    return 1;
}


int url_check_check_preview(void *data,char *preview_data,int preview_data_len, request_t *req){
    ci_header_list_t* req_header;
    struct url_check_data *uc=data;
    struct http_info httpinf;

    ci_reqmod_add_header(req,"Via: C-ICAP  0.01/url_check");/*This type of headers must moved to global*/
    if((req_header=ci_reqmod_headers(req))==NULL) /*It is not possible but who knows .....*/
	return CI_ERROR;

    get_http_info(req,req_header, &httpinf);
    
    if(1)/*we like header*/
	return EC_204;

     /*Else the URL is not a good one so....*/
    uc->body=ci_membuf_new();
    ci_respmod_create(req,1); /*Build the responce headers*/
    ci_respmod_add_header(req,"HTTP/1.1 200 OK");
    ci_respmod_add_header(req,"Server: C-ICAP");
    ci_respmod_add_header(req,"Connection: close");
    ci_respmod_add_header(req,"Content-Type: text/html");
    ci_respmod_add_header(req,"Content-Language: en");
    
    ci_membuf_write(uc->body,"<H1>Permition deny!<H1>",23,1);
    
    unlock_data(req);
    return EC_100;
}


int url_check_process(void *b,request_t *req){

/*
	  printf("Buffer size=%d, Data size=%d\n ",
		 ((struct membuf *)b)->bufsize,((struct membuf *)b)->endpos);
*/  
     return CI_MOD_DONE;     
}


int url_check_write(void *data, char *buf,int len ,int iseof,request_t *req){
    return len;
}


int url_check_read(void *data,char *buf,int len,request_t *req){
    struct url_check_data *uc=data;
    if(uc->body)
	return ci_membuf_read(uc->body,buf,len);
    return CI_EOF;
}

