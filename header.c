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
#include <fcntl.h>
#include "debug.h"
#include "header.h"



const char *CI_CommonHeaders[]={
  "Cache-Control",
  "Connection",
  "Date",
  "Expires",
  "Pragma",
  "Trailer",
  "Upgrade",
  /*And ICAP speciffic headers .....*/
  "Encapsulated"
};  



const char *CI_Methods[]={
     "",        /*0x00*/
     "OPTIONS", /*0x01*/
     "REQMOD",  /*0x02*/
     "",        /*0x03*/
     "RESPMOD"  /*0x04*/
};


const char *CI_RequestHeaders[]={
  "Authorization",
  "Allow",
  "From",
  "Host", /*REQUIRED ......*/
  "Referer",
  "User-Agent",
  /*And ICAP specific headers .....*/
  "Preview"
};

const char *CI_ResponceHeaders[]={
  "Server",
  /*ICAP spacific headers*/
  "ISTag"
};

const char *CI_OptionsHeaders[]={
  "Methods",
  "Service",
  "ISTag",
  "Encapsulated",
  "Opt-body-type",
  "Max-Connections",
  "Options-TTL",
  "Date",
  "Service-ID",
  "Allow",
  "Preview",
  "Transfer-Preview",
  "Transfer-Ignore",
  "Transfer-Complete"
};


const struct ci_error_code CI_ErrorCodes[]={
  {100,"Continue"}, /*Continue after ICAP Preview*/
  {204,"Unmodified"}, /*No modifications needed */
  {400,"Bad request"}, /*Bad request*/
  {401,"Unauthorized"},
  {403,"Forbidden"},
  {404,"Service not found"}, /*ICAP Service not found*/
  {405,"Not allowed"}, /*Method not allowed for service (e.g., RESPMOD requested for
	  service that supports only REQMOD). */
  {408,"Request timeout"}, /*Request timeout.  ICAP server gave up waiting for a request
	  from an ICAP client*/
  {500,"Server error"}, /*Server error.  Error on the ICAP server, such as "out of disk
	     space"*/
  {501,"Not implemented"}, /*Method not implemented.  This response is illegal for an
	  OPTIONS request since implementation of OPTIONS is mandatory.*/
  {502,"Bad Gateway"}, /*Bad Gateway.  This is an ICAP proxy and proxying produced an
	   error.*/
  {503,"Service overloaded"}, /*Service overloaded.  The ICAP server has exceeded a maximum
	   connection limit associated with this service; the ICAP client
	   should not exceed this limit in the future.*/
  {505, "Unsupported version"}  /*ICAP version not supported by server.*/
};

/*
#ifdef __CYGWIN__
int ci_error_code(int ec){
     return (ec>=EC_100&&ec<=EC_500?CI_ErrorCodes[ec].code:1000);
}

const char *unknownerrorcode="UNKNOWN ERROR CODE";

const char *ci_error_code_string(int ec){
     return (ec>=EC_100&&ec<=EC_505?CI_ErrorCodes[ec].str:unknownerrorcode);
}
#endif
*/


const char *CI_EncapsEntities[]={
  "req-hdr",
  "res-hdr",
  "req-body",
  "res-body",
  "null-body",
  "opt-body"
};

#ifdef __CYGWIN__

const char *unknownentity="UNKNOWN";
const char *unknownmethod="UNKNOWN";

const char *ci_method_string(int method){
  return (method<=ICAP_RESPMOD && method>=ICAP_OPTIONS ?CI_Methods[method]:unknownmethod);
}


const char *ci_encaps_entity_string(int e){
     return (e<=ICAP_OPT_BODY&&e>=ICAP_REQ_HDR?CI_EncapsEntities[e]:unknownentity);
}
#endif

ci_header_list_t  *mk_header(){
  ci_header_list_t *h;
  h=malloc(sizeof(ci_header_list_t));
  if(!h)
       return NULL;

  if(!(h->headers=malloc(HEADERSTARTSIZE*sizeof(char *))) ||!(h->buf=malloc(HEADSBUFSIZE*sizeof(char))) ){
       ci_debug_printf(1,"Server Error: Error allocation memory \n");
       if(h->headers) free(h->headers);
       if(h->buf) free(h->buf);
       if(h) free(h);
       return NULL;    
  }
  
  h->size=HEADERSTARTSIZE;
  h->used=0;
  h->bufsize=HEADSBUFSIZE;
  h->bufused=0;
  
  return h;
}

void destroy_header(ci_header_list_t *h){
  free(h->headers);
  free(h->buf);
  free(h);
}



int set_size_header(ci_header_list_t *h, int size){
     char * newbuf;
     if(size<h->bufsize)
	  return 1;
     
     newbuf=realloc(h->buf,size*sizeof(char));
     if(!newbuf){
	  ci_debug_printf(1,"Server Error:Error allocation memory \n");
	  return 0;
     }
     h->buf=newbuf;
     h->bufsize=size;
     return 1;
}

void reset_header(ci_header_list_t *h){
     h->used=0;
     h->bufused=0;
}

char *add_header(ci_header_list_t *h, char *line){
  char *newhead, **newspace, *newbuf;
  int len,linelen;
  int i=0;
 
  if(h->used==h->size){
       len=h->size+HEADERSTARTSIZE;
       newspace=realloc(h->headers,len*sizeof(char *));
       if(!newspace){
	    ci_debug_printf(1,"Server Error:Error allocation memory \n");
	    return NULL;
       }
       h->headers=newspace;
  }
  linelen=strlen(line);
  while(h->bufused+linelen+4 >= h->bufsize){
       len=h->bufsize+HEADSBUFSIZE;
       newbuf=realloc(h->buf,len*sizeof(char));
       if(!newbuf){
	    ci_debug_printf(1,"Server Error:Error allocation memory \n");
	    return NULL;
       }
       h->buf=newbuf;
       h->bufsize=len;
       h->headers[0]=h->buf;
       for(i=1;i<h->used;i++)
	    h->headers[i]=h->headers[i-1]+strlen(h->headers[i-1])+2;
  }
  newhead=h->buf+h->bufused;
  strcpy(newhead,line);
  h->bufused+=linelen+2; //2 char size for \r\n at the end of each header 
  *(newhead+linelen+1)='\n';*(newhead+linelen+3)='\n'; 
  if(newhead)
       h->headers[h->used++]=newhead;

  return newhead;
}


char * search_header(ci_header_list_t *h, char *header){
     int i;
     for(i=0;i<h->used;i++){
	  if(strncasecmp(h->headers[i],header,strlen(header))==0)
	       return h->headers[i];
     }
     return NULL;
}

char *get_header_value(ci_header_list_t *h, char *header){
     char *ptr,*phead;
     if(!(phead=search_header(h, header)))
	  return NULL;
     while(*phead!='\0' && *phead!=':') phead++;
     if(*phead!=':')
	  return NULL;
     phead++;
     while(isspace(*phead) && *phead != '\0')phead++;
     return phead;
}

int remove_header(ci_header_list_t *h, char *header){
     char *phead,*pnexthead;
     int i,j,header_len,rest_len;
     for(i=0;i<h->used;i++){
	  if(strncasecmp(h->headers[i],header,strlen(header))==0){
	       /*remove it........*/
	       phead=h->headers[i];
	       if(i==h->used-1){
		    phead=h->headers[i];
		    *phead='\r';
		    *(++phead)='\n';
		    h->bufused=(phead-h->buf);
		    (h->used)--;
		    return 1;
	       }
	       else{
		    header_len=h->headers[i+1]-h->headers[i];
		    rest_len=h->bufused-(h->headers[i]-h->buf)-header_len;
		    ci_debug_printf(1,"remove_header : remain len %d\n",rest_len);
		    memmove(phead,h->headers[i+1],rest_len);
		    /*reconstruct index.....*/
		    h->bufused-=header_len;
		    (h->used)--;
		    for(j=i+1;j<h->used;j++){
			 header_len=strlen(h->headers[j-1]);
			 h->headers[j]=h->headers[j-1]+header_len+1;
			 if(h->headers[j][0]=='\n') (h->headers[j])++;
		    }

		    return 1;
	       }
	  }
     }
     return 0;
}

char *replace_header(ci_header_list_t *h, char *header, char *newval){
}


void preparetosend(ci_header_list_t *h){/*Put the \r\n sequence at the end of each header before sending...... */
     int i=0,len=0;
     for(i=0;i<h->used;i++){
	  len=strlen(h->headers[i]);
	  if(h->headers[i][len+1]=='\n'){
	       h->headers[i][len]='\r';
/*	       h->headers[i][len+1]='\n';*/
	  }
	  else{ /*   handle the case that headers seperated with a '\n' only */
	       h->headers[i][len]='\n';
	  }
     }

     if(h->buf[h->bufused+1]=='\n'){
	  h->buf[h->bufused]='\r';
/*	  h->buf[h->bufused+1]='\n';*/
	  h->bufused+=2;
     }
     else{ /*   handle the case that headers seperated with a '\n' only */
	  h->buf[h->bufused]='\n';
	  h->bufused++;
     }
}

/********************************************************************************************/
/*             Entities List                                                                */


ci_encaps_entity_t *mk_encaps_entity(int type,int val){
  ci_encaps_entity_t *h;
  h=malloc(sizeof(ci_encaps_entity_t));
  if(!h) 
    return NULL;

  h->start=val;
  h->type=type;
  if(type==ICAP_REQ_HDR || type==ICAP_RES_HDR)
    h->entity=mk_header();
  else
    h->entity=NULL;
  return h;
}

void destroy_encaps_entity(ci_encaps_entity_t *e){
  if(e->type==ICAP_REQ_HDR || e->type==ICAP_RES_HDR){
    destroy_header((ci_header_list_t *)e->entity);
  }
  else
    free(e->entity);
  free(e);
}

int get_encaps_type(char *buf,int *val,char **endpoint){

  if(0==strncmp(buf,"req-hdr",7)){
    *val=strtol(buf+8,endpoint,10);
    return ICAP_REQ_HDR;
  }
  if(0==strncmp(buf,"res-hdr",7)){
    *val=strtol(buf+8,endpoint,10);
    return ICAP_RES_HDR;
  }
  if(0==strncmp(buf,"req-body",8)){
    *val=strtol(buf+9,endpoint,10);
    return ICAP_REQ_BODY;
  }
  if(0==strncmp(buf,"res-body",8)){
    *val=strtol(buf+9,endpoint,10);
    return ICAP_RES_BODY;
  }
  if(0==strncmp(buf,"null-body",9)){
    *val=strtol(buf+10,endpoint,10);
    return ICAP_NULL_BODY;
  }
  return -1;
}




int  get_method(char *buf){
  if(!strncmp(buf,"OPTIONS",7)){
    return ICAP_OPTIONS;
  }
  else if(!strncmp(buf,"REQMOD",6)){
    return ICAP_REQMOD;
  }
  else if(!strncmp(buf,"RESPMOD",7)){
    return ICAP_RESPMOD;
  }
  else{
    return -1;
  }
}


int sizeofheader(ci_header_list_t *h){
  int size=0,i;
  for(i=0;i<h->used;i++){
    size+=strlen(h->headers[i])+2;
  }
  size+=2; /*The sequence \r\n at the end of header*/
  return size;
}

int sizeofencaps(ci_encaps_entity_t *e){
  if(e->type==ICAP_REQ_HDR || e->type==ICAP_RES_HDR){
    return sizeofheader((ci_header_list_t *)e->entity);
  }
  return 0;
}


