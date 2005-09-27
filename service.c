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
#include "module.h"
#include "header.h"
#include "body.h"
#include "debug.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "cfg_param.h"



/****************************************************************
 Base functions for services support ....
*****************************************************************/

static service_module_t **service_list=NULL;
static int service_list_size;
static int services_num=0;
#define STEP 20

service_module_t *create_service(char *service_file){
     char *extension;
     service_handler_module_t *service_handler;
     extension=strrchr(service_file,'.');
     service_handler=find_servicehandler_by_ext(extension);
     
     if(!service_handler)
	  return NULL;
     return service_handler->create_service(service_file);

}


/*Must called only in initialization procedure.
  It is not thread-safe!
*/
service_module_t * register_service(char *service_file){ 
     service_module_t *service=NULL;

     if(service_list==NULL){
	  service_list_size=STEP;
	  service_list=malloc(service_list_size*sizeof(service_module_t *));
     }
     else if(services_num==service_list_size){
	  service_list_size+=STEP;
	  service_list=realloc(service_list,service_list_size*sizeof(service_module_t *));
     }

     if(service_list==NULL){
	  //log an error......and...
	  exit(-1);
     }
	  
     service=create_service(service_file);
     if(!service){
	  ci_debug_printf(1,"Error finding symbol \"service\" in  module %s\n",service_file);
	  return NULL;
     }

     if(service->mod_init_service)
	  service->mod_init_service(service,&CONF);
     
     service_list[services_num++]=service;

     if(service->mod_conf_table)
	  register_conf_table(service->mod_name,service->mod_conf_table);

     return service;
}


service_module_t *find_service(char *service_name){
     int i;
     for(i=0;i<services_num;i++){
	  if(strcmp(service_list[i]->mod_name,service_name)==0)
	       return (service_list[i]);
     }
     return NULL;
}




/**********************************************************************
  The code for the default handler (C_handler)
  that handles services written in C/C++
  and loaded as dynamic libraries
 **********************************************************************/

service_module_t *load_c_service(char *service_file);

service_handler_module_t c_service_handler={
     "C_handler",
     ".so,.sa,.a",
     NULL,/*init*/
     NULL,/*post_init*/
     load_c_service,
     NULL /*config table ....*/
};



service_module_t *load_c_service(char *service_file){
     service_module_t *service=NULL;
     char *path;
#ifdef HAVE_DLFCN_H 
     void *handle;
#else if defined (_WIN32)
     HMODULE handle;
     WCHAR filename[512];
     WCHAR c; int i=0;
#endif 
     
#ifdef HAVE_DLFCN_H 
     if(service_file[0]!='/'){
	  if((path=malloc((strlen(service_file)+strlen(CONF.SERVICES_DIR)+5)*sizeof(char)))==NULL)
	       return NULL;
	  strcpy(path,CONF.SERVICES_DIR);
	  strcat(path,"/");
	  strcat(path,service_file);
	  handle=dlopen(path,RTLD_NOW|RTLD_GLOBAL);	  
	  free(path);
     }
     else
	  handle=dlopen(service_file,RTLD_NOW|RTLD_GLOBAL);

     if(!handle){
          ci_debug_printf(1,"Error loading service %s: %s\n",service_file,dlerror());
	  return NULL;
     }
     service=dlsym(handle,"service");
     
#else if defined _WIN32
/*Maybe Windows specific code ..........*/
     /*Converting path to wide char .......*/
     for(i=0;i<strlen(service_file) && i< 511;i++)
	  filename[i]=service_file[i];
     filename[i]='\0';
     if( !(handle=LoadLibraryEx(filename,NULL,LOAD_WITH_ALTERED_SEARCH_PATH))
	&&
	!(handle=LoadLibraryEx(filename,NULL,NULL))){
	  ci_debug_printf(1,"Error loading service. Error code %d\n",GetLastError());
	  return NULL;
     }
     service=GetProcAddress(handle,"service");
#endif
     return service;
}


