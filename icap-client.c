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
#include <ctype.h>
#include "request.h"
#include "simple_api.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"

/*Some of the following code will be moved to the icap library*/
 
int TIMEOUT=300;

request_t *ci_client_request(ci_connection_t *conn,char *server,char *service){
     request_t *req;
     req=ci_request_alloc(conn);
     strncpy(req->req_server,server,CI_MAXHOSTNAMELEN);
     req->req_server[CI_MAXHOSTNAMELEN]='\0';
     req->service=strdup(service);
     req->is_client_request=1;
     return req;
}


void ci_client_request_reuse(request_t *req){
     int i;

     if(req->args)
	  free(req->args);


     req->args=NULL;
     req->type=-1;
     ci_buf_reset(&(req->preview_data));

     req->hasbody=0;
     req->responce_hasbody=0;
     reset_header(req->head);
     req->eof_received=0;
     req->status=0;

     req->pstrblock_read=NULL;
     req->pstrblock_read_len=0;
     req->current_chunk_len=0;
     req->chunk_bytes_read=0;
     req->pstrblock_responce=NULL;
     req->remain_send_block_bytes=0;
     req->write_to_module_pending=0;
     req->data_locked=1;

     for(i=0;req->entities[i]!=NULL;i++) {
	  ci_request_release_entity(req,i);
     }

}



int create_request(request_t *req,char *servername,char *service,int reqtype){
    char buf[256];    

    if(reqtype!=ICAP_OPTIONS && reqtype!=ICAP_REQMOD && reqtype!=ICAP_RESPMOD)
	return CI_ERROR;

    req->type=reqtype;
    req->is_client_request=1;
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


int ci_writen(int fd,char *buf,int len,int timeout){
    int ret=0,remains;
    remains=len;
    while(remains){
	if((ret=ci_write(fd,buf,remains,timeout))<0)
	    return ret;
	buf+=ret;
	remains-=ret;
    }
    return len;
}



int ci_send_request_headers(request_t *req,int has_eof){
    ci_encaps_entity_t **elist,*e;
    ci_header_list_t *headers;
    int bytes;

    ci_request_pack(req);
    if(ci_writen(req->connection->fd,req->head->buf,req->head->bufused,TIMEOUT)<0)
	 return CI_ERROR;

    elist=req->entities;
    while((e=*elist++)!=NULL){
	if(e->type==ICAP_REQ_HDR || e->type==ICAP_RES_HDR){
	    headers=( ci_header_list_t *)e->entity;
	    if(ci_writen(req->connection->fd,headers->buf,headers->bufused,TIMEOUT)<0)
		 return CI_ERROR;
	}
    }

    if(req->preview>0 && req->preview_data.used >0 ){
	bytes=sprintf(req->wbuf,"%x\r\n",req->preview);
	if(ci_writen(req->connection->fd,req->wbuf,bytes,TIMEOUT)<0)
	     return CI_ERROR;
	if(ci_writen(req->connection->fd,req->preview_data.buf,req->preview,TIMEOUT)<0)
	     return CI_ERROR;
	if(has_eof){
	     if(ci_writen(req->connection->fd,"\r\n0; ieof\r\n\r\n",13,TIMEOUT)<0)
		  return CI_ERROR;
	    req->eof_received=1;
	    
	}
	else{
	     if(ci_writen(req->connection->fd,"\r\n0\r\n\r\n",7,TIMEOUT)<0)
		  return CI_ERROR;
	}
    }

    return CI_OK;
}

/*this function check if there is enough space in buffer buf ....*/
int check_realloc(char **buf,int *size,int used,int mustadded){
     char *newbuf;
     int len;
     while(*size-used < mustadded ){
	  len=*size+HEADSBUFSIZE;
	  newbuf=realloc(*buf,len); 
	  if(!newbuf){
	       return EC_500;
	  }
	  *buf=newbuf;
	  *size=*size+HEADSBUFSIZE;
     }
     return CI_OK;
}


int parse_icap_header(request_t *req,ci_header_list_t *h){
     int readed=0,eoh=0;;
     char *buf,*end;
     if(req->pstrblock_read_len<4)/*we need 4 bytes for the end of headers "\r\n\r\n" string*/
	  return CI_NEEDS_MORE;
     if((end=strstr(req->pstrblock_read,"\r\n\r\n"))!=NULL){
	  readed=end-req->pstrblock_read+4;
	  eoh=1;
     }
     else
	  readed=req->pstrblock_read_len-3;
     
     if(check_realloc(&(h->buf),&(h->bufsize),h->bufused,readed)!=CI_OK)
	  return CI_ERROR;
     
     buf=h->buf+h->bufused;
     memcpy(buf,req->pstrblock_read,readed);
     h->bufused+=readed;
     req->pstrblock_read+=readed;
     req->pstrblock_read_len-=readed;
     
     if(!eoh)
	  return CI_NEEDS_MORE;

     h->bufused-=2; /*We keep the first \r\n  of the eohead sequence and the other dropped
		      So stupid but for the time never mind....*/
     return CI_OK;
}

int process_encapsulated(request_t *req,char *buf){
     char *start;
     char *pos;
     char *end;
     int type=0,num=0,val=0;
     int hasbody=1;/*Assume that we have a resbody or reqbody or optbody*/
     start=buf+14;
     pos=start;
     while(*pos!='\0'){
	  while(!isalpha(*pos) && *pos!='\0') pos++;
	  type=get_encaps_type(pos,&val,&end);
	  if(num>5) /*In practice there is an error here .... */
	       break;
	  if(type==ICAP_NULL_BODY)
	       hasbody=0; /*We have not a body*/
	  req->entities[num++]=ci_request_alloc_entity(req,type,val);
	  pos=end;/* points after the value of entity....*/
     }
     req->hasbody=hasbody;
     return 0;
}


int parse_encaps_header(request_t *req,ci_header_list_t *h,int size){
     int remains,readed=0;
     char *buf_end=NULL;

//     readed=h->bufused;
     remains=size-h->bufused;
     if(remains<0)/*is it possible ?????*/
	  return CI_ERROR;
     if(remains==0)
	  return CI_OK;

     if(req->pstrblock_read_len>0){
	  readed=(remains>req->pstrblock_read_len?req->pstrblock_read_len:remains);
	  memcpy(h->buf,req->pstrblock_read,readed);
	  h->bufused+=readed;
	  req->pstrblock_read=(req->pstrblock_read)+readed;
	  req->pstrblock_read_len=(req->pstrblock_read_len)-readed;

     }
     
     if(h->bufused<size)
	  return CI_NEEDS_MORE;

     buf_end=h->buf+h->bufused;
     if(strncmp(buf_end-4,"\r\n\r\n",4)==0){
	  h->bufused-=2; /*eat the last 2 bytes of "\r\n\r\n"*/
	  return CI_OK;
     }
     else{
	  ci_debug_printf(1,"Error parsing encapsulated headers,"
			  "no \\r\\n\\r\\n at the end of headers:%s!\n",buf_end);
	  return CI_ERROR;
     }
     
}


int get_options(request_t *req){
    
    if(CI_OK!=create_request(req,req->req_server,req->service,ICAP_OPTIONS))
	return CI_ERROR;
    ci_send_request_headers(req,0);
    reset_header(req->head);

//    ci_read_icap_header(req,req->head,128);
    do{
	ci_wait_for_incomming_data(req->connection->fd,TIMEOUT);
	if(net_data_read(req)==CI_ERROR)
	     return CI_ERROR;
    }while(parse_icap_header(req,req->head)==CI_NEEDS_MORE);

    ci_headers_unpack(req->head);
    ci_get_request_options(req,req->head);

    return CI_OK;
}


int ci_host_to_sockaddr_t(char *servername,ci_sockaddr_t *addr,int proto){
    int ret=0;
    struct addrinfo hints,*res;
    memset(&hints,0,sizeof(hints));
    hints.ai_family=proto;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=0;
    if((ret=getaddrinfo(servername,NULL,&hints,&res))!=0){
	ci_debug_printf(1,"Error geting addrinfo\n");
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


int prepere_body_chunk(request_t *req,void *data, int (*readdata)(void *data,char *,int)){
     int chunksize,def_bytes;
     char *wbuf=NULL;
     char tmpbuf[EXTRA_CHUNK_SIZE];

     
     wbuf=req->wbuf+EXTRA_CHUNK_SIZE;/*Let size of EXTRA_CHUNK_SIZE space in the beggining of chunk*/
     if((chunksize=(*readdata)(data,wbuf,MAX_CHUNK_SIZE)) <=0){
/*	  ci_debug_printf(1,"No data to send or eof reached (%d,).......\n",chunksize);*/
	  req->remain_send_block_bytes=0;
	  return chunksize; /*Must be 0 or CI_EOF */
     }

     wbuf+=chunksize; /*Put the "\r\n" sequence at the end of chunk*/
     *(wbuf++)='\r';
     *wbuf='\n';

     def_bytes=snprintf(tmpbuf,EXTRA_CHUNK_SIZE,"%x\r\n",chunksize);
     wbuf=req->wbuf+EXTRA_CHUNK_SIZE-def_bytes;  /*Copy the chunk define in the beggining of chunk .....*/
     memcpy(wbuf,tmpbuf,def_bytes);
     
     req->pstrblock_responce=wbuf;
     req->remain_send_block_bytes=def_bytes+chunksize+2;     

     return req->remain_send_block_bytes;
}


int parse_incoming_data(request_t *req,void *data_dest,  int (*dest_write) (void *,char *,int)){
     int ret,v1,v2,status,bytes,size;
     char *buf,*val;
     ci_header_list_t *resp_heads;

     if(req->status==GET_NOTHING){
	  /*And reading the new .....*/
	  ret=parse_icap_header(req,req->head);
	  if(ret!=CI_OK)
	       return ret;
	  sscanf(req->head->buf,"ICAP/%d.%d %d",&v1,&v2,&status);
	  ci_debug_printf(3,"Responce was with status:%d \n",status);	  
	  ci_headers_unpack(req->head);

	  if((val=search_header(req->head,"Encapsulated"))==NULL){
	       ci_debug_printf(1,"No encapsulated entities!\n");
	       return CI_ERROR;
	  }
	  process_encapsulated(req,val);

	  if(!req->entities[0] || !req->entities[1])
	       return CI_ERROR;
	  size=req->entities[1]->start-req->entities[0]->start;
	  resp_heads=req->entities[0]->entity;
	  if(!set_size_header(resp_heads,size))
	       return CI_ERROR;


	  req->status=GET_HEADERS;
//	  return CI_NEEDS_MORE;
     }

     /*read encups headers */
     
     /*Non option responce has 2 entities: 
          "req-headers [req-body|null-body]" or resp-headers [resp-body||null-body]   
       So here parse_encaps_header will be called for one headers block
     */

     if(req->status==GET_HEADERS){
	  size=req->entities[1]->start-req->entities[0]->start;
	  resp_heads=req->entities[0]->entity;
	  if((ret=parse_encaps_header(req,resp_heads,size))!=CI_OK)
	       return ret;

     
	  ci_headers_unpack(resp_heads);
	  ci_debug_printf(5,"OK reading headers boing to read body\n");

	  /*reseting body chunks related variables*/	  
	  req->current_chunk_len=0;
	  req->chunk_bytes_read=0;
	  req->write_to_module_pending=0;	       
	  req->status=SEND_BODY;
     }
     
     if(req->status==SEND_BODY){
	  do{
	       if((ret=parse_chunk_data(req,&buf))==CI_ERROR){
		    ci_debug_printf(1,"Error parsing chunks, current chunk len: %d readed:%d, str:%s\n",
				    req->current_chunk_len,
				    req->chunk_bytes_read,
				    req->pstrblock_read);
		    return CI_ERROR;
	       }

	       while(req->write_to_module_pending>0){
		    bytes=(*dest_write)(data_dest,buf,req->write_to_module_pending);
		    if(bytes<0){
			 ci_debug_printf(1,"Error writing to output file!\n");
			 return CI_ERROR;
		    }
		    req->write_to_module_pending-=bytes;
	       }

	       if(ret==CI_EOF){
		    req->status=GET_EOF;
		    return CI_OK;
	       }
	  }while(ret!=CI_NEEDS_MORE);
	  
	  return CI_NEEDS_MORE;
     }
     
     return CI_OK;
}

const char *eof_str="0\r\n\r\n";

int send_get_data( request_t *req,
		   void *data_source,int (*source_read)(void *,char *,int),
		   void *data_dest,  int (*dest_write) (void *,char *,int)
){
     int io_ret,read_status,bytes,io_action;
     if(!req->eof_received){
	  io_action=wait_for_readwrite;
     }
     else
	  io_action=wait_for_read;
     
     while(io_action && (io_ret=ci_wait_for_data(req->connection->fd,TIMEOUT,io_action))){
	  if(io_ret&wait_for_write){
	       if(req->remain_send_block_bytes==0){
		    if(prepere_body_chunk(req,data_source,source_read)<=0){
			 req->eof_received=1;
			 req->pstrblock_responce=(char *)eof_str;
			 req->remain_send_block_bytes=5;
		    }
	       }
	       bytes=ci_write_nonblock(req->connection->fd,
				     req->pstrblock_responce,req->remain_send_block_bytes);
	       if(bytes<0)
		    return CI_ERROR;
	       req->pstrblock_responce+=bytes;
	       req->remain_send_block_bytes-=bytes;
	  }
	  
	  if(req->eof_received && req->remain_send_block_bytes==0)
	       io_action=0;
	  else
	       io_action=wait_for_write;


	  if(io_ret&wait_for_read){
	       if(net_data_read(req)==CI_ERROR)
		    return CI_ERROR;
	       
	       if((read_status=parse_incoming_data(req,data_dest,dest_write))==CI_ERROR)
		    return CI_ERROR;	       
	  }
	  
	  if(req->status!=GET_EOF)
	       io_action|=wait_for_read;

     }
     return CI_OK;
}



int do_send_file_request(request_t *req,
			 void *data_source,int (*source_read)(void *,char *,int),
			 void *data_dest,  int (*dest_write) (void *,char *,int)){
    int ret,v1,v2,remains,pre_eof=0,preview_status;
    char *buf,*val;
    
    if(CI_OK!=create_request(req,req->req_server,req->service,ICAP_RESPMOD)){
	ci_debug_printf(1,"Error making respmod request ....\n");
	return CI_ERROR;
    }

    if(req->preview>0){ /*The preview data will be send with headers....*/
	ci_buf_mem_alloc(&(req->preview_data),req->preview); /*Alloc mem for preview data*/
	buf=req->preview_data.buf;
	remains=req->preview;
	while(remains && ! pre_eof){  /*Getting the preview data*/
	    if((ret=(*source_read)(data_source,buf,remains))<=0){
		pre_eof=1;
		break;
	    }
	    remains-=ret;
	    
	}
	req->preview-=remains;
	req->preview_data.used=req->preview;
    }
    if(pre_eof)
	req->eof_received=1;

    /*Adding the */
    ci_request_create_respmod(req,1);
    ci_respmod_add_header(req,"Filetype: Unknown");
    ci_respmod_add_header(req,"User: chtsanti");
    
    ci_send_request_headers(req,pre_eof);
    /*send body*/
    
    reset_header(req->head);
    preview_status=100;

    if(req->preview>0){/*we must wait for ICAP responce here.....*/

	 do{
	     ci_wait_for_incomming_data(req->connection->fd,TIMEOUT);
	     if(net_data_read(req)==CI_ERROR)
		  return CI_ERROR;
	 }while(parse_icap_header(req,req->head)==CI_NEEDS_MORE);
	 
	 sscanf(req->head->buf,"ICAP/%d.%d %d",&v1,&v2,&preview_status);
	 ci_debug_printf(3,"Responce was with status:%d \n",preview_status);
	 if(req->eof_received && preview_status==200){
	      req->status=GET_HEADERS;
	      ci_headers_unpack(req->head);
	      if((val=search_header(req->head,"Encapsulated"))==NULL){
		   ci_debug_printf(1,"No encapsulated entities!\n");
		   return CI_ERROR;
	      }
	      process_encapsulated(req,val);	      
	 }
	 else
	      reset_header(req->head);
    }  
    
    if(preview_status==204)
	 return 204;

    if(send_get_data(req,data_source,source_read,data_dest,dest_write)==CI_ERROR)
	 return CI_ERROR;
    
    return CI_OK;    
}



void print_headers(request_t *req){
     int i;
     int type;
     ci_header_list_t *headers;
     ci_debug_printf(1,"\nICAP HEADERS:\n");
     for(i=0;i<req->head->used;i++){
	  ci_debug_printf(1,"\t%s\n",req->head->headers[i]);
     }
     ci_debug_printf(1,"\n");

     if((headers=ci_respmod_headers(req))==NULL){
	  headers=ci_reqmod_headers(req);
	  type=ICAP_REQMOD;
     }else
	  type=ICAP_RESPMOD;
     
     if(headers){
	  ci_debug_printf(1,"%s HEADERS:\n",ci_method_string(type));
	  for(i=0;i<req->head->used;i++){
	       if(headers->headers[i])
		    ci_debug_printf(1,"\t%s\n",headers->headers[i]);
	  }
	  ci_debug_printf(1,"\n");     
     }
}


int fileread(void *fd,char *buf,int len){
    int ret;
    ret=read(*(int *)fd,buf,len);
    return ret;
}

int filewrite(void *fd,char *buf,int len){
    int ret;
    ret=write(*(int *)fd,buf,len);
    return ret;
}

char *icap_server="localhost";
char *service="echo";
char *input_file=NULL;
char *output_file=NULL;
int RESPMOD=1;
int verbose=0;

static struct options_entry options[]={
     {"-i","icap_servername",&icap_server,ci_cfg_set_str,"The icap server name"},
     {"-f","filename",&input_file,ci_cfg_set_str,"Send this file to the icap server.\nDefault is to send an options request"},
     {"-o","filename",&output_file,ci_cfg_set_str,"Save output to this file.\nDefault is to send to the stdout"},
     {"-req",NULL,&RESPMOD,ci_cfg_disable,"Send a request modification instead of responce modification"},
     {"-d","level", &CI_DEBUG_LEVEL,ci_cfg_set_int,"debug level info to stdout"},
     {"-v",NULL,&verbose,ci_cfg_enable,"Print responce headers"},
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

int main(int argc, char **argv){
     int fd_in,fd_out;
     int ret,port=1344;
     char ip[CI_IPLEN];
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

     req=ci_client_request(conn,icap_server,service);

     get_options(req);
     ci_debug_printf(10,"OK done with options!\n");
     ci_conn_remote_ip(conn,ip);
     ci_debug_printf(1,"\nICAP server:%s, ip:%s, port:%d\n\n",icap_server,ip,port);
     if(!input_file){
	  ci_debug_printf(1,"OPTIONS:\n");
	  ci_debug_printf(1,"\tAllow 204: %s\n\tPreview: %d\n\tKeep alive: %s\n",
			  (req->allow204?"Yes":"No"),
			  req->preview,
			  (req->keepalive?"Yes":"No")
	       );
	  print_headers(req);
     }
     else{
	  if((fd_in=open(input_file,O_RDONLY))<0){
	       ci_debug_printf(1,"Error openning file %s\n",input_file);
	       exit(-1);
	  }
	  
	  if(output_file){
	       if((fd_out=open(output_file,O_CREAT|O_RDWR|O_EXCL,S_IRWXU|S_IRGRP))<0){
		    ci_debug_printf(1,"Error opening output file %s\n",output_file);
		    exit(-1);
	       }
	  }
	  else{
	       fd_out=fileno(stdout);
	  }
	  
	  
	  
	  ci_client_request_reuse(req);
	  
	  ci_debug_printf(10,"Preview:%d keepalive:%d,allow204:%d\n",
			  req->preview,req->keepalive,req->allow204);
	  
	  ci_debug_printf(10,"OK allocating request going to send request\n");
	  ret=do_send_file_request(req,
				   &fd_in,(int (*)(void *,char *,int))fileread,
				   &fd_out,(int (*)(void *,char *,int))filewrite
	       );
	  close(fd_in);
	  close(fd_out);

	  if(ret==204){
	       ci_debug_printf(1,"No modification needed (Allow 204 responce)\n");
	       if(output_file)
		    unlink(output_file);
	  }
	  else if(verbose)
	       print_headers(req);

	  
	  ci_debug_printf(2, "Done\n");
     }
     close(conn->fd);      
     return 0;
}
