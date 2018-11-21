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

#include "common.h"
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
#include "simple_api.h"
#include "module.h"
#include "stats.h"
#include <errno.h>


/*Services tables declarations */
#define STEP 32

static ci_service_module_t **service_list = NULL;
static ci_service_xdata_t *service_extra_data_list = NULL;
static int service_list_size;
static int services_num = 0;


static service_alias_t *service_aliases = NULL;
static int service_aliases_size;
static int service_aliases_num = 0;



/**************************************************************/
/*service globale config table                                */

int cfg_srv_transfer_preview(const char *directive, const char **argv, void *setdata);
int cfg_srv_transfer_ignore(const char *directive, const char **argv, void *setdata);
int cfg_srv_transfer_complete(const char *directive, const char **argv, void *setdata);
int cfg_srv_preview_size(const char *directive, const char **argv, void *setdata);
int cfg_srv_max_connections(const char *directive, const char **argv, void *setdata);
int cfg_srv_options_ttl(const char *directive, const char **argv, void *setdata);
int cfg_srv_allow206(const char *directive, const char **argv, void *setdata);

static struct ci_conf_entry services_global_conf_table[] = {
    {"TransferPreview", NULL, cfg_srv_transfer_preview, NULL},
    {"TransferIgnore", NULL, cfg_srv_transfer_ignore, NULL},
    {"TransferComplete", NULL, cfg_srv_transfer_complete, NULL},
    {"PreviewSize", NULL, cfg_srv_preview_size, NULL},
    {"MaxConnections", NULL, cfg_srv_max_connections, NULL},
    {"OptionsTTL", NULL, cfg_srv_options_ttl, NULL},
    {"Allow206", NULL, cfg_srv_allow206, NULL},
    {NULL, NULL, NULL, NULL}
};

int cfg_srv_transfer_preview(const char *directive, const char **argv, void *setdata)
{
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    ci_service_set_transfer_preview(srv_xdata,argv[0]);
    return 1;
}

int cfg_srv_transfer_ignore(const char *directive, const char **argv, void *setdata)
{
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    ci_debug_printf(1, "Setting parameter: %s=%s\n", directive, argv[0]);
    ci_service_set_transfer_ignore(srv_xdata,argv[0]);
    return 1;
}

int cfg_srv_transfer_complete(const char *directive, const char **argv, void *setdata)
{
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    ci_debug_printf(1, "Setting parameter: %s=%s\n", directive, argv[0]);
    ci_service_set_transfer_complete(srv_xdata,argv[0]);
    return 1;
}

int cfg_srv_preview_size(const char *directive, const char **argv, void *setdata)
{
    int preview;
    char *end;
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    if (strcmp(argv[0], "-") == 0) {
        preview = -1;
        ci_debug_printf(2, "Setting parameter: %s=\"disable\"\n", directive);
    } else {
        errno = 0;
        preview = strtoll(argv[0], &end, 10);
        if (errno != 0 || preview < 0) {
            ci_debug_printf(1, "Invalid argument in directive %s \n", directive);
            return 0;
        }
        ci_debug_printf(2, "Setting parameter: %s=%d\n", directive, preview);
    }
    ci_service_set_preview(srv_xdata, preview);
    return 1;
}

int cfg_srv_max_connections(const char *directive, const char **argv, void *setdata)
{
    int max_connections;
    char *end;
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    errno = 0;
    max_connections = strtoll(argv[0], &end, 10);
    if (errno != 0 || max_connections < 0) {
        ci_debug_printf(1, "Invalid argument in directive %s \n", directive);
        return 0;
    }
    ci_debug_printf(2, "Setting parameter: %s=%d\n", directive, max_connections);
    ci_service_set_max_connections(srv_xdata, max_connections);
    return 1;
}

int cfg_srv_options_ttl(const char *directive, const char **argv, void *setdata)
{
    int ttl;
    int u;
    char *end = NULL;
    const char *units;
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    errno = 0;
    ttl = strtoll(argv[0], &end, 10);
    if (errno != 0 || ttl < 0) {
        ci_debug_printf(1, "Invalid argument in directive %s \n", directive);
        return 0;
    }

    if (end && *end != '\0')
        units = end;
    else if (argv[1] != NULL)
        units = argv[1];
    else
        units = NULL;

    if (units) {
        if (strcasecmp(units, "sec") == 0 || strcasecmp(units, "secs") == 0)
            u = 1;
        else if (strcasecmp(units, "min") == 0 || strcasecmp(units, "mins") == 0)
            u = 60;
        else if (strcasecmp(units, "hour") == 0 || strcasecmp(units, "hours") == 0)
            u = 60*60;
        else {
            ci_debug_printf(1, "Invalid units %s,  in directive %s \n", units, directive);
            return 0;
        }
    } else
        u = 1;

    ttl = ttl * u;
    ci_debug_printf(2, "Setting parameter: %s=%d\n", directive, ttl);
    ci_service_set_options_ttl(srv_xdata, ttl);
    return 1;
}

int cfg_srv_allow206(const char *directive, const char **argv, void *setdata)
{
    struct ci_service_xdata *srv_xdata = ( struct ci_service_xdata *)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
        return 0;
    }
    ci_debug_printf(1, "Setting parameter: %s=%s\n", directive, argv[0]);

    if (strcasecmp(argv[0], "off")) {
        ci_thread_rwlock_wrlock(&srv_xdata->lock);
        srv_xdata->disable_206 = 1;
        ci_thread_rwlock_unlock(&srv_xdata->lock);
    }

    return 1;
}

struct ci_conf_entry *create_service_conf_table(struct ci_service_xdata *srv_xdata,struct ci_conf_entry *user_table)
{
    int i,k,size;
    struct ci_conf_entry *table;
    for (i = 0; services_global_conf_table[i].name != NULL; i++);
    size = i;
    if (user_table) {
        for (i = 0; user_table[i].name != NULL; i++);
        size += i;
    }

    table = malloc((size+1)*sizeof(struct ci_conf_entry));
    if (!table)
        return NULL;

    for (i = 0;  services_global_conf_table[i].name != NULL; i++) {
        table[i].name = services_global_conf_table[i].name;
        table[i].data = srv_xdata;
        table[i].action = services_global_conf_table[i].action;
        table[i].msg = services_global_conf_table[i].msg;
    }
    k = i;
    if (user_table) {
        for (i = 0; user_table[i].name != NULL; i++, k++) {
            table[k].name = user_table[i].name;
            table[k].data = user_table[i].data;
            table[k].action = user_table[i].action;
            table[k].msg = user_table[i].msg;
        }
    }
    table[k].name = NULL;
    table[k].data = NULL;
    table[k].action = NULL;
    table[k].msg = NULL;
    return table;
}

/****************************************************************
 Base functions for services support ....
*****************************************************************/

ci_service_module_t *find_service_by_alias(const char *service_name);


ci_service_module_t *create_service(const char *service_file, const char *argv[])
{
    char *extension;
    service_handler_module_t *service_handler;
    extension = strrchr(service_file, '.');
    service_handler = find_servicehandler_by_ext(extension);

    if (!service_handler)
        return NULL;
    return service_handler->create_service(service_file, argv);
}

void init_extra_data(ci_service_xdata_t * srv_xdata, const char *service)
{
    char buf[1024];
    char stat_group[1024];
    memset(srv_xdata, 0, sizeof(ci_service_xdata_t));
    ci_thread_rwlock_init(&srv_xdata->lock);
    strcpy(srv_xdata->ISTag, "ISTag: ");
    strcat(srv_xdata->ISTag, CI_ISTAG "-XXXXXXXXX");
    memset(srv_xdata->xincludes, 0, XINCLUDES_SIZE + 1);
    memset(srv_xdata->TransferPreview, 0, MAX_HEADER_SIZE + 1);
    memset(srv_xdata->TransferIgnore, 0, MAX_HEADER_SIZE + 1);
    memset(srv_xdata->TransferComplete, 0, MAX_HEADER_SIZE + 1);
    srv_xdata->preview_size = 0;
    srv_xdata->allow_204 = 0;
    srv_xdata->allow_206 = 0;
    srv_xdata->disable_206 = 0;
    srv_xdata->max_connections = -1;
    srv_xdata->xopts = 0;
    srv_xdata->status = CI_SERVICE_NOT_INITIALIZED;
    srv_xdata->options_ttl = -1;
    srv_xdata->option_handlers = NULL;

    snprintf(stat_group, 1023, "Service %s", service);
    stat_group[1023] = '\0';

    buf[1023] = '\0';
    snprintf(buf, 1023, "Service %s REQMODS", service);
    srv_xdata->stat_reqmods = ci_stat_entry_register(buf, CI_STAT_INT64_T, stat_group);

    snprintf(buf, 1023, "Service %s RESPMODS", service);
    srv_xdata->stat_respmods = ci_stat_entry_register(buf, CI_STAT_INT64_T, stat_group);

    snprintf(buf, 1023, "Service %s OPTIONS", service);
    srv_xdata->stat_options = ci_stat_entry_register(buf, CI_STAT_INT64_T, stat_group);

    snprintf(buf, 1023, "Service %s ALLOW 204", service);
    srv_xdata->stat_allow204 = ci_stat_entry_register(buf, CI_STAT_INT64_T, stat_group);

    snprintf(buf, 1023, "Service %s BYTES IN", service);
    srv_xdata->stat_bytes_in = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);

    snprintf(buf, 1023, "Service %s BYTES OUT", service);
    srv_xdata->stat_bytes_out = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);

    snprintf(buf, 1023, "Service %s HTTP BYTES IN", service);
    srv_xdata->stat_http_bytes_in = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);

    snprintf(buf, 1023, "Service %s HTTP BYTES OUT", service);
    srv_xdata->stat_http_bytes_out = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);

    snprintf(buf, 1023, "Service %s BODY BYTES IN", service);
    srv_xdata->stat_body_bytes_in = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);

    snprintf(buf, 1023, "Service %s BODY BYTES OUT", service);
    srv_xdata->stat_body_bytes_out = ci_stat_entry_register(buf, CI_STAT_KBS_T, stat_group);
}

/*Must called only in initialization procedure.
  It is not thread-safe!
*/



ci_service_module_t *add_service(ci_service_module_t *service)
{
    struct ci_service_xdata *xdata=NULL;
    struct ci_conf_entry *cfg_table;
    int ret;

    if (service_list == NULL) {
        service_list_size = STEP;
        service_list = malloc(service_list_size * sizeof(ci_service_module_t *));
        service_extra_data_list =
            malloc(service_list_size * sizeof(ci_service_xdata_t));
    } else if (services_num == service_list_size) {
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
                        "Fatal error: Cannot allocate memory! Exiting immediately!\n");
        exit(-1);
    }


    xdata = &service_extra_data_list[services_num];
    init_extra_data(xdata, service->mod_name);
    if (service->mod_init_service) {
        ret = service->mod_init_service(xdata, &CI_CONF);
        if (ret != CI_OK)
            xdata->status = CI_SERVICE_ERROR;
        else
            xdata->status = CI_SERVICE_OK;
    } else
        xdata->status = CI_SERVICE_OK;

    service_list[services_num++] = service;

    cfg_table = create_service_conf_table(xdata, service->mod_conf_table);
    xdata->intl_srv_conf_table = cfg_table;

    if (cfg_table)
        register_conf_table(service->mod_name, cfg_table,
                            MAIN_TABLE);

    return service;
}

ci_service_module_t *register_service(const char *service_file, const char *argv[])
{
    ci_service_module_t *service;
    service = create_service(service_file, argv);
    if (!service) {
        ci_debug_printf(3, "Error while loading service %s\n",
                        service_file);
        return NULL;
    }

    if (find_service(service->mod_name) != NULL) {
        ci_debug_printf(1, "Error, the service %s  is already loaded\n", service->mod_name);
        return NULL;
    }

    return add_service(service);
}


ci_service_module_t *find_service(const char *service_name)
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

extern ci_service_module_t info_service;
int init_services()
{
    int ret =0;

    if (add_service(&info_service) != NULL)
        ret = 1;

    return ret;
}

int post_init_services()
{
    int i, ret;
    ci_service_xdata_t *xdata;
    for (i = 0; i < services_num; i++) {
        if (service_list[i]->mod_post_init_service != NULL) {
            xdata = &service_extra_data_list[i];
            if ( xdata->status == CI_SERVICE_OK) {
                ret = service_list[i]->
                      mod_post_init_service(xdata, &CI_CONF);
                if (ret != CI_OK)
                    xdata->status = CI_SERVICE_ERROR;
            }
        }
    }
    return 1;
}

int release_services()
{
    int i;
    struct ci_conf_entry *table;

    for (i = 0; i < service_aliases_num; i++) {
        unregister_conf_table(service_aliases[i].alias);
    }

    for (i = 0; i < services_num; i++) {
        if (service_list[i]->mod_close_service != NULL) {
            service_list[i]->mod_close_service();
            ci_thread_rwlock_destroy(&service_extra_data_list[i].lock);
            table = unregister_conf_table(service_list[i]->mod_name);
            if (table!=service_extra_data_list[i].intl_srv_conf_table) {
                ci_debug_printf(1, "Error unregistering service %s configuration table\n",
                                service_list[i]->mod_name);
            }
            if (table)
                free(table);
            service_extra_data_list[i].intl_srv_conf_table = NULL;
            if (service_extra_data_list[i].option_handlers) {
                ci_list_destroy(service_extra_data_list[i].option_handlers);
                service_extra_data_list[i].option_handlers = NULL;
            }
        }
    }

    free(service_list);
    service_list = NULL;
    service_list_size = 0;
    services_num = 0;

    free(service_aliases);
    service_aliases = NULL;
    service_aliases_size = 0;
    service_aliases_num = 0;
    return 1;
}

int run_services_option_handlers(ci_service_xdata_t *srv_xdata, struct ci_request *req)
{
    const struct ci_option_handler *oh;
    int ret;
    char buf[1024];
    if (!srv_xdata->option_handlers)
        return CI_OK;
    for (oh = ci_list_first(srv_xdata->option_handlers); oh != NULL; oh = ci_list_next(srv_xdata->option_handlers)) {
        if (oh->handler) {
            ret = oh->handler(req);
            if (ret != CI_OK) {
                ci_debug_printf(1, "The %s Options check of %s service failed\n", oh->name, req->current_service_mod->mod_name);
                snprintf(buf, sizeof(buf), "X-C-ICAP-Fail: %s", oh->name);
                ci_icap_add_xheader(req, buf);
                return CI_ERROR;
            }
        }
    }
    return CI_OK;
}

/********************** Service aliases *****************************/
service_alias_t *add_service_alias(const char *service_alias, const char *service_name,
                                   const char *args)
{
    ci_service_module_t *service = NULL;
    service_alias_t *salias = NULL;
    ci_service_xdata_t *xdata = NULL;

    int len = 0;
    int alias_indx = 0;

    if (!strcmp(service_alias, service_name)) {
        ci_debug_printf(1, "Warning, alias is the same as service_name, not adding\n");
        return NULL;
    }

    if (service_aliases == NULL) {
        service_aliases = malloc(STEP * sizeof(service_alias_t));
        service_aliases_size = STEP;
    } else if (service_aliases_num == service_aliases_size) {
        service_aliases_size += STEP;
        service_aliases =
            realloc(service_aliases,
                    service_aliases_size * sizeof(service_alias_t));
    }

    if (service_aliases == NULL) {
        ci_debug_printf(1,
                        "add_service_alias: Error allocating memory. Exiting...\n");
        exit(-1);
    }

    service = find_service(service_name);
    if (!service) {
        salias = find_service_alias(service_name);
        if (!salias)
            return NULL;
        service = salias->service;
    }
    xdata = service_data(service);

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

    if (xdata->intl_srv_conf_table)
        register_conf_table(service_aliases[alias_indx].alias,
                            xdata->intl_srv_conf_table, ALIAS_TABLE);

    return &(service_aliases[alias_indx]);
}

ci_service_module_t *find_service_by_alias(const char *service_name)
{
    int i;
    for (i = 0; i < service_aliases_num; i++) {
        if (strcmp(service_aliases[i].alias, service_name) == 0)
            return (service_aliases[i].service);
    }
    return NULL;
}

service_alias_t *find_service_alias(const char *service_name)
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

ci_service_module_t *load_c_service(const char *service_file, const char *argv[]);
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

ci_service_module_t *load_c_service(const char *service_file, const char *argv[])
{
    ci_service_module_t *service = NULL;
    CI_DLIB_HANDLE service_handle;
    int forceUnload = 1;
    while (*argv != NULL) {
        if (strcasecmp(*argv, "forceUnload=off") == 0)
            forceUnload = 0;
        argv++;
    }
    service_handle = ci_module_load(service_file, CI_CONF.SERVICES_DIR);
    if (!service_handle)
        return NULL;
    service = ci_module_sym(service_handle, "service");
    if (!service) {
        ci_debug_printf(1,
                        "Not found symbol \"service\" in library, unload it\n");
        ci_module_unload(service_handle, service_file);
        return NULL;
    }

    if (ci_module_sym(service_handle, CI_MOD_DISABLE_FORCE_UNLOAD_STR))
        forceUnload = 0;

    ci_dlib_entry((service->mod_name != NULL ? service->mod_name : ""),
                  service_file, service_handle, forceUnload);
    return service;
}

void release_c_handler()
{
}
