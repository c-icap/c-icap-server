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


#ifndef __C_ICAP_ACCESS_H
#define __C_ICAP_ACCESS_H

#include "c-icap.h"
#include "request.h"
#include "net_io.h"


#ifdef __cplusplus
extern "C"
{
#endif

/**************************************************/
/*Basic authentication method definitions ......  */

#define HTTP_MAX_PASS_LEN 256

struct http_basic_auth_data {
    char http_user[MAX_USERNAME_LEN+1];
    char http_pass[HTTP_MAX_PASS_LEN+1];
};


int access_reset();
int http_authorize(ci_request_t *req, char *method);
int http_authenticate(ci_request_t *req, char *method);
int access_check_client(ci_request_t *req);
int access_check_request(ci_request_t *req);
int access_authenticate_request(ci_request_t *req);
int access_check_logging(ci_request_t *req);


#ifdef __cplusplus
}
#endif

#endif

