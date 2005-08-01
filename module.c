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
#include "cfg_param.h"

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


static struct modules_list service_handlers;
service_handler_module_t *default_service_handler;

static struct modules_list loggers;
static struct modules_list access_controllers;
static struct modules_list auth_methods;
static struct modules_list authenticators;



static struct modules_list *modules_lists_table[]={/*Must follows the 'enum module_type' 
						     enumeration*/
     NULL,
     &service_handlers,
     &loggers,
     &access_controllers,
     &auth_methods,
     &authenticators
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
	  if((path=malloc((strlen(module_file)+strlen(CONF.MODULES_DIR)+5)*sizeof(char)))==NULL)
	       return NULL;
	  strcpy(path,CONF.MODULES_DIR);
	  strcat(path,"/");
	  strcat(path,module_file);
	  handle=dlopen(path,RTLD_LAZY|RTLD_GLOBAL);	  
	  free(path);
     }
     else
	  handle=dlopen(module_file,RTLD_LAZY|RTLD_GLOBAL);
     
     if(!handle){
          ci_debug_printf(1,"Error loading module %s:%s\n",module_file,dlerror());
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
	  ci_debug_printf(1,"Error loading module. Error code %d\n",GetLastError());
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
     else if(strcmp(type,"auth_method")==0){
	  return AUTH_METHOD;
     }
     else if(strcmp(type,"authenticator")==0){
	  return AUTHENTICATOR;
     }
     
     ci_debug_printf(1,"Uknown type of module:%s\n",type);
     return UNKNOWN;
}


static int init_module(void *module,enum module_type type){
     int ret;
     switch(type){
     case SERVICE_HANDLER:
	  if(((service_handler_module_t *)module)->init_service_handler)
	       ret=((service_handler_module_t *)module)->init_service_handler(&CONF);
	  if(((service_handler_module_t *)module)->conf_table)
	       register_conf_table(((service_handler_module_t *)module)->name,
				   ((service_handler_module_t *)module)->conf_table);
	  break;
     case LOGGER:
	  if(((logger_module_t *)module)->init_logger)
	       ret=((logger_module_t *)module)->init_logger(&CONF);
	  if(((logger_module_t *)module)->conf_table)
	       register_conf_table(((logger_module_t *)module)->name,
				   ((logger_module_t *)module)->conf_table);

	  break;
     case ACCESS_CONTROLLER:
	  if(((access_control_module_t *)module)->init_access_controller)
	       ret=((access_control_module_t *)module)->init_access_controller(&CONF);
	  if(((access_control_module_t *)module)->conf_table)
	       register_conf_table(((access_control_module_t *)module)->name,
				   ((access_control_module_t *)module)->conf_table);
	  break;
     case AUTH_METHOD:
	  if(((http_auth_method_t *)module)->init_auth_method)
	       ret=((http_auth_method_t *)module)->init_auth_method(&CONF);
	  if(((http_auth_method_t *)module)->conf_table)
	       register_conf_table(((http_auth_method_t *)module)->name,
				   ((http_auth_method_t *)module)->conf_table);
	  break;

     case AUTHENTICATOR:
	  if(((authenticator_module_t *)module)->init_authenticator)
	       ret=((authenticator_module_t *)module)->init_authenticator(&CONF);
	  if(((authenticator_module_t *)module)->conf_table)
	       register_conf_table(((authenticator_module_t *)module)->name,
				   ((authenticator_module_t *)module)->conf_table);
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


/******************************************************************/




http_auth_method_t *find_auth_method( char *method){
     int i;
     for(i=0;i<auth_methods.modules_num;i++){
	  if(strcmp(method,((http_auth_method_t *)auth_methods.modules[i])->name)==0)
	       return (http_auth_method_t *)auth_methods.modules[i];
     }
     return NULL;
}


/* The following function is a hacked version of find_auth_method function.
Also return an integer (method_id) which corresponds to a hash key points
to an array with authenticators which can handle the authentication method.
 */

http_auth_method_t *find_auth_method_id( char *method,int *method_id){
     int i;
     *method_id=0;
     for(i=0;i<auth_methods.modules_num;i++){
	  if(strcasecmp(method,((http_auth_method_t *)auth_methods.modules[i])->name)==0){
	       *method_id=i;
	       return (http_auth_method_t *)auth_methods.modules[i];
	  }
     }
     return NULL;
}


authenticator_module_t *find_authenticator(char *name){
     int i;
     for(i=0;i<authenticators.modules_num;i++){
	  if(strcmp(name,((authenticator_module_t *)authenticators.modules[i])->name)==0){
	       return (authenticator_module_t *)authenticators.modules[i];
	  }
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
     if(mod=find_auth_method(name)){
	  *type=AUTH_METHOD;
	  return mod;
     }
     if(mod=find_authenticator(name)){
	  *type=AUTHENTICATOR;
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
	  ci_debug_printf(1,"Error finding symbol \"module\" in  module %s\n",module_file);
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
	       ci_debug_printf(1,"Found handler %s for service with extension:%s\n",sh->name,extension);
	       return sh;
	  }
     }

     ci_debug_printf(1,"Not handler for extension :%s. Using default ...\n",extension);
     return default_service_handler;
}



/*************************************************************************************/

#define MAX_HASH_SIZE 256 /*Maybe, better a value of 10 or 20*/
struct auth_hash{
     authenticator_module_t ***hash; /*A 2-d array which contains pointers to authenticator_module_t*/
     int usedsize;
     int hash_size;
};

struct auth_hash authenticators_hash;

int init_auth_hash(struct auth_hash *hash){
     hash->usedsize=0;
     if(NULL==(hash->hash=malloc(STEP*sizeof(authenticator_module_t *)))){
	  hash->hash_size=STEP;
	  return 0;
     }
     hash->hash_size=STEP;
     memset(hash->hash,(int)NULL,hash->hash_size);
     return 1;
}

void release_auth_hash(struct auth_hash *hash){
     int i;
     for(i=0;i<hash->hash_size;i++){
	  if(hash->hash[i]!=NULL){
	       free(hash->hash[i]);
	  }
     }
     free(hash->hash);
     hash->hash=NULL;
}


authenticator_module_t **get_authenticators_list(struct auth_hash *hash,int method_id){
     if(method_id>hash->hash_size)
	  return NULL;
     return hash->hash[method_id];
}

int check_to_add_method_id(struct auth_hash *hash,int method_id){
     authenticator_module_t ***new_mem;
     if(method_id> MAX_HASH_SIZE || method_id <0){
	  ci_debug_printf(1,"Method id is %d. Possible bug, please report it to developers!!!!!!\n",method_id);
	  return 0;
     }
     
     while(hash->hash_size<method_id){
	  new_mem=realloc(hash->hash,hash->hash_size+STEP);
	  if(!new_mem){
	       ci_debug_printf(1,"Error allocating memory for authenticators hash!!!!!!\n");
	       return 0;
	  }
	  memset(hash->hash+hash->hash_size,(int)NULL,STEP);/*Reset the newly allocated memory */
	  hash->hash=new_mem;
	  hash->hash_size+=STEP;
     }
     return 1;
}


int methods_authenticators(struct auth_hash *hash,char *method_name,int method_id,char **argv){
     int i,k,auths_num;
     authenticator_module_t **new_mem,*auth_mod;

     if(!check_to_add_method_id(hash,method_id))
	  return 0;

     for(auths_num=0;argv[auths_num]!=NULL;auths_num++);
     
     if(NULL==(new_mem=malloc((auths_num+1)*sizeof(authenticator_module_t *)))){
	  ci_debug_printf(1,"Error allocating memory!!!!!!\n");
	  return 0;
     }
     memset(new_mem,(int)NULL,auths_num+1);
     if(hash->hash[method_id]!=NULL)
	  free(hash->hash[method_id]);
     hash->hash[method_id]=new_mem;

     k=0;
     for(i=0;i<auths_num;i++){
	       ci_debug_printf(1,"Authenticator %s......\n",argv[i]);
	  if((auth_mod=find_authenticator(argv[i]))==NULL){
	       ci_debug_printf(1,"Authenticator %s does not exist!!!!!\n",argv[i]);
	       continue;
	  }
	  if(strcasecmp(auth_mod->method,method_name)!=0){
	       ci_debug_printf(1,"Authenticator %s does not provides authentication method %s!!!!\n",
			    auth_mod->name,method_name);
	       continue;
	  }
	  new_mem[k++]=auth_mod;
     }
     new_mem[k]=NULL;
     return 1;
}


int set_method_authenticators(char *method_name,char **argv){
     int i,method_id,authenticators_num;
     http_auth_method_t *method_mod;
     
     if(!(method_mod=find_auth_method_id(method_name,&method_id))){
	  ci_debug_printf(1,"Authentication method \"%s\" not supported\n",method_name);
	  return 0;     
     }
     return methods_authenticators(&authenticators_hash,method_name,method_id,argv);
}


http_auth_method_t * get_authentication_schema(char *method_name,authenticator_module_t ***authenticators){
     int method_id;
     http_auth_method_t *method_mod;
     if(!(method_mod=find_auth_method_id(method_name,&method_id))){
	  *authenticators=NULL;
	  return NULL;
     }
     *authenticators=get_authenticators_list(&authenticators_hash,method_id);
     return method_mod;
}


/*************************************************************************************/


extern service_handler_module_t c_service_handler;
extern logger_module_t file_logger;
extern logger_module_t *default_logger;
extern access_control_module_t default_acl;
extern http_auth_method_t basic_auth;
extern authenticator_module_t file_basic;


int init_modules(){
     /*first initialize authenticators hash......*/
     init_auth_hash(&authenticators_hash);
     
     default_service_handler=&c_service_handler;
     add_to_modules_list(&service_handlers,default_service_handler);

     default_logger=&file_logger;
/*     init_module(default_logger,LOGGER); Must be called, if default module has conf table 
                                           or init_service_handler. */

     add_to_modules_list(&loggers,default_logger);

     init_module(&default_acl,ACCESS_CONTROLLER);
     add_to_modules_list(&access_controllers,&default_acl);

     add_to_modules_list(&auth_methods,&basic_auth);

     add_to_modules_list(&authenticators,&file_basic);
     
     return 1;
}

