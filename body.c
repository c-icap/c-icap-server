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
#include <stdio.h>
#include "body.h"
#include "debug.h"
#include "simple_api.h"
#include <assert.h>

#define STARTLEN 8192 /*8*1024*1024*/
#define INCSTEP  4096

struct mem_body *newmembody(){
     struct mem_body *b;
     b=malloc(sizeof(struct mem_body));
     if(!b)
	  return NULL;

     b->len=0;
     b->endpos=0;
     b->readpos=0;
     b->hasalldata=0;
     b->buf=malloc(STARTLEN*sizeof(char));
     if(b->buf==NULL){
	  free(b);
	  return NULL;
     }
     b->bufsize=STARTLEN;
     return b;
}

void freemembody(struct mem_body *b){
     if(!b)
	  return;
     if(b->buf)
	  free(b->buf);
     free(b);
}


int writememdata(struct mem_body *b, char *data,int len, int iseof){
     int remains,newsize;
     char *newbuf;
     if(iseof){
	  b->hasalldata=1;
	  debug_printf(10,"Buffer size=%d, Data size=%d\n ",
		       ((struct mem_body *)b)->bufsize,((struct mem_body *)b)->endpos);
     }

     remains=b->bufsize-b->endpos;
     while(remains< len){
	  newsize=b->bufsize+INCSTEP;
	  newbuf=realloc(b->buf,newsize);
	  if(newbuf==NULL){
	       if(remains)
		    memcpy(b->buf+b->endpos,data,remains);
	       b->endpos=b->bufsize;
	       return remains;
	  }
	  b->buf=newbuf;
	  b->bufsize=newsize;
	  remains=b->bufsize-b->endpos;
     }/*while remains<len */
     if(len){
	  memcpy(b->buf+b->endpos,data,len);
	  b->endpos+=len;
     }
     return len;
}

int readmemdata(struct mem_body *b,char *data,int len){
     int remains,copybytes;
     remains=b->endpos-b->readpos;
     if(remains==0 && b->hasalldata)
	  return CI_EOF;
     copybytes=(len<=remains?len:remains);
     if(copybytes){
	  memcpy(data,b->buf+b->readpos,copybytes);
	  b->readpos+=copybytes;
     }

     return copybytes;
}

void markendofdata(struct mem_body *b){
     b->hasalldata=1;
}



/******************************************************************************************************/
/*                                                                                                    */
/*                                                                                                    */

#define tmp_template "CI_TMP_XXXXXX"

extern int  BODY_MAX_MEM;
extern char *TMPDIR;

int open_tmp_file(char *filename){

     strncpy(filename,TMPDIR,FILENAME_LEN-sizeof(tmp_template)-1);
     strcat(filename,tmp_template);
     return mkstemp(filename);
}

int  resize_buffer(ci_cached_file_t *body,int new_size){
     char *newbuf;

     if(new_size<body->bufsize)
	  return 1;
     if(new_size>BODY_MAX_MEM)
	  return 0;
     
     newbuf=realloc(body->buf,new_size);
     if(newbuf){
	  body->buf=newbuf;
	  body->bufsize=new_size;
     }
     return 1;
}

ci_cached_file_t * ci_new_cached_file(int size){
     ci_cached_file_t *body;
     if(!(body=malloc(sizeof(ci_cached_file_t))))
	  return NULL;
     
/*     if(size==0)
	  size=BODY_MAX_MEM;
*/
     if(size>0 && size <=BODY_MAX_MEM ){
	  body->buf=malloc(size*sizeof(char));
     }
     else
	  body->buf=NULL;

     if(body->buf==NULL){
	  body->bufsize=0;
	  if((body->fd=open_tmp_file(body->filename))<0){
	       free(body);
	       return NULL;
	  }
     }
     else{
	  body->bufsize=size;
	  body->fd=-1;
     }
/*     body->growtosize=0;*/
     body->endpos=0;
     body->readpos=0;
     body->eof_received=0;
     body->unlocked=-1;/*Not use look*/
     return body;
}

void ci_reset_cached_file(ci_cached_file_t *body,int new_size){

     if(body->fd>0){
	  close(body->fd);
	  unlink(body->filename); /*Comment out for debuging reasons*/
     }
/*     body->growtosize=0;*/
     body->endpos=0;
     body->readpos=0;
     body->eof_received=0;
     body->unlocked=-1;
     body->fd=-1;

     if(!resize_buffer(body,new_size)){
	  /*free memory and open a file.*/
     }
}


void ci_release_cached_file(ci_cached_file_t *body){
     if(!body)
	  return;
     if(body->buf)
	  free(body->buf);
     
     if(body->fd>=0){
	  close(body->fd);
	  unlink(body->filename); /*Comment out for debuging reasons*/
     }
     
     free(body);
}


int ci_write_cached_file(ci_cached_file_t *body, char *buf,int len, int iseof){
     int remains,newsize;
     char *newbuf;

     if(iseof){
	  body->eof_received=1;
	  debug_printf(10,"Buffer size=%d, Data size=%d\n ",
		       ((ci_cached_file_t *)body)->bufsize,((ci_cached_file_t *)body)->endpos);
     }

     if(body->fd>0){ /*A file was open so write the data at the end of file.......*/
	  lseek(body->fd,0,SEEK_END);
	  write(body->fd,buf,len);
	  body->endpos+=len;
	  return len;
     }
     
     remains=body->bufsize-body->endpos;
     assert(remains>=0);
     if(remains< len){
	  
	  if((body->fd=open_tmp_file(body->filename))<0){
	       debug_printf(1,"I can not create the temporary file name:%s!!!!!!\n",body->filename);
	       return -1;
	  }
	  write(body->fd,body->buf,body->endpos);
	  write(body->fd,buf,len);
	  body->endpos+=len;
	  return len;
     }/*  if remains<len */
     
     if(len>0){
	  memcpy(body->buf+body->endpos,buf,len);
	  body->endpos+=len;
     }
     return len;

}

/*
body->unlocked=?
*/

int ci_read_cached_file(ci_cached_file_t *body,char *buf,int len){
     int remains,bytes;

     if( (body->readpos==body->endpos) && body->eof_received)
	  return CI_EOF;
     
     if(body->fd>0){
	  if(body->unlocked>=0)
	       remains=body->unlocked-body->readpos;
	  else
	       remains=len;

/*	  assert(remains>=0);*/

	  bytes=(remains>len?len:remains); /*Number of bytes that we are going to read from file.....*/

	  lseek(body->fd,body->readpos,SEEK_SET);
	  if((bytes=read(body->fd,buf,bytes))>0)
	       body->readpos+=bytes;
	  return bytes;
     }
     
     if(body->unlocked>=0)
	  remains=body->unlocked-body->readpos;
     else
	  remains=body->endpos-body->readpos;

/*     assert(remains>=0);     */

     bytes=(len<=remains?len:remains);
     if(bytes>0){
	  memcpy(buf,body->buf+body->readpos,bytes);
	  body->readpos+=bytes;
     }
     else /*?????????????????????????????? */
	  bytes=0;

     return bytes;
}



int ci_memtofile_cached_file(ci_cached_file_t *body){
     int size;
     if(body->fd>0)
	  return 0;

     if((body->fd=open_tmp_file(body->filename))<0){
	  return -1;
     }
     size=write(body->fd,body->buf,body->endpos);
     if(size!=body->endpos){
	  /*Do something .........\n*/
     }
     return size;
}
