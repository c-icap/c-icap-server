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


#ifndef __ACCESS_H
#define __ACCESS_H

#include "c-icap.h"
#include "request.h"
#include "net_io.h"


#define CI_ACCESS_ALLOW        1
#define CI_ACCESS_UNKNOWN      0
#define CI_ACCESS_DENY        -1
#define CI_ACCESS_PARTIAL     -2
#define CI_ACCESS_HTTP_AUTH   -3



#define HTTP_AUTH_NONE   0
#define HTTP_AUTH_BASIC  1
#define HTTP_AUTH_DIGEST 2


#define HTTP_MAX_PASS_LEN 256


struct http_auth_data{
     char http_user[MAX_USERNAME_LEN];
     char http_pass[MAX_PASS_LEN];
     int  http_auth_method; /*Digest or Basic ....*/
};



CI_DECLARE_FUNC(int) get_http_authorized_data(request_t *req,struct http_auth_data *auth_data);

CI_DECLARE_FUNC(int) access_check_client(ci_connection_t *connection);
CI_DECLARE_FUNC(int) access_check_request(request_t *req);
CI_DECLARE_FUNC(int) access_authenticate_request(request_t *req);


#endif

