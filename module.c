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
#include "module.h"
#include "header.h"
#include "body.h"
#include "debug.h"
#ifdef _WIN32
#include <windows.h>
#endif


struct modules_list{
     void **modules;
     int modules_num;
     int list_size;
};


/*
 service_module_t **module_list=NULL;
 int module_list_size;
 int modules_num=0;
*/
#define STEP 20
extern char *MODULES_DIR;

static struct modules_list service_handlers;
service_handler_module_t *default_service_handler;

static struct modules_list loggers;
static struct modules_list access_controllers;

static struct modules_list *modules_lists_table[]={/*Must follows the 'enum module_type' 
						     enumeration*/
     NULL,
     &service_handlers,
     &loggers,
     &access_controllers
};


void *load_module(char *module_file){
     void *service=NULL;
     char *path;
#ifdef HAVE_DLFCN_H 
     void *handle;
#else if defined (_WIN32)
     HMODULE handle;
     WCHAR filename[512];
     WCHAR c; int i=0;
#endif 
#ifdef HAVE_DLFCN_H
     if(module_file[0]!='/'){
	  if((path=malloc((strlen(module_file)+strlen(MODULES_DIR)+5)*sizeof(char)))==NULL)
	       return NULL;
	  strcpy(path,MODULES_DIR);
	  strcat(path,"/");
	  strcat(path,module_file);
	  handle=dlopen(path,RTLD_LAZY|RTLD_GLOBAL);	  
	  free(path);
     }
     else
	  handle=dlopen(module_file,RTLD_LAZY|RTLD_GLOBAL);
     
     if(!handle){
          debug_printf(1,"Error loading module %s\n",module_file);
	  return NULL;
     }
     service=dlsym(handle,"module");
     
#else if defined _WIN32
/*Maybe Windows specific code ..........*/
     /*Converting path to wide char .......*/
     for(i=0;i<strlen(module_file) && i< 511;i++)
	  filename[i]=module_file[i];
     filename[i]='\0';
     if( !(handle=LoadLibraryEx(filename,NULL,LOAD_WITH_ALTERED_SEARCH_PATH))
	&&
	!(handle=LoadLibraryEx(filename,NULL,NULL))){
	  debug_printf(1,"Error loading module. Error code %d\n",GetLastError());
	  return NULL;
     }
     service=GetProcAddress(handle,"module");
#endif
     return service;
}


/*
  Must called only in initialization procedure.
  It is not thread-safe!
*/


void *add_to_modules_list(struct modules_list *mod_list,void *module){
     if(mod_list->modules==NULL){
	  mod_list->list_size=STEP;
	  mod_list->modules=malloc(mod_list->list_size*sizeof(void *));
     }
     else if(mod_list->modules_num==mod_list->list_size){
	  mod_list->list_size+=STEP;
	  mod_list->modules=realloc(mod_list->modules, mod_list->list_size*sizeof(void *));
     }

     if(mod_list->modules==NULL){
	  //log an error......and...
	  exit(-1);
     }
     mod_list->modules[mod_list->modules_num++]=module;
     return module;
}


static int module_type(char *type){
     if(strcmp(type,"service_handler")==0){
	  return SERVICE_HANDLER;
     }
     else if(strcmp(type,"logger")==0){
	  return LOGGER;
     }
     else if(strcmp(type,"access_controller")==0){
	  return ACCESS_CONTROLLER;
     }

     debug_printf(1,"Uknown type of module:%s\n",type);
     return UNKNOWN;
}


static int init_module(void *module,enum module_type type){
     int ret;
     switch(type){
     case SERVICE_HANDLER:
	  if(((service_handler_module_t *)module)->init_service_handler())
	       ret=((service_handler_module_t *)module)->init_service_handler();
	  if(((service_handler_module_t *)module)->conf_table)
	       register_conf_table(((service_handler_module_t *)module)->name,
				   ((service_handler_module_t *)module)->conf_table);
	  break;
     case LOGGER:
	  if(((logger_module_t *)module)->init_logger)
	       ret=((logger_module_t *)module)->init_logger();
	  if(((logger_module_t *)module)->conf_table)
	       register_conf_table(((logger_module_t *)module)->name,
				   ((logger_module_t *)module)->conf_table);

	  break;
     case ACCESS_CONTROLLER:
	  if(((access_control_module_t *)module)->init_access_controller)
	       ret=((access_control_module_t *)module)->init_access_controller();
	  if(((access_control_module_t *)module)->conf_table)
	       register_conf_table(((access_control_module_t *)module)->name,
				   ((access_control_module_t *)module)->conf_table);
	  break;
     default:
	  return 0;
     }
     return ret;
}


logger_module_t *find_logger(char *name){
     logger_module_t *sh;
     int i;
     for(i=0; i<loggers.modules_num;i++){
	  sh=(logger_module_t *)loggers.modules[i];
	  if(sh->name && strcmp(sh->name,name)==0)
	       return sh;
     }
     return NULL;
     
}

access_control_module_t *find_access_controller(char *name){
     access_control_module_t *sh;
     int i;
     for(i=0; i<access_controllers.modules_num;i++){
	  sh=(access_control_module_t *)access_controllers.modules[i];
	  if(sh->name && strcmp(sh->name,name)==0)
	       return sh;
     }
     return NULL;     
}

service_handler_module_t *find_servicehandler(char *name){
     service_handler_module_t *sh;
     int i;
     for(i=0; i<service_handlers.modules_num;i++){
	  sh=(service_handler_module_t *)service_handlers.modules[i];
	  if(sh->name && strcmp(sh->name,name)==0)
	       return sh;
     }
     return NULL;
}

void *find_module(char *name,int *type){
     void *mod;
     if(mod=find_logger(name)){
	  *type=LOGGER;
	  return mod;
     }
     if(mod=find_servicehandler(name)){
	  *type=SERVICE_HANDLER;
	  return mod;
     }
     if(mod=find_access_controller(name)){
	  *type=ACCESS_CONTROLLER;
	  return mod;
     }
     *type=UNKNOWN;
     return NULL;
}

void *register_module(char *module_file,char *type){ 
     void *module=NULL;
     int mod_type;
     struct modules_list *l=NULL;
     
     l=modules_lists_table[mod_type=module_type(type)];
     if(l==NULL)
	  return NULL;
     
     module=load_module(module_file);
     if(!module){
	  debug_printf(1,"Error finding symbol \"module\" in  module %s\n",module_file);
	  return NULL;
     }
     
     init_module(module,mod_type);
     
     add_to_modules_list(l,module);
     return module;
}


service_handler_module_t *find_servicehandler_by_ext(char *extension){
     service_handler_module_t *sh;
     char *s;
     int i,len_extension,len_s,found=0;
     len_extension=strlen(extension);
     for(i=0; i<service_handlers.modules_num;i++){
	  sh=(service_handler_module_t *)service_handlers.modules[i];
	  s=sh->extensions;
	  
	  do{
	       if((s=strstr(s,extension))!=NULL){
		    len_s=strlen(s);
		    if( len_s>=len_extension && (strchr(",. \t",s[len_extension]) ||s[len_extension]=='\0') ){
			 found=1;
		    }
	       }
	       if(!s || len_extension>=len_s)/*There is no any more extensions.......*/
		    break;
	       s+=len_extension;
	  }while(s && !found);

	  if(found){
	       debug_printf(1,"Found handler %s for service with extension:%s\n",sh->name,extension);
	       return sh;
	  }
     }

     debug_printf(1,"Not handler for extension :%s. Using default ...\n",extension);
     return default_service_handler;
}


extern service_handler_module_t c_service_handler;
extern logger_module_t file_logger;
extern logger_module_t *default_logger;
extern access_control_module_t default_acl;
extern access_control_module_t **used_access_controllers;

int init_modules(){
     used_access_controllers=malloc(3*sizeof(access_control_module_t *));
     
     default_service_handler=&c_service_handler;
     add_to_modules_list(&service_handlers,default_service_handler);
     default_logger=&file_logger;
     add_to_modules_list(&loggers,default_logger);

     init_module(&default_acl,ACCESS_CONTROLLER);
     add_to_modules_list(&access_controllers,&default_acl);

     used_access_controllers[0]=&default_acl;
     used_access_controllers[1]=NULL;
/*     init_module(default_logger,LOGGER); Must be called, if default module has conf table 
                                           or init_service_handler. */
     return 1;
}

