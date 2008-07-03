/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
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

static ci_service_module_t **service_list = NULL;
static ci_service_xdata_t *service_extra_data_list = NULL;
static int service_list_size;
static int services_num = 0;


static service_alias_t *service_aliases = NULL;
static int service_aliases_size;
static int service_aliases_num = 0;
ci_service_module_t *find_service_by_alias(char *service_name);


ci_service_module_t *create_service(char *service_file)
{
     char *extension;
     service_handler_module_t *service_handler;
     extension = strrchr(service_file, '.');
     service_handler = find_servicehandler_by_ext(extension);

     if (!service_handler)
          return NULL;
     return service_handler->create_service(service_file);

}

void init_extra_data(ci_service_xdata_t * srv_xdata)
{
     ci_thread_rwlock_init(&srv_xdata->lock);
     strcpy(srv_xdata->ISTag, "ISTag: ");
     strcat(srv_xdata->ISTag, ISTAG "-XXXXXXXXX");
     memset(srv_xdata->xincludes, 0, XINCLUDES_SIZE + 1);
     memset(srv_xdata->TransferPreview, 0, MAX_HEADER_SIZE + 1);
     memset(srv_xdata->TransferIgnore, 0, MAX_HEADER_SIZE + 1);
     memset(srv_xdata->TransferComplete, 0, MAX_HEADER_SIZE + 1);
     srv_xdata->preview_size = 0;
     srv_xdata->allow_204 = 0;
     srv_xdata->xopts = 0;
}

/*Must called only in initialization procedure.
  It is not thread-safe!
*/
ci_service_module_t *register_service(char *service_file)
{
     ci_service_module_t *service = NULL;

     if (service_list == NULL) {
          service_list_size = STEP;
          service_list = malloc(service_list_size * sizeof(ci_service_module_t *));
          service_extra_data_list =
              malloc(service_list_size * sizeof(ci_service_xdata_t));
     }
     else if (services_num == service_list_size) {
          service_list_size += STEP;
          service_list =
              realloc(service_list,
                      service_list_size * sizeof(ci_service_module_t *));
          service_extra_data_list = realloc(service_extra_data_list,
                                            service_list_size *
                                            sizeof(ci_service_xdata_t));
     }

     if (service_list == NULL || service_extra_data_list == NULL) {
          ci_debug_printf(1,
                          "Fatal error:Can not allocate memory! Exiting imediatelly!\n");
          exit(-1);
     }

     service = create_service(service_file);
     if (!service) {
          ci_debug_printf(1, "Error finding symbol \"service\" in  module %s\n",
                          service_file);
          return NULL;
     }

     init_extra_data(&service_extra_data_list[services_num]);
     if (service->mod_init_service)
          service->mod_init_service(&service_extra_data_list[services_num],
                                    &CONF);

     service_list[services_num++] = service;

     if (service->mod_conf_table)
          register_conf_table(service->mod_name, service->mod_conf_table,
                              MAIN_TABLE);

     return service;
}


ci_service_module_t *find_service(char *service_name)
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (strcmp(service_list[i]->mod_name, service_name) == 0)
               return (service_list[i]);
     }
     return NULL;
/*     return find_service_by_alias(service_name);*/
}

ci_service_xdata_t *service_data(ci_service_module_t * srv)
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (service_list[i] == srv)
               return &(service_extra_data_list[i]);
     }
     return NULL;
}

int post_init_services()
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (service_list[i]->mod_post_init_service != NULL) {
               service_list[i]->
                   mod_post_init_service(&service_extra_data_list[i], &CONF);
          }
     }
     return 1;
}

int release_services()
{
     int i;
     for (i = 0; i < services_num; i++) {
          if (service_list[i]->mod_close_service != NULL) {
               service_list[i]->mod_close_service();
               ci_thread_rwlock_destroy(&service_extra_data_list[i].lock);
          }
     }
     services_num = 0;
     service_aliases_num = 0;
     return 1;
}


/********************** Service aliases *****************************/
service_alias_t *add_service_alias(char *service_alias, char *service_name,
                                   char *args)
{
     ci_service_module_t *service = NULL;
     service_alias_t *salias = NULL;
     int len = 0;
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
     if (!service) {
          salias = find_service_alias(service_name);
          if (!salias)
               return NULL;
          service = salias->service;
     }
     alias_indx = service_aliases_num;
     service_aliases_num++;
     service_aliases[alias_indx].service = service;
     strncpy(service_aliases[alias_indx].alias,
             service_alias, MAX_SERVICE_NAME);
     service_aliases[alias_indx].alias[MAX_SERVICE_NAME] = '\0';
     service_aliases[alias_indx].args[0] = '\0';
     if (salias) {
          len = strlen(salias->args);
          strcpy(service_aliases[alias_indx].args, salias->args);       /*we had check for len */
     }
     if (args && len) {
          service_aliases[alias_indx].args[len] = '&';
          len++;
     }
     if (args && MAX_SERVICE_ARGS - len > 0) {
          strcpy(service_aliases[alias_indx].args + len, args);
     }
     service_aliases[alias_indx].args[MAX_SERVICE_ARGS] = '\0';

     if (service->mod_conf_table)
          register_conf_table(service_aliases[alias_indx].alias,
                              service->mod_conf_table, ALIAS_TABLE);

     return &(service_aliases[alias_indx]);
}

ci_service_module_t *find_service_by_alias(char *service_name)
{
     int i;
     for (i = 0; i < service_aliases_num; i++) {
          if (strcmp(service_aliases[i].alias, service_name) == 0)
               return (service_aliases[i].service);
     }
     return NULL;
}

service_alias_t *find_service_alias(char *service_name)
{
     int i;
     for (i = 0; i < service_aliases_num; i++) {
          if (strcmp(service_aliases[i].alias, service_name) == 0)
               return &(service_aliases[i]);
     }
     return NULL;
}

/**********************************************************************
  The code for the default handler (C_handler)
  that handles services written in C/C++
  and loaded as dynamic libraries
 **********************************************************************/

ci_service_module_t *load_c_service(char *service_file);
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

ci_service_module_t *load_c_service(char *service_file)
{
     ci_service_module_t *service = NULL;
     CI_DLIB_HANDLE service_handle;

     service_handle = ci_module_load(service_file, CONF.SERVICES_DIR);
     if (!service_handle)
          return NULL;
     service = ci_module_sym(service_handle, "service");
     if (!service) {
          ci_debug_printf(1,
                          "Not found symbol \"service\" in library, unload it\n");
          ci_module_unload(service_handle, service_file);
          return NULL;
     }
     ci_dlib_entry((service->mod_name != NULL ? service->mod_name : ""),
                   service_file, service_handle);
     return service;
}

void release_c_handler()
{
}
