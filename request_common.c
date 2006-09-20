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
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include "debug.h"
#include "request.h"
#include "simple_api.h"

/* struct buf functions*/
void ci_buf_init(struct ci_buf *buf){
     buf->buf=NULL;
     buf->size=0;
     buf->used=0;
}

void ci_buf_reset(struct ci_buf *buf){
     buf->used=0;
}

int ci_buf_mem_alloc(struct ci_buf *buf,int size){
     if(!(buf->buf=malloc(size*sizeof(char))))
	  return 0;
     buf->size=size;
     buf->used=0;
     return size;
}

void ci_buf_mem_free(struct ci_buf *buf){
     free(buf->buf);
     buf->buf=NULL;
     buf->size=0;
     buf->used=0;
}


int ci_buf_write(struct ci_buf *buf,char *data,int len){
     if(len > (buf->size-buf->used))
	  return -1;
     memcpy(buf->buf+buf->used,data,len);
     buf->used+=len;
     return len;
}

int ci_buf_reset_size(struct ci_buf *buf,int req_size){
     if(buf->size > req_size)
	  return req_size;
     if(buf->buf)
	  free(buf->buf);
     return ci_buf_mem_alloc(buf,req_size);
}



void ci_request_pack(request_t *req){
    ci_encaps_entity_t **elist,*e;
    char buf[256]; 

    if(req->is_client_request && req->preview>0){
	 sprintf(buf,"Preview: %d",req->preview);
	 ci_headers_add(req->head,buf);
    }
    
    elist=req->entities;
    
    if(elist[0]!=NULL)
	elist[0]->start=0;
     
    if(elist[1]!=NULL){
	elist[1]->start=sizeofencaps(elist[0]);
     }

     if(elist[2]!=NULL){
	 elist[2]->start=sizeofencaps(elist[1])+elist[1]->start;
     }

     
     if(elist[0]==NULL){
	 sprintf(buf,"Encapsulated: null-body=0");
     }
     else if(elist[2]!=NULL){
	 sprintf(buf,"Encapsulated: %s=%d, %s=%d, %s=%d",
		 ci_encaps_entity_string(elist[0]->type),elist[0]->start,
		 ci_encaps_entity_string(elist[1]->type),elist[1]->start,
		 ci_encaps_entity_string(elist[2]->type),elist[2]->start);
     }
     else if(elist[1]!=NULL){
	 sprintf(buf,"Encapsulated: %s=%d, %s=%d",
		 ci_encaps_entity_string(elist[0]->type),elist[0]->start,
		 ci_encaps_entity_string(elist[1]->type),elist[1]->start);
     }
     else{ /*Only req->entities[0] exists*/
	 sprintf(buf,"Encapsulated: %s=%d",
		  ci_encaps_entity_string(elist[0]->type),elist[0]->start);
     }
     ci_headers_add(req->head,buf);
     
     while((e=*elist++)!=NULL){
	 if(e->type==ICAP_REQ_HDR || e->type==ICAP_RES_HDR)
	     ci_headers_pack(( ci_headers_list_t *)e->entity);
     }
     /*e_list is not usable now !!!!!!! */
     ci_headers_pack(req->head);
}


/*
Valid forms of encapsulated entities

   REQMOD  request  encapsulated_list: [reqhdr] reqbody
   REQMOD  response encapsulated_list: {[reqhdr] reqbody} |
                                       {[reshdr] resbody}
   RESPMOD request  encapsulated_list: [reqhdr] [reshdr] resbody
   RESPMOD response encapsulated_list: [reshdr] resbody
   OPTIONS request  encapsulated_list: [optbody]
   OPTIONS response encapsulated_list: optbody

TODO: 
   The following function must chech request and if the encapsulated entity is valid
   must put it to the right position in req->entities .........
*/

//alloc_an_entity
ci_encaps_entity_t *ci_request_alloc_entity(request_t *req,int type,int val){
     ci_encaps_entity_t *e=NULL;
     
     if(type> ICAP_OPT_BODY || type<0){ //
	  return NULL;
     }
     
     if(req->trash_entities[type]){
	  e=req->trash_entities[type];	  
	  req->trash_entities[type]=NULL;
	  e->type=type;
	  e->start=val;
	  ci_debug_printf(8,"Get entity from trash....\n");
	  return e;
     }
     
     //Else there is no available entity to trash_entities so make a new....
     ci_debug_printf(8,"Allocate a new entity of type %d\n",type);
     return mk_encaps_entity(type,val);
}


int ci_request_release_entity(request_t *req,int pos){
     int type=0;
     if(!req->entities[pos])
	  return 0;
     
     type=req->entities[pos]->type;
     if(type> ICAP_OPT_BODY || type<0){ //?????????
	  destroy_encaps_entity(req->entities[pos]);
	  req->entities[pos]=NULL;
	  return 0;
     }
     
     if(req->trash_entities[type]!=NULL){
	  ci_debug_printf(3,"ERROR!!!!! There is an entity of type %d to trash..... ", type);
	  destroy_encaps_entity(req->trash_entities[type]);
     }
     req->trash_entities[type]=req->entities[pos];     
     if(req->trash_entities[type]->type==ICAP_REQ_HDR || req->trash_entities[type]->type==ICAP_RES_HDR){
	  if(req->trash_entities[type]->entity)
	       ci_headers_reset(req->trash_entities[type]->entity);
     }
     req->entities[pos]=NULL;
     return 1;
}


request_t *ci_request_alloc(ci_connection_t *connection){
     request_t *req;
     int i;
     req=(request_t *)malloc(sizeof(request_t));
     
     req->connection=connection;
     req->user[0]='\0';

     req->access_type=0;

     req->service=NULL;
     req->current_service_mod=NULL;
     req->service_data=NULL;
     req->args=NULL;
     req->type=-1;
     req->is_client_request=0;
     req->preview=0;
     ci_buf_init(&(req->preview_data));

     req->keepalive=1; /*Keep alive connection is the default behaviour for icap protocol.*/
     req->allow204=0;
     req->hasbody=0;
     req->responce_hasbody=0;
     req->eof_received=0;
     
     req->head=ci_headers_make();
     req->xheaders=ci_headers_make();
     req->status=SEND_NOTHING;


     req->pstrblock_read=NULL;
     req->pstrblock_read_len=0;
     req->current_chunk_len=0;
     req->chunk_bytes_read=0;
     req->write_to_module_pending=0;

     req->pstrblock_responce=NULL;
     req->remain_send_block_bytes=0;
     req->data_locked=1;
     
     for(i=0;i<5;i++) //
	  req->entities[i]=NULL;
     for(i=0;i<7;i++) //
	  req->trash_entities[i]=NULL;
     
     return req;

}

/*reset_request simply reset request to use it with tunneled requests
  The req->access_type must not be reset!!!!!
*/
void ci_request_reset(request_t *req){
     int i;
     if(req->service)
	  free(req->service);
     if(req->args)
	  free(req->args);
     /*     memset(req->connections,0,sizeof(ci_connection)) */ /*Not really needed...*/

     req->user[0]='\0';
     req->service=NULL;
     req->current_service_mod=NULL;
     req->service_data=NULL;
     req->args=NULL;
     req->type=-1;
     req->is_client_request=0;
     req->preview=0;
     ci_buf_reset(&(req->preview_data));

     req->keepalive=1; /*Keep alive connection is the default behaviour for icap protocol.*/
     req->allow204=0;
     req->hasbody=0;
     req->responce_hasbody=0;
     ci_headers_reset(req->head);
     ci_headers_reset(req->xheaders);
     req->eof_received=0;
     req->status=SEND_NOTHING;

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

void ci_request_destroy(request_t *req){
     int i;
     if(req->service)
	 free(req->service);
     if(req->args)
	 free(req->args);
     if(req->connection)
	 free(req->connection);
     
     ci_buf_mem_free(&(req->preview_data));
     ci_headers_destroy(req->head);
     ci_headers_destroy(req->xheaders);
     for(i=0;req->entities[i]!=NULL;i++) 
	  destroy_encaps_entity(req->entities[i]);

     for(i=0;i<7;i++){
	  if(req->trash_entities[i])
	       destroy_encaps_entity(req->trash_entities[i]);
     }
  
     free(req);
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


enum chunk_status{READ_CHUNK_DEF=1, READ_CHUNK_DATA};


/*
  maybe the wdata must moved to the request_t and write_to_module_pending must replace wdata_len
*/
int parse_chunk_data(request_t *req, char **wdata){
     char *end;
     int num_len,remains,tmp;
     int read_status=0;

     *wdata=NULL;
     if(req->write_to_module_pending){
	  /*We must not here if the chunk buffer did not flashed*/
	  return CI_ERROR;
     }
     
     while(1){
	  if(req->current_chunk_len==req->chunk_bytes_read)
	       read_status=READ_CHUNK_DEF;
	  else
	       read_status=READ_CHUNK_DATA;
	  
	  if(read_status==READ_CHUNK_DEF){	       
	       errno=0;
	       tmp=strtol(req->pstrblock_read,&end,16);
	       /*here must check for erron*/
	       if(tmp==0 && req->pstrblock_read==end ){ /*Oh .... an error ...*/
		    ci_debug_printf(5,"Parse error:count=%d,start=%c\n",
				    tmp,
				    req->pstrblock_read[0]);
		    return CI_ERROR; 
	       }	
	       num_len=end-req->pstrblock_read;	
	       if(req->pstrblock_read_len-num_len < 2){ 
		    return CI_NEEDS_MORE;
	       }
	       /*At this point the req->pstrblock_read must point to the start of a chunk eg to a "123\r\n"*/
	       req->chunk_bytes_read=0;
	       req->current_chunk_len=tmp;

	       if(req->current_chunk_len==0){
		    
		    if(*end==';'){ 
			 if(req->pstrblock_read_len < 11){ /*must hold a 0; ieof\r\n\r\n*/
			      return CI_NEEDS_MORE;
			 }
			 
			 if(strncmp(end,"; ieof",6)!=0)
			      return CI_ERROR;
			 
			 req->eof_received=1;
			 return CI_EOF;
		    }
		    else{
			 if(req->pstrblock_read_len-num_len < 4){
			      return CI_NEEDS_MORE;
			 }
			 if(strncmp(end,"\r\n\r\n",4)!=0)
			      return CI_ERROR;

			 req->pstrblock_read=NULL;
			 req->pstrblock_read_len=0;
			 return CI_EOF;
		    }
	       }
	       else{
		    if(*end!='\r' || *(end+1)!='\n'){
			 return CI_ERROR;
		    }
		    read_status=READ_CHUNK_DATA;
		    req->pstrblock_read=end+2;
		    req->pstrblock_read_len-=(num_len+2);
		    /*include the \r\n end of chunk data*/
		    req->current_chunk_len+=2;
	       }
	  }
	  /*if we have data for service leaving this function now*/
	  if(req->write_to_module_pending)
	       return CI_OK;
	  if(read_status==READ_CHUNK_DATA){
	       if(req->pstrblock_read_len<=0){
		    return CI_NEEDS_MORE;
	       }
	       *wdata=req->pstrblock_read;
	       remains=req->current_chunk_len-req->chunk_bytes_read;
	       if(remains<=req->pstrblock_read_len){/*we have all the chunks data*/
		    if(remains>2)
			 req->write_to_module_pending=remains-2;
		    else/*we are in all or part of the \r\n end of chunk data*/
			 req->write_to_module_pending=0;
		    req->chunk_bytes_read+=remains;
		    req->pstrblock_read+=remains;
		    req->pstrblock_read_len-=remains;
	       }else{
		    tmp=remains-req->pstrblock_read_len;
		    if(tmp<2)
			 req->write_to_module_pending=req->pstrblock_read_len-tmp;
		    else
			 req->write_to_module_pending=req->pstrblock_read_len;
		    req->chunk_bytes_read+=req->pstrblock_read_len;
		    req->pstrblock_read+=req->pstrblock_read_len;
		    req->pstrblock_read_len-=req->pstrblock_read_len;		   
	       }
	  }
	  if(req->pstrblock_read_len==0)
	       return CI_NEEDS_MORE;
     }

    return CI_OK;
}

int net_data_read(request_t *req){
    int bytes;
  
    if(req->pstrblock_read!=req->rbuf){
	/*... put the current data to the begining of buf ....*/
	 if(req->pstrblock_read_len)
	      memmove(req->rbuf,req->pstrblock_read,req->pstrblock_read_len);
	req->pstrblock_read=req->rbuf;
    }
    
    bytes=BUFSIZE-req->pstrblock_read_len;
    if(bytes<=0){
	ci_debug_printf(5,"Not enough space to read data! is this a bug (%d %d)?????\n",
			req->pstrblock_read_len, BUFSIZE);
	return CI_ERROR;
    }

    if((bytes=ci_read_nonblock(req->connection->fd,
			       req->rbuf+req->pstrblock_read_len,
			       bytes))<=0){ /*... read some data...*/
	ci_debug_printf(5,"Error reading data (read return=%d) \n",bytes);
	return CI_ERROR; 
    }
    req->pstrblock_read_len+=bytes;/* ... (size of data is readed plus old )...*/
    return CI_OK;
}




/*************************************************************************/
/* ICAP client functions                                                 */


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
     ci_headers_reset(req->head);
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



int client_create_request(request_t *req,char *servername,char *service,int reqtype){
    char buf[256];    

    if(reqtype!=ICAP_OPTIONS && reqtype!=ICAP_REQMOD && reqtype!=ICAP_RESPMOD)
	return CI_ERROR;

    req->type=reqtype;
    req->is_client_request=1;
    snprintf(buf,255,"%s icap://%s/%s ICAP/1.0",
	     ci_method_string(reqtype),servername,service);
    buf[255]='\0';
    ci_headers_add(req->head,buf);
    snprintf(buf,255,"Host: %s",servername);
    buf[255]='\0';
    ci_headers_add(req->head,buf);
    ci_headers_add(req->head,"User-Agent: C-ICAP-Client-Library/0.01");
    if(ci_allow204(req))
	 ci_headers_add(req->head,"Allow: 204");
    return CI_OK;
}

int get_request_options(request_t *req,ci_headers_list_t *h){
    char *pstr;

    if((pstr=ci_headers_value(h, "Preview"))!=NULL){
	req->preview=strtol(pstr,NULL,10);
	if(req->preview<0)
	    req->preview=0;
    }
    else
	req->preview=0;

    
    req->allow204=0;
    if((pstr=ci_headers_value(h, "Allow"))!=NULL){
	if(strtol(pstr,NULL,10) == 204)
	    req->allow204=1;
    }

    req->keepalive=1;
    if((pstr=ci_headers_value(h, "Connection"))!=NULL && strncmp(pstr,"close",5)==0){
	req->keepalive=0;
    }

    /*Moreover we are interested for the followings*/
    if((pstr=ci_headers_value(h, "Transfer-Preview"))!=NULL){
	/*Not implemented yet*/
    }

    if((pstr=ci_headers_value(h, "Transfer-Ignore"))!=NULL){
	/*Not implemented yet*/
    }
	
    if((pstr=ci_headers_value(h, "Transfer-Complete"))!=NULL){
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



int client_send_request_headers(request_t *req,int has_eof,int timeout){
    ci_encaps_entity_t **elist,*e;
    ci_headers_list_t *headers;
    int bytes;

    ci_request_pack(req);
    if(ci_writen(req->connection->fd,req->head->buf,req->head->bufused,timeout)<0)
	 return CI_ERROR;

    elist=req->entities;
    while((e=*elist++)!=NULL){
	if(e->type==ICAP_REQ_HDR || e->type==ICAP_RES_HDR){
	    headers=( ci_headers_list_t *)e->entity;
	    if(ci_writen(req->connection->fd,headers->buf,headers->bufused,timeout)<0)
		 return CI_ERROR;
	}
    }

    if(req->preview>0 && req->preview_data.used >0 ){
	bytes=sprintf(req->wbuf,"%x\r\n",req->preview);
	if(ci_writen(req->connection->fd,req->wbuf,bytes,timeout)<0)
	     return CI_ERROR;
	if(ci_writen(req->connection->fd,req->preview_data.buf,req->preview,timeout)<0)
	     return CI_ERROR;
	if(has_eof){
	     if(ci_writen(req->connection->fd,"\r\n0; ieof\r\n\r\n",13,timeout)<0)
		  return CI_ERROR;
	    req->eof_received=1;
	    
	}
	else{
	     if(ci_writen(req->connection->fd,"\r\n0\r\n\r\n",7,timeout)<0)
		  return CI_ERROR;
	}
    }

    return CI_OK;
}

/*this function check if there is enough space in buffer buf ....*/
static int check_realloc(char **buf,int *size,int used,int mustadded){
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


int client_parse_icap_header(request_t *req,ci_headers_list_t *h){
     int readed=0,eoh=0;
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

int client_parse_encaps_header(request_t *req,ci_headers_list_t *h,int size){
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


int ci_client_get_server_options(request_t *req,int timeout){
    
    if(CI_OK!=client_create_request(req,req->req_server,req->service,ICAP_OPTIONS))
	return CI_ERROR;
    client_send_request_headers(req,0,timeout);
    ci_headers_reset(req->head);

    do{
	ci_wait_for_incomming_data(req->connection->fd,timeout);
	if(net_data_read(req)==CI_ERROR)
	     return CI_ERROR;
    }while(client_parse_icap_header(req,req->head)==CI_NEEDS_MORE);

    ci_headers_unpack(req->head);
    get_request_options(req,req->head);

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


ci_connection_t *ci_client_connect_to(char *servername,int port,int proto){
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


int client_prepere_body_chunk(request_t *req,void *data, int (*readdata)(void *data,char *,int)){
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


int client_parse_incoming_data(request_t *req,void *data_dest,  int (*dest_write) (void *,char *,int)){
     int ret,v1,v2,status,bytes,size;
     char *buf,*val;
     ci_headers_list_t *resp_heads;

     if(req->status==GET_NOTHING){
	  /*And reading the new .....*/
	  ret=client_parse_icap_header(req,req->head);
	  if(ret!=CI_OK)
	       return ret;
	  sscanf(req->head->buf,"ICAP/%d.%d %d",&v1,&v2,&status);
	  ci_debug_printf(3,"Responce was with status:%d \n",status);	  
	  ci_headers_unpack(req->head);

	  if(ci_allow204(req) && status==204)
	       return 204;

	  if((val=ci_headers_search(req->head,"Encapsulated"))==NULL){
	       ci_debug_printf(1,"No encapsulated entities!\n");
	       return CI_ERROR;
	  }
	  process_encapsulated(req,val);

	  if(!req->entities[0])
	       return CI_ERROR;

	  if(!req->entities[1]){ /*Then we have only body*/
	       req->status=GET_BODY;
	       if(req->pstrblock_read_len==0)
		    return CI_NEEDS_MORE;
	  }
	  else{
	       req->status=GET_HEADERS;
	       size=req->entities[1]->start-req->entities[0]->start;
	       resp_heads=req->entities[0]->entity;
	       if(!ci_headers_setsize(resp_heads,size))
		    return CI_ERROR;
	  }

//	  return CI_NEEDS_MORE;
     }

     /*read encups headers */
     
     /*Non option responce has one or two entities: 
          "req-headers req-body|null-body" or [resp-headers] resp-body||null-body   
       So here client_parse_encaps_header will be called for one headers block
     */

     if(req->status==GET_HEADERS){
	  size=req->entities[1]->start-req->entities[0]->start;
	  resp_heads=req->entities[0]->entity;
	  if((ret=client_parse_encaps_header(req,resp_heads,size))!=CI_OK)
	       return ret;

     
	  ci_headers_unpack(resp_heads);
	  ci_debug_printf(5,"OK reading headers boing to read body\n");

	  /*reseting body chunks related variables*/	  
	  req->current_chunk_len=0;
	  req->chunk_bytes_read=0;
	  req->write_to_module_pending=0;	       
	  req->status=GET_BODY;
	  if(req->pstrblock_read_len==0)
	       return CI_NEEDS_MORE;
     }
     
     if(req->status==GET_BODY){
	  do{
	       if((ret=parse_chunk_data(req,&buf))==CI_ERROR){
		    ci_debug_printf(1,"Error parsing chunks, current chunk len: %d readed:%d, readlen:%d, str:%s\n",
				    req->current_chunk_len,
				    req->chunk_bytes_read,
				    req->pstrblock_read_len,
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

int client_send_get_data( request_t *req,
			  int timeout,
			  void *data_source,int (*source_read)(void *,char *,int),
			  void *data_dest,  int (*dest_write) (void *,char *,int)
){
     int io_ret,read_status,bytes,io_action;
     if(!req->eof_received){
	  io_action=wait_for_readwrite;
     }
     else
	  io_action=wait_for_read;
     
     while(io_action && (io_ret=ci_wait_for_data(req->connection->fd,timeout,io_action))){
	  if(io_ret&wait_for_write){
	       if(req->remain_send_block_bytes==0){
		    if(client_prepere_body_chunk(req,data_source,source_read)<=0){
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
	       
	       if((read_status=client_parse_incoming_data(req,data_dest,dest_write))==CI_ERROR)
		    return CI_ERROR;

	       if(read_status==204)
		    return 204;
	  }
	  
	  if(req->status!=GET_EOF)
	       io_action|=wait_for_read;

     }
     return CI_OK;
}



int ci_client_icapfilter(request_t *req,
			 int timeout,
			 ci_headers_list_t *headers,
			 void *data_source,int (*source_read)(void *,char *,int),
			 void *data_dest,  int (*dest_write) (void *,char *,int)){
    int i,ret,v1,v2,remains,pre_eof=0,preview_status;
    char *buf,*val;
    
    if(CI_OK!=client_create_request(req,req->req_server,req->service,ICAP_RESPMOD)){
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

    /*Add the user supplied headers*/
    if(headers){
	 ci_request_create_respmod(req,1,1);
	 for(i=0;i<headers->used;i++){
	      ci_respmod_add_header(req,headers->headers[i]);
	 }
    }
    else
	 ci_request_create_respmod(req,0,1);

    client_send_request_headers(req,pre_eof,timeout);
	 
    /*send body*/
    
    ci_headers_reset(req->head);
    for(i=0;req->entities[i]!=NULL;i++) {
	ci_request_release_entity(req,i);
    }
    preview_status=100;

    if(req->preview>0){/*we must wait for ICAP responce here.....*/

	 do{
	     ci_wait_for_incomming_data(req->connection->fd,timeout);
	     if(net_data_read(req)==CI_ERROR)
		  return CI_ERROR;
	 }while(client_parse_icap_header(req,req->head)==CI_NEEDS_MORE);
	 
	 sscanf(req->head->buf,"ICAP/%d.%d %d",&v1,&v2,&preview_status);
	 ci_debug_printf(3,"Preview responce was with status:%d \n",preview_status);
	 if(req->eof_received && preview_status==200){
	      ci_headers_unpack(req->head);
	      if((val=ci_headers_search(req->head,"Encapsulated"))==NULL){
		   ci_debug_printf(1,"No encapsulated entities!\n");
		   return CI_ERROR;
	      }
	      process_encapsulated(req,val);	      
	      if(!req->entities[1]) /*Then we have only body*/
		   req->status=GET_BODY;
	      else
		   req->status=GET_HEADERS;
	 }
	 else
	      ci_headers_reset(req->head);
    }  
    
    if(preview_status==204)
	 return 204;

    ret=client_send_get_data(req,timeout,data_source,source_read,data_dest,dest_write);
    return ret;
}
