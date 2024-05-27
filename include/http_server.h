/*
 *  Copyright (C) 2004-2022 Christos Tsantilas
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

/**
 \defgroup HTTP_SERVER API
 \ingroup API
 * API for implementing utility functions to register and use c-icap's
 * HTTP server services
 */

#ifndef __C_ICAP_HTTP_SERVER_H
#define __C_ICAP_HTTP_SERVER_H
#include "c-icap.h"
#include "request.h"
#include "simple_api.h"
#include "body.h"
#include <assert.h>

CI_DECLARE_FUNC(void) ci_http_server_register_service(const char *path, int (*handler)(ci_request_t *req), unsigned);

static inline ci_membuf_t *ci_http_server_response_body(ci_request_t *req) {
    assert(req);
    return (ci_membuf_t *)req->service_data;
}

static inline ci_headers_list_t *ci_http_server_request_headers(ci_request_t *req) {
    assert(req);
    return req->request_header;
}

static inline void ci_http_server_response_set_status(ci_request_t *req, int status) {
    assert(req);
    req->return_code = status;
}

static inline const char *ci_http_server_response_add_header(ci_request_t *req, const char *header) {
    assert(req);
    return ci_icap_add_xheader(req, header);
}

#endif
