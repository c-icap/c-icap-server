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


#ifndef __SIMPLE_API_H
#define __SIMPLE_API_H

#include "c-icap.h"
#include "request.h"


#define ci_req_lock_data(req) (req->data_locked=1)
#define ci_req_unlock_data(req) (req->data_locked=0)
#define ci_req_hasbody(req) (req->hasbody)
#define ci_req_type(req) (req->type)
#define ci_req_preview_size(req) (req->preview) /*The preview data size*/
#define ci_req_sent_data(req)(req->responce_status) /*if icap server has sent data
                                                      to client */


CI_DECLARE_FUNC(ci_header_list_t *)  ci_req_respmod_headers(request_t *req);
CI_DECLARE_FUNC(ci_header_list_t *)  ci_req_reqmod_headers(request_t *req);
CI_DECLARE_FUNC(char *)              ci_req_respmod_add_header(request_t *req,char *header);
CI_DECLARE_FUNC(char *)              ci_req_reqmod_add_header(request_t *req,char *header);
CI_DECLARE_FUNC(int)                 ci_req_respmod_revove_header(request_t *req,char *header);
CI_DECLARE_FUNC(int)                 ci_req_reqmod_revove_header(request_t *req,char *header);
CI_DECLARE_FUNC(char *)              ci_req_respmod_get_header(request_t *req,char *head_name);
CI_DECLARE_FUNC(int)                 ci_req_content_lenght(request_t *req);
CI_DECLARE_FUNC(int)                 ci_req_respmod_reset_headers(request_t *req);

#endif

