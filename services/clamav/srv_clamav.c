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
#include "cfg_param.h"
#include <clamav.h>
#include <errno.h>

/******************************************************************************/
/* ClamAV definitions                                                         */

#define CL_TYPENO 500
typedef enum {
    CL_TYPE_UNKNOWN_TEXT = CL_TYPENO,
    CL_TYPE_UNKNOWN_DATA,
    CL_TYPE_MSEXE,
    CL_TYPE_DATA,
    CL_TYPE_TAR,
    CL_TYPE_GZ,
    CL_TYPE_ZIP,
    CL_TYPE_BZ,
    CL_TYPE_RAR,
    CL_TYPE_MSSZDD,
    CL_TYPE_MSOLE2,
    CL_TYPE_MSCAB,
    CL_TYPE_MSCHM,
    CL_TYPE_SCRENC,
    CL_TYPE_GRAPHICS,

    /* bigger numbers have higher priority (in o-t-f detection) */
    CL_TYPE_HTML, /* on the fly */
    CL_TYPE_MAIL  /* magic + on the fly */

} cli_file_t;

char *ft_description[]={
     "UNKNOWN_TEXT",
     "UNKNOWN_DATA",
     "MSEXE",
     "DATA",
     "TAR",
     "GZ",
     "ZIP",
     "BZ",
    "RAR",
    "MSSZDD",
    "MSOLE2",
    "MSCAB",
    "MSCHM",
    "SCRENC",
    "GRAPHICS",
    "HTML", 
    "MAIL"
};
#define MAXTYPES 17

typedef enum {
     NONE=0,
     SCAN_IN_MEM,
     SCAN_IN_FILE
} scan_type_t;


cli_file_t cli_filetype(const char *buf, size_t buflen);
scan_type_t scan_type(cli_file_t);


struct cl_node *root;
struct cl_limits limits;

typedef struct av_req_data{
     ci_cached_file_t *body;
     request_t *req;
     scan_type_t scantype ;
     const char *virus_name;
     int has_error_page;
}av_req_data_t;


void generate_error_page(av_req_data_t *data,request_t *req,ci_cached_file_t *body);

/***********************************************************************************/
/* Module definitions                                                              */

int SEND_PERCENT_BYTES=100; /* Can send all bytes that has received without checked */
int scantypes[MAXTYPES];


int srvclamav_init_service(service_module_t *this);
void srvclamav_close_service(service_module_t *this);
void srvclamav_end_of_headers_handler(void *data,request_t *req);
int srvclamav_check_preview_handler(void *data, request_t *);
int srvclamav_end_of_data_handler(void  *data,request_t *);
void *srvclamav_init_request_data(service_module_t *serv,request_t *req);
void srvclamav_release_request_data(void *data);
int srvclamav_write(void *data, char *buf,int len ,int iseof);
int srvclamav_read(void *data,char *buf,int len);

/*Configuration Functions*/
int SetScanFileTypes(char *directive,char **argv,void *setdata);
int SetPercentBytes(char *directive,char **argv,void *setdata);

/*Configuration Table .....*/
static struct conf_entry conf_variables[]={
     {"SendPercentData",NULL,SetPercentBytes,NULL},
     {"ScanFileTypes",NULL,SetScanFileTypes,NULL},
     {NULL,NULL,NULL,NULL}
};


char *srvclamav_options[]={
     "Preview: 1024",
     "Allow: 204",
     "Transfer-Preview: *",
     "Encapsulated: null-body=0",
     NULL
};


CI_DECLARE_MOD_DATA service_module_t service={
     "srv_clamav",  /*Module name*/
     "Clamav/Antivirus service", /*Module short description*/
     ICAP_RESPMOD|ICAP_REQMOD, /*Service type responce or request modification*/
     srvclamav_options, /*Extra options headers*/
     NULL,/* Options body*/
     srvclamav_init_service, /*init_service.*/
     srvclamav_close_service,/*close_service*/
     srvclamav_init_request_data,/*init_request_data. */
     srvclamav_release_request_data, /*release request data*/
     srvclamav_end_of_headers_handler,
     srvclamav_check_preview_handler,
     srvclamav_end_of_data_handler,
     srvclamav_write,
     srvclamav_read,
     conf_variables,
     NULL
};




int srvclamav_init_service(service_module_t *this){
     int ret,no=0,i;
     if((ret = cl_loaddbdir(cl_retdbdir(), &root, &no))) {
	  debuf_printf(10,"cl_loaddbdir: %s\n", cl_perror(ret));
	  return 0;
     }
     if((ret = cl_build(root))) {
	  debug_printf(10,"Database initialization error: %s\n", cl_strerror(ret));;
	  cl_free(root);
	  return 0;
     }

     for(i=0;i<MAXTYPES;i++)  scantypes[i]=0;

     
     memset(&limits, 0, sizeof(struct cl_limits));
     limits.maxfiles = 1000; /* max files */
     limits.maxfilesize = 100 * 1048576; /* maximal archived file size == 100 Mb */
     limits.maxreclevel = 5; /* maximal recursion level */
     limits.maxratio = 200; /* maximal compression ratio */
     limits.archivememlim = 0; /* disable memory limit for bzip2 scanner */
     return 1;
}


void srvclamav_close_service(service_module_t *this){
     cl_free(root);
}



void *srvclamav_init_request_data(service_module_t *serv,request_t *req){
     int content_size,preview_size,mem_buf_size;
     av_req_data_t *data;
     content_size=ci_req_content_lenght(req);
     preview_size=ci_req_preview_size(req);

     if(ci_req_hasbody(req)){
	  debug_printf(10,"Request type: %d. We expect to read :%d body data. Preview size:%d\n",
		 req->type,content_size,preview_size);
	  data=malloc(sizeof(av_req_data_t));
	  if(content_size==0)
	       mem_buf_size=preview_size+10; /*something more than preview_size*/
	  else
	       mem_buf_size=content_size+10;/*something more than content size*/

	  data->has_error_page=0;
	  data->body=ci_new_cached_file(mem_buf_size); /*make the body and use mem_buf_size of mem*/
	  data->virus_name=NULL;
	  data->scantype=SCAN_IN_MEM;
	  data->req=req;
	  return data;
     }
     return NULL;
}

void srvclamav_release_request_data(void *data){
     if(data)
	  ci_release_cached_file(((av_req_data_t *)data)->body);
}


void srvclamav_end_of_headers_handler(void *data,request_t *req){

     if(ci_req_type(req)==ICAP_RESPMOD){
	  ci_req_respmod_add_header(req,"Via: C-ICAP  0.01/ClamAV Antivirus");
     }
     else if(ci_req_type(req)==ICAP_REQMOD){
	  ci_req_reqmod_add_header(req,"Via: C-ICAP  0.01/ClamAV Antivirus");
     }
     ci_req_unlock_data(req); /*Icap server can send data before all body has received*/
} 


int srvclamav_check_preview_handler(void *data, request_t *req){
     ci_cached_file_t *body=(data?((av_req_data_t *)data)->body:NULL);
     char *content_type=NULL;
//     char smallbuf[128];
     cli_file_t file_type;

     if(ci_req_type(req)==ICAP_RESPMOD){
	  content_type=ci_req_respmod_get_header(req,"Content-Type");
	  debug_printf(10,"File type: %s\n",(content_type!=NULL?content_type:"Unknown") );
     }     
     if(!body) /*It is not possible, but who knows.........*/
	  return EC_100;


     if(!ci_ismem_cached_file(body)){
	  debug_printf(10,"Not a memory resident body (preview:%d, bufsize:%d).We are going to scan it anyway!\n",
		       ci_req_preview_size(req),body->bufsize);
	  ((av_req_data_t *)data)->scantype=SCAN_IN_FILE;
	  return EC_100; /*Continue sending data....... */
     }

     /*Going to determine the file type ....... */
     file_type=cli_filetype(body->buf,body->endpos);
     if(file_type==CL_TYPE_UNKNOWN_TEXT && content_type!=NULL){
	  if(strstr(content_type,"text/html") || strstr(content_type,"text/css"))
	       file_type=CL_TYPE_HTML;
     }
     debug_printf(10,"File type returned by ClamAV:%s\n",ft_description[file_type-CL_TYPENO]);

     /*Clamav can scan archived or zipped data only from a file.......*/
     /*Lets see if we can scan this data type in the memory or must by scanned from a file*/
     if((((av_req_data_t *)data)->scantype=scan_type(file_type))==NONE){
	  debug_printf(10,"Not in \"must scanned list\".Allow it...... \n");
	  return EC_204;
     }

     if(((av_req_data_t *)data)->scantype==SCAN_IN_FILE)
	  ci_memtofile_cached_file(body); /*Write the data to the disk......*/

     return EC_100;
}



int srvclamav_write(void *data, char *buf,int len ,int iseof){
     /*We can put here scanning hor jscripts and html and raw data ......*/
     int allow_transfer;
     if(data){
	  if(SEND_PERCENT_BYTES<100){
                          /*Allow transfer SEND_PERCENT_BYTES of the data*/
	       allow_transfer=(SEND_PERCENT_BYTES*(((av_req_data_t *)data)->body->endpos+len))/100;
	       ci_unlockdata_cached_file(((av_req_data_t *)data)->body,allow_transfer);
               /*debug_printf(10,"Allowing bytes for transfer:%d\n",allow_transfer);*/
	  }
	  return ci_write_cached_file(((av_req_data_t *)data)->body, buf,len ,iseof);
     }
     return -1;
}

int srvclamav_read(void *data,char *buf,int len){
     

     if(((av_req_data_t *)data)->virus_name!=NULL &&  !((av_req_data_t *)data)->has_error_page) {
	  /*Inform user. Q:How? Maybe with a mail......*/
	  return CI_EOF;    /* Do not send more data if a virus found and data has sent (readpos!=0)*/
     }
     /*if a virus foud and no data sent, an inform page has already generated*/


     if(data)
	  return ci_read_cached_file(((av_req_data_t *)data)->body,buf,len);
     return -1;
}


int srvclamav_end_of_data_handler(void *data,request_t *req){
     ci_cached_file_t *body=(data?((av_req_data_t *)data)->body:NULL);
     const char *virname;
     int ret=0;
     unsigned long int scanned_data=0;

     if(!data)
	  return CI_MOD_DONE;

     if(ci_ismem_cached_file(body)){
	  debug_printf(10,"Scan from mem\n");
	  ret=cl_scanbuff(body->buf,body->endpos,&virname,root);	  
     }
     else if(ci_isfile_cached_file(body)){
	  debug_printf(10,"Scan from file\n");
	  lseek(body->fd,0,SEEK_SET);
	  ret=cl_scandesc(body->fd,&virname,&scanned_data,root,&limits,CL_SCAN_STDOPT);
     }
  
    debug_printf(10,"Clamav engine scanned %d size of  data....\n",(scanned_data?scanned_data:body->endpos));

     if(ret==CL_VIRUS){
	  debug_printf(10,"VIRUS DETECTED:%s.\nTake action.......\n ",virname);
	  ((av_req_data_t *)data)->virus_name=virname;
	  if(!ci_req_sent_data(req)) /*If no data had sent we can send an error page  */
	       generate_error_page((av_req_data_t *)data,req,body);
	  else
	       debug_printf(10,"Simply not send other data\n");
     }
     else if (ret!= CL_CLEAN){
	  debug_printf(10,"srvClamAv module:An error occured while scanning the data\n");
     }

     ci_unlockalldata_cached_file(body);/*Unlock all data to continue send them.....*/

     return CI_MOD_DONE;     
}



/*******************************************************************************/
/* Other  functions                                                            */

scan_type_t scan_type(cli_file_t file_type){
     int indx=file_type-CL_TYPENO;

     if(!scantypes[indx])
	  return NONE;

     switch(file_type){
     CL_TYPE_UNKNOWN_DATA:
     CL_TYPE_MSEXE:
     CL_TYPE_DATA:
     CL_TYPE_TAR:
     CL_TYPE_GZ:
     CL_TYPE_ZIP:
     CL_TYPE_BZ:
     CL_TYPE_RAR:
     CL_TYPE_MSSZDD:
     CL_TYPE_MSOLE2:
     CL_TYPE_MSCAB:
     CL_TYPE_MSCHM:
     CL_TYPE_SCRENC:
     CL_TYPE_MAIL:
	  return SCAN_IN_FILE;

     CL_TYPE_UNKNOWN_TEXT:
     CL_TYPE_HTML:
     CL_TYPE_GRAPHICS:
	  return SCAN_IN_MEM;
     }     
}

char *error_message="<H1>A VIRUS FOUND</H1>"\
                      "You try to upload/download a file that contain the virus<br>";
char *tail_message="<p>This message generated by C-ICAP srvClamAV/antivirus module";

void generate_error_page(av_req_data_t *data,request_t *req,ci_cached_file_t *body){
     int new_size=0;
     new_size=strlen(error_message)+strlen(tail_message)+strlen(data->virus_name)+10;
     ci_req_respmod_reset_headers(req);
     ci_reset_cached_file(body,new_size);


     ci_req_respmod_add_header(req,"HTTP/1.1 200 OK");
     ci_req_respmod_add_header(req,"Server: C-ICAP");
     ci_req_respmod_add_header(req,"Connection: close");
     ci_req_respmod_add_header(req,"Content-Type: text/html");
     ci_req_respmod_add_header(req,"Content-Language: en");


     ci_write_cached_file(body,error_message,strlen(error_message),0);
     ci_write_cached_file(body,(char *)data->virus_name,strlen(data->virus_name),0);
     ci_write_cached_file(body,tail_message,strlen(tail_message),1);/*And here is the eof....*/
     ((av_req_data_t *)data)->has_error_page=1;
}


/****************************************************************************************/
/*Configuration Functions                                                               */

int SetScanFileTypes(char *directive,char **argv,void *setdata){
     int i,k;
//     debug_printf(1,"Setting parameter :%s=%s, %s\n",directive,argv[0],argv[1]);
     for(i=0;argv[i]!=NULL;i++){
	  
	  for(k=0;k<MAXTYPES;k++){
	       if(strcasecmp(argv[i],ft_description[k])==0){
		    scantypes[k]=1;
		    break;
	       }
	  }
     }
     debug_printf(10,"Iam going to scan data of type:");
     for(i=0;i<MAXTYPES;i++){
	  if(scantypes[i]==1)
	       debug_printf(10,",%s",ft_description[i]);
     }
     debug_printf(10,"\n");
}


int SetPercentBytes(char *directive,char **argv,void *setdata){
   int val=0;
   char *end;
   if(argv==NULL || argv[0]==NULL){
	debug_printf(1,"Missing arguments in directive %s \n",directive);
	return 0;
   }
   errno=0;
   val=strtoll(argv[0],&end,10);
   if(errno!=0 || val < 0 || val > 100 ){
	debug_printf(1,"Invalid argument in directive %s \n",directive);
	return 0;
   }
   
   SEND_PERCENT_BYTES=val;
   debug_printf(1,"Setting parameter :%s=%d\n",directive,val);
   return val;
}
