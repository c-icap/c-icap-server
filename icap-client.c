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


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "request.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"
 
//char *servername;
//char *modulename;

int TIMEOUT=300;




int create_request(request_t *req,char *servername,char *service,int reqtype){
    char buf[256];    

    if(reqtype!=ICAP_OPTIONS && reqtype!=ICAP_REQMOD && reqtype!=ICAP_RESPMOD)
	return CI_ERROR;

    req->type=reqtype;
    snprintf(buf,255,"%s icap://%s/%s ICAP/1.0",
	     ci_method_string(reqtype),servername,service);
    buf[255]='\0';
    add_header(req->head,buf);
    snprintf(buf,255,"Host: %s",servername);
    buf[255]='\0';
    add_header(req->head,buf);
    add_header(req->head,"User-Agent: C-ICAP-Client-Library/0.1");
    return CI_OK;
}

int ci_get_request_options(request_t *req,ci_header_list_t *h){
    char *pstr;

    if((pstr=get_header_value(h, "Preview"))!=NULL){
	req->preview=strtol(pstr,NULL,10);
	if(req->preview<0)
	    req->preview=0;
    }
    else
	req->preview=0;

    
    req->allow204=0;
    if((pstr=get_header_value(h, "Allow"))!=NULL){
	if(strtol(pstr,NULL,10) == 204)
	    req->allow204=1;
    }

    req->keepalive=1;
    if((pstr=get_header_value(h, "Connection"))!=NULL && strncmp(pstr,"close",5)==0){
	req->keepalive=0;
    }

    /*Moreover we are interested for the followings*/
    if((pstr=get_header_value(h, "Transfer-Preview"))!=NULL){
	/*Not implemented yet*/
    }

    if((pstr=get_header_value(h, "Transfer-Ignore"))!=NULL){
	/*Not implemented yet*/
    }
	
    if((pstr=get_header_value(h, "Transfer-Complete"))!=NULL){
	/*Not implemented yet*/
    }

    /*
      The headers Max-Connections and  Options-TTL are not needed in this client 
      but if this functions moves to a general client api must be implemented
    */
    
    return CI_OK;
}



int send_request(request_t *req){
    char *buf;
    int remains,ret;

    ci_headers_pack(req->head);
    remains=sizeofheader(req->head);
    buf=req->head->buf;
    while(remains){
	ret=ci_write(req->connection->fd,buf,remains,0);
	buf+=ret;
	remains-=ret;
    }
    return 1;
}

int get_responce(request_t *req){
    char buf[512];
    int i,len;
    printf("\n\n---RESPONCE----\n");
    ci_read_icap_header(req,req->responce_head,0);
    ci_headers_unpack(req->responce_head);
    ci_get_request_options(req,req->responce_head);

    printf("OPTIONS:\n");
    printf("\tAllow 204:%s\n\tPreview:%d\n\tKeep alive:%s\n",
	   (req->allow204?"Yes":"No"),
	   req->preview,
	   (req->keepalive?"Yes":"No")
	   );

    printf("HEADERS:\n");
    
    for(i=0;i<req->responce_head->used;i++){
	printf("\t%s\n",req->responce_head->headers[i]);
    }
    printf("BODY:\n");
     while((len=ci_read(req->connection->fd,buf,512,0))>0){
	 buf[len]='\0';
	 printf("\t%s",buf);
     }
     return 1;
}


int get_options(request_t *req,char *icap_server,char *service){
    
    if(CI_OK!=create_request(req,icap_server,service,ICAP_OPTIONS))
	return CI_ERROR;
    send_request(req);
    ci_read_icap_header(req,req->responce_head,0);
    ci_headers_unpack(req->responce_head);
    ci_get_request_options(req,req->responce_head);
    
    return CI_OK;
}



char *icap_server="localhost";
char *service="echo";
char *input_file=NULL;
char *output_file=NULL;
int RESPMOD=1;

static struct options_entry options[]={
     {"-i","icap_servername",&icap_server,ci_cfg_set_str,"The icap server name"},
     {"-f","filename",&input_file,ci_cfg_set_str,"Send this file to the icap server.\nDefault is to send an options request"},
     {"-o","filename",&output_file,ci_cfg_set_str,"Save output to this file.\nDefault is to send to the stdout"},
     {"-req",NULL,&RESPMOD,ci_cfg_disable,"Send a request modification instead of responce modification"},
     {"-d","level", &CI_DEBUG_LEVEL,ci_cfg_enable,"Print debug info to stdout"},
     {NULL,NULL,NULL,NULL}
};

void log_errors(request_t *req, const char *format,... ){
     va_list ap;
     va_start(ap,format);
     vfprintf(stderr,format,ap);
     va_end(ap);
}

void vlog_errors(request_t *req, const char *format, va_list ap){
    vfprintf(stderr,format,ap);
}

int ci_host_to_sockaddr_t(char *servername,ci_sockaddr_t *addr,int proto){
    int ret=0;
    struct addrinfo hints,*res;
    memset(&hints,0,sizeof(hints));
    hints.ai_family=proto;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=0;
    if((ret=getaddrinfo(servername,NULL,&hints,&res))!=0){
	ci_debug_printf(1,"Error geting addrinfo return was:%d\n",ret);
	return 0;
    }
    //fill the addr..... and 
    memcpy(&(addr->sockaddr),res->ai_addr,CI_SOCKADDR_SIZE);
    freeaddrinfo(res);
    return 1;
}


ci_connection_t *ci_connect_to(char *servername,int port,int proto){
    ci_connection_t *connection=malloc(sizeof(ci_connection_t));
    char hostname[CI_MAXHOSTNAMELEN+1];
    int addrlen=0;
    if(!connection)
	return NULL;
    connection->fd = socket(proto, SOCK_STREAM, 0);
    if(connection->fd == -1){
	ci_debug_printf(1,"Error oppening socket ....\n");
	free(connection);
	return NULL;
    }

    if(!ci_host_to_sockaddr_t(servername,&(connection->srvaddr),proto)){
	free(connection);
	return NULL;	
    }
    ci_sockaddr_set_port(&(connection->srvaddr),port);

    if(connect(connection->fd, (struct sockaddr *)&(connection->srvaddr.sockaddr), CI_SOCKADDR_SIZE)){
	ci_sockaddr_t_to_host(&(connection->srvaddr), hostname, CI_MAXHOSTNAMELEN);
	ci_debug_printf(1,"Error connecting to socket (host: %s) .....\n",hostname);
	free(connection);
	return NULL;
    }
     
    addrlen=CI_SOCKADDR_SIZE;
    getsockname(connection->fd,(struct sockaddr *)&(connection->claddr.sockaddr),&addrlen);
    ci_fill_sockaddr(&(connection->claddr));
    ci_fill_sockaddr(&(connection->srvaddr));
    
    ci_netio_init(connection->fd);
    return connection;
}



int do_send_file_request(request_t *req,char *icap_server,char *service,char *file){

    if(CI_OK!=create_request(req,icap_server,service,ICAP_RESPMOD))
	return CI_ERROR;
    
    send_request(req);
    ci_read_icap_header(req,req->responce_head,0/*timeout 0 = for ever*/);
    ci_headers_unpack(req->responce_head);
    ci_get_request_options(req,req->responce_head);
    
    return CI_OK;
    
}



int main(int argc, char **argv){
//     int fd;
     int i,port=1344;
     ci_connection_t *conn;
     request_t *req;

     CI_DEBUG_LEVEL=1; /*Default debug level is 1*/

     if(!ci_args_apply(argc,argv,options)){
	 ci_args_usage(argv[0],options);
	 exit(-1);
     }

#if ! defined(_WIN32)
     __log_error=(void(*)(void *, const char *,... ))log_errors; /*set c-icap library log  function*/
#else
     __vlog_error=vlog_errors; /*set c-icap library  log function for win32.....*/
#endif


     if(!(conn=ci_connect_to(icap_server,port,AF_INET))){
	 ci_debug_printf(1,"Failed to connect to icap server.....\n");
	 exit(-1);
     }

     req=ci_request_alloc(conn);

     get_options(req,icap_server,service);


     if(!input_file){
	 printf("OPTIONS:\n");
	 printf("\tAllow 204:%s\n\tPreview:%d\n\tKeep alive:%s\n",
		(req->allow204?"Yes":"No"),
		req->preview,
		(req->keepalive?"Yes":"No")
	     );
	 printf("HEADERS:\n");
	 for(i=0;i<req->responce_head->used;i++){
	     printf("\t%s\n",req->responce_head->headers[i]);
	 }
     }
     else{
	 ci_request_reset(req);
	 do_send_file_request(req,icap_server,service,input_file);
     }
     close(conn->fd);      
     return 0;
}
