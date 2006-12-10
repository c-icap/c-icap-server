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
#include "dlib.h"
#include "cfg_param.h"



/****************************************************************
 Base functions for services support ....
*****************************************************************/
#define STEP 32

static service_module_t **service_list = NULL;
static int service_list_size;
static int services_num = 0;


static service_alias_t *service_aliases = NULL;
static int service_aliases_size;
static int service_aliases_num = 0;
service_module_t *find_alias_service(char *service_name);


service_module_t *create_service(char *service_file)
{
     char *extension;
     service_handler_module_t *service_handler;
     extension = strrchr(service_file, '.');
     service_handler = find_servicehandler_by_ext(extension);

     if (!service_handler)
          return NULL;
     return service_handler->create_service(service_file);

}


/*Must called only in initialization procedure.
  It is not thread-safe!
*/
service_module_t *register_service(char *service_file)
{
     service_module_t *service = NULL;

     if (service_list == NULL) {
          service_list_size = STEP;
          service_list = malloc(service_list_size * sizeof(service_module_t *));
     }
     else if (services_num == service_list_size) {
          service_list_size += STEP;
          service_list =
              realloc(service_list,
                      service_list_size * sizeof(service_module_t *));
     }

     if (service_list == NULL) {
          //log an error......and...
          exit(-1);
     }

     service = create_service(service_file);
     if (!service) {
          ci_debug_printf(1, "Error finding symbol \"service\" in  module %s\n",
                          service_file);
          return NULL;
     }

     if (service->mod_init_service)
          service->mod_init_service(service, &CONF);

     service_list[services_num++] = service;

     if (service->mod_conf_table)
          register_conf_table(service->mod_name, service->mod_conf_table,
                              MAIN_TABLE);

     return service;
}


service_module_t *find_service(char *service_name)
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (strcmp(service_list[i]->mod_name, service_name) == 0)
               return (service_list[i]);
     }
     return find_alias_service(service_name);
}


int post_init_services()
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (service_list[i]->mod_post_init_service != NULL) {
               service_list[i]->mod_post_init_service(service_list[i], &CONF);
          }
     }
     return 1;

}

/********************** Service aliases *****************************/
service_alias_t *add_service_alias(char *service_alias, char *service_name)
{
     service_module_t *service = NULL;
     int alias_indx = 0;
     if (service_aliases == NULL) {
          service_aliases = malloc(STEP * sizeof(service_alias_t));
          service_aliases_size = STEP;
     }
     else if (service_aliases_num == service_aliases_size) {
          service_aliases_size += STEP;
          service_aliases =
              realloc(service_aliases,
                      service_aliases_size * sizeof(service_alias_t));
     }

     if (service_aliases == NULL) {
          ci_debug_printf(1,
                          "add_service_alias:Error allocation memory. Exiting...\n");
          exit(-1);
     }

     service = find_service(service_name);
     if (!service)
          return NULL;

     alias_indx = service_aliases_num;
     service_aliases_num++;
     service_aliases[alias_indx].service = service;
     service_aliases[alias_indx].alias = strdup(service_alias);

     if (service->mod_conf_table)
          register_conf_table(service_aliases[alias_indx].alias,
                              service->mod_conf_table, ALIAS_TABLE);

     return &(service_aliases[alias_indx]);
}

service_module_t *find_alias_service(char *service_name)
{
     int i;
     for (i = 0; i < service_aliases_num; i++) {
          if (strcmp(service_aliases[i].alias, service_name) == 0)
               return (service_aliases[i].service);
     }
     return NULL;
}

/**********************************************************************
  The code for the default handler (C_handler)
  that handles services written in C/C++
  and loaded as dynamic libraries
 **********************************************************************/

service_module_t *load_c_service(char *service_file);
void release_c_handler();

service_handler_module_t c_service_handler = {
     "C_handler",
     ".so,.sa,.a",
     NULL,                      /*init */
     NULL,                      /*post_init */
     release_c_handler,
     load_c_service,
     NULL                       /*config table .... */
};

service_module_t *load_c_service(char *service_file)
{
     service_module_t *service = NULL;
     CI_DLIB_HANDLE service_handle;

     service_handle=ci_module_load(service_file, CONF.SERVICES_DIR);
     if(!service_handle)
	  return NULL;
     service=ci_module_sym(service_handle,"service");
     if(!service){
	  ci_debug_printf(1,"Not found symbol \"service\" in library, unload it\n");
	  ci_module_unload(service_handle,service_file);
	  return NULL;
     }
     ci_dlib_entry((service->mod_name!=NULL?service->mod_name:""), 
		   service_file, service_handle);
     return service;
}

void release_c_handler()
{
}
