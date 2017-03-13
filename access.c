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
#include "request.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include "access.h"
#include "simple_api.h"
#include "net_io.h"



/********************************************************************************************/

extern access_control_module_t default_acl;
access_control_module_t *default_access_controllers[] = {
    &default_acl,
    NULL
};


access_control_module_t **used_access_controllers = default_access_controllers;

int access_reset()
{
    used_access_controllers = default_access_controllers;
    return 1;
}

int access_check_client(ci_request_t *req)
{
    int i = 0, res;
    if (!used_access_controllers)
        return CI_ACCESS_ALLOW;

    i = 0;
    while (used_access_controllers[i] != NULL) {
        if (used_access_controllers[i]->client_access) {
            res =
                used_access_controllers[i]->client_access(req);
            if (res != CI_ACCESS_UNKNOWN)
                return res;
        }
        i++;
    }
    return CI_ACCESS_ALLOW;
}


int check_request(ci_request_t * req)
{
    int res, i = 0;
    while (used_access_controllers[i] != NULL) {
        if (used_access_controllers[i]->request_access) {
            res = used_access_controllers[i]->request_access(req);
            if (res != CI_ACCESS_UNKNOWN)
                return res;
        }
        i++;
    }
    return CI_ACCESS_ALLOW;
}

int access_check_request(ci_request_t * req)
{
    int  res;

    if (!used_access_controllers)
        return CI_ACCESS_ALLOW;

    ci_debug_printf(9,"Going to check request for access control restrictions\n");

    res = check_request(req);

    ci_debug_printf(9,"Access control: %s\n", (res==CI_ACCESS_ALLOW?
                    "ALLOW":
                    (res==CI_ACCESS_DENY?"DENY":"UNKNOWN")));
    return res;
}

