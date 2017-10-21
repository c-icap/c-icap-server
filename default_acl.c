/*
 *  Copyright (C) 2004 Christos Tsantilas
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
#include "request.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include "access.h"
#include "simple_api.h"
#include "acl.h"
#include "net_io.h"
#include "common.h"


/*********************************************************************************************/
/* Default Authenticator  definitions                                                        */
int default_acl_init(struct ci_server_conf *server_conf);
int default_acl_post_init(struct ci_server_conf *server_conf);
void default_acl_release();
int default_acl_client_match(ci_request_t *req);
int default_acl_request_match(ci_request_t *req);

int cfg_default_acl_add(const char *directive, const char **argv, void *setdata);
int cfg_default_acl_access(const char *directive, const char **argv, void *setdata);


ci_access_entry_t *acl_connection_access_list = NULL;
ci_access_entry_t *acl_access_list = NULL;

/*Configuration Table .....*/
static struct ci_conf_entry acl_conf_variables[] = {
    {"acl", NULL, cfg_default_acl_add, NULL},
    {"client_access", NULL, cfg_default_acl_access, NULL},
    {"icap_access", NULL, cfg_default_acl_access, NULL},
    {NULL, NULL, NULL, NULL}
};


access_control_module_t default_acl = {
    "default_acl",
    default_acl_init,
    default_acl_post_init,     /*post_init */
    default_acl_release,
    default_acl_client_match,
    default_acl_request_match,
    acl_conf_variables
};

int default_acl_init(struct ci_server_conf *server_conf)
{

    return 1;
}

int default_acl_post_init(struct ci_server_conf *server_conf)
{
    return 1;
}

void default_acl_release()
{
    ci_access_entry_release(acl_access_list);
    ci_access_entry_release(acl_connection_access_list);
    acl_access_list = NULL;
    acl_connection_access_list = NULL;
}

int default_acl_client_match(ci_request_t *req)
{
    return ci_access_entry_match_request(acl_connection_access_list, req);
}

int default_acl_request_match(ci_request_t *req)
{
    return ci_access_entry_match_request(acl_access_list, req);
}


int cfg_default_acl_add(const char *directive, const char **argv, void *setdata)
{
    return 1;
}

int cfg_default_acl_access(const char *directive, const char **argv, void *setdata)
{
    int type, argc, error = 0;
    int only_connection = 0;
    const char *acl_spec_name;
    ci_access_entry_t **tolist,*access_entry;
    const ci_acl_spec_t *acl_spec;
    const ci_acl_type_t *spec_type ;

    if (argv[0] == NULL || argv[1] == NULL) {
        ci_debug_printf(1, "Parse error in directive %s \n", directive);
        return 0;
    }

    if (strcmp("client_access", directive) == 0) {
        tolist = &acl_connection_access_list;
        only_connection = 1;
    } else if (strcmp("icap_access", directive) == 0) {
        tolist = &acl_access_list;
    } else
        return 0;

    if (strcmp(argv[0], "allow") == 0) {
        type = CI_ACCESS_ALLOW;

    } else if (strcmp(argv[0], "deny") == 0) {
        type = CI_ACCESS_DENY;
    } else {
        ci_debug_printf(1, "Invalid directive :%s. Disabling %s acl rule \n",
                        argv[0], argv[1]);
        return 0;
    }

    if ((access_entry = ci_access_entry_new(tolist, type)) == NULL) {
        ci_debug_printf(1,"Error creating new access entry as %s access list\n", argv[0]);
        return 0;
    }

    ci_debug_printf(2,"Creating new access entry as %s with specs:\n", argv[0]);
    for (argc=1; argv[argc] != NULL; argc++) {
        acl_spec_name = argv[argc];
        acl_spec = ci_acl_search(acl_spec_name);
        if (acl_spec)
            spec_type = acl_spec->type;
        else
            spec_type = NULL;
        if (only_connection && spec_type &&
                strcmp(spec_type->name,"port") != 0 &&
                strcmp(spec_type->name,"src") != 0 &&
                strcmp(spec_type->name,"srvip") != 0 ) {
            ci_debug_printf(1, "Only \"port\", \"src\" and \"srvip\" acl types allowed in client_access access list (given :%s)\n", acl_spec_name);
            error = 1;
        } else {
            /*TODO: check return type.....*/
            ci_access_entry_add_acl_by_name(access_entry, acl_spec_name);
            ci_debug_printf(2,"\tAdding acl spec: %s\n", acl_spec_name);
        }
    }
    if (error)
        return 0;
    else
        return 1;
}
