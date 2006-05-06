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


extern int TIMEOUT; /*can not be here becouse request_common will be into library.....*/

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
     for(i=0;req->entities[i]!=NULL;i++) 
	  destroy_encaps_entity(req->entities[i]);

     for(i=0;i<7;i++){
	  if(req->trash_entities[i])
	       destroy_encaps_entity(req->trash_entities[i]);
     }
  
     free(req);
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
			       bytes))<0){ /*... read some data...*/
	ci_debug_printf(5,"Error reading data (read return=%d) \n",bytes);
	return CI_ERROR; 
    }
    req->pstrblock_read_len+=bytes;/* ... (size of data is readed plus old )...*/
    return CI_OK;
}
