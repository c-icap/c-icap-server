/*
 *  Copyright (C) 2004-2020 Christos Tsantilas
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

#ifndef __C_ICAP_CLIENT_H
#define __C_ICAP_CLIENT_H

#include "net_io.h"
#include "request.h"

#ifdef __cplusplus
extern "C"
{
#endif

CI_DECLARE_FUNC(ci_request_t *)  ci_client_request(ci_connection_t *conn,const char *server,const char *service);
CI_DECLARE_FUNC(void)         ci_client_request_reuse(ci_request_t *req);

CI_DECLARE_FUNC(int)          ci_client_get_server_options(ci_request_t *req,int timeout);
CI_DECLARE_FUNC(int)          ci_client_get_server_options_nonblocking(ci_request_t *req);

CI_DECLARE_FUNC(int)          ci_client_icapfilter(ci_request_t *req,
        int timeout,
        ci_headers_list_t *req_headers,
        ci_headers_list_t *resp_headers,
        void *data_source,
        int (*source_read)(void *,char *,int),
        void *data_dest,
        int (*dest_write) (void *,char *,int));

/**
 \ingroup ICAPCLIENT
 * Function to send HTTP objects to an ICAP server for processing. It sends
 * the HTTP request headers, and the HTTP response from HTTP server (headers
 * plus body data), and receives modified HTTP response headers and body data.
 \param req The ci_request_t object.
 \param io_action is a combination set of ci_wait_for_read and
 *      ci_wait_for_write flags. It has the meaning that the
 *      ci_client_icapfilter_nonblocking can read from or write to ICAP server.
 \param req_headers The HTTP request headers to use.
 \param resp_headers The HTTP response headers to use.
 \param data_source User data to use with source_read callback function.
 \param source_read Callback function to use for reading HTTP object body data.
 \param data_dest User data to use with dest_write callback function.
 \param dest_write Callback function to use for storing modified body data.
 \return combination of the following flags: NEEDS_TO_READ_FROM_ICAP,
 *       NEEDS_TO_WRITE_TO_ICAP, NEEDS_TO_READ_USER_DATA and
 *       NEEDS_TO_WRITE_USER_DATA.
 */
CI_DECLARE_FUNC(int) ci_client_icapfilter_nonblocking(ci_request_t * req, int io_action,
        ci_headers_list_t * req_headers,
        ci_headers_list_t * resp_headers,
        void *data_source,
        int (*source_read) (void *, char *, int),
        void *data_dest,
        int (*dest_write) (void *, char *, int));

CI_DECLARE_FUNC(int) ci_client_http_headers_completed(ci_request_t * req);

CI_DECLARE_FUNC(void) ci_client_set_user_agent(const char *agent);

CI_DECLARE_FUNC(void) ci_client_library_init();

CI_DECLARE_FUNC(void) ci_client_library_release();

/** Deprecated. Use ci_connect_to declared in net_io.h instead. */
CI_DECLARE_FUNC(ci_connection_t *)  ci_client_connect_to(char *servername,int port,int proto);

#ifdef __cplusplus
}
#endif

#endif /* __C_ICAP_CLIENT_H */
