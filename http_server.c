/*
 *  Copyright (C) 2004-2021 Christos Tsantilas
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
#include "debug.h"
#include "http_server.h"
#include "request.h"
#include "simple_api.h"
#include "access.h"
#include "array.h"
#include "acl.h"
#include "util.h"
#include "cfg_param.h"
#include "stats.h"
#include "body.h"

#include <assert.h>

extern int TIMEOUT;

static int STAT_WS_REQUESTS = -1;
static int STAT_WS_FAILED_REQUESTS = -1;
static int STAT_WS_BYTES_IN = -1;
static int STAT_WS_BYTES_OUT = -1;
static int STAT_WS_BODY_BYTES_IN = -1;
static int STAT_WS_BODY_BYTES_OUT = -1;

struct http_service_handler {
    int (*handler)(ci_request_t *req);
    /*Statistics info and other members:*/
};

ci_array_t *ServiceHandlers = NULL;


static int request_stat_entry_register(char *name, int type, char *gname)
{
    int stat = ci_stat_entry_register(name, type, gname);
    assert(stat >= 0);
    return stat;
}

void http_server_init()
{
    STAT_WS_REQUESTS = request_stat_entry_register("REQUESTS", STAT_INT64_T, "WEB SERVER");
    STAT_WS_FAILED_REQUESTS = request_stat_entry_register("FAILED REQUESTS", STAT_INT64_T, "WEB SERVER");
    STAT_WS_BYTES_IN = request_stat_entry_register("BYTES IN", STAT_KBS_T, "WEB SERVER");
    STAT_WS_BYTES_OUT = request_stat_entry_register("BYTES OUT", STAT_KBS_T, "WEB SERVER");
    STAT_WS_BODY_BYTES_IN = request_stat_entry_register("BODY BYTES IN", STAT_KBS_T, "WEB SERVER");
    STAT_WS_BODY_BYTES_OUT = request_stat_entry_register("BODY BYTES OUT", STAT_KBS_T, "WEB SERVER");
    ServiceHandlers = ci_array_new2(1024, sizeof(struct http_service_handler));
}

void http_server_close()
{
    if (ServiceHandlers) {
        ci_array_destroy(ServiceHandlers);
        ServiceHandlers = NULL;
    }
}

void ci_http_server_register_service(const char *path, int (*handler)(ci_request_t *req), unsigned flags)
{
    /*
      if (KIDS_STARTED || GO_MULTITHREAD) {
          ci_debug_printf(1, "ERROR: registering '%s' HTTP service. Web services can only be registered during c-icap initialization procedure\n");
          return;
      }
     */
    struct http_service_handler service_handler;
    service_handler.handler = handler;
    ci_array_add(ServiceHandlers, path, &service_handler, sizeof(struct http_service_handler));
}

static int http_server_response_send(ci_request_t *req)
{
    char buf[256];
    int len, ok;
    if (req->return_code < 0)
        req->return_code = EC_200;
    ci_headers_reset(req->response_header);
    /*ICAP status codes matches HTTP codes:*/
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s",
             ci_error_code(req->return_code), ci_error_code_string(req->return_code));
    ci_headers_add(req->response_header, buf);
    ci_headers_add(req->response_header, "Server: C-ICAP/" VERSION);

    strncpy(buf, "Date: ", sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    const size_t DATE_PREFIX_LEN = strlen(buf);
    ci_strtime_rfc822(buf + DATE_PREFIX_LEN);
    ci_headers_add(req->response_header, buf);

    if (req->keepalive)
        ci_headers_add(req->response_header, "Connection: keep-alive");
    else
        ci_headers_add(req->response_header, "Connection: close");

    ci_membuf_t *body = (ci_membuf_t *)req->service_data;
    if (body && ci_membuf_size(body)) {
        snprintf(buf, sizeof(buf), "Content-Length: %d", ci_membuf_size(body));
        ci_headers_add(req->response_header, buf);
    }

    if (!ci_headers_is_empty(req->xheaders)) {
        ci_headers_addheaders(req->response_header, req->xheaders);
    }
    ci_headers_pack(req->response_header);

    ok = 0;
    len = ci_connection_write(req->connection,
                              req->response_header->buf, req->response_header->bufused,
                              TIMEOUT);
    if (len <= 0)
        goto http_server_response_send_done;

    req->bytes_out = len;
    if (body && ci_membuf_size(body)) {
        len = ci_connection_write(req->connection, body->buf, ci_membuf_size(body), TIMEOUT);
        if (len <= 0)
            goto http_server_response_send_done;
        req->bytes_out += len;
        req->body_bytes_out = len;
    }
    ok = 1;

http_server_response_send_done:
    /*We are finishing sending*/
    req->status = SEND_EOF;
    req->http_bytes_out = req->bytes_out;

    return ok;
}

int ci_read_icap_header(ci_request_t * req, ci_headers_list_t * h, int timeout);
static int ci_read_http_header(ci_request_t * req, int timeout)
{
    int ret = ci_read_icap_header(req, req->request_header, timeout);
    req->http_bytes_in = req->bytes_in;
    return (ret == EC_100 ? EC_200 : ret);
}

static int http_server_request_parse_first_line(ci_request_t *req)
{
    const char *line = req->request_header->headers[0];
    const char *str = line;
    if (strncasecmp("GET", str, 3) != 0) {
        ci_debug_printf(7, "Http request protocol info, method not allowed: %.15s\n", str);
        return EC_405; /*Not allowed. Only GET method is allowed*/
    }
    req->type = CI_HTTP_METHOD_GET;
    str += 3;
    while(*str == ' ') ++str; /*Str point to the start of the path*/
    const char *end = str;
    while(*end && *end != ' ' && *end != '?') ++end;
    if (!*end) {
        ci_debug_printf(7, "Http request path parse error: %15s\n", str);
        return EC_400; /*Bad request, expect a " HTTP/major.min" after path*/
    }
    if ((end - str) >= MAX_SERVICE_NAME) {
        ci_debug_printf(7, "Http request protocol info path size exceed\n");
        return EC_404; /*"Not found" we have only paths of size HTTP_SERVER_MAX_PATH_SIZE */
    }
    strncpy(req->service, str, end - str);
    req->service[end-str] = '\0';

    if (*end == '?') {
        end++;
        str = end;
        while(*end && *end != ' ') ++end;
        if (!*end) {
            ci_debug_printf(7, "Http request path/args parse error: %15s\n", str);
            return EC_400; /*Bad request, expect a " HTTP/major.min" after path*/
        }

        const int args_size = end - str > MAX_SERVICE_ARGS ? MAX_SERVICE_ARGS : end - str;
        strncpy(req->args, str, args_size);
        req->args[args_size] = '\0';
    } else
        req->args[0] = '\0';
    str = end;
    while(*str && strchr(" \t\r\n", *str) != NULL) ++str;
    const int ret = sscanf(str, "HTTP/%d.%d", &req->proto_version.major, &req->proto_version.minor);
    if (ret != 2) {
        ci_debug_printf(7, "Http request protocol info parse error: %.15s\n", str);
        return EC_400; /*Bad request*/
    }

    if (req->proto_version.major !=  1 || req->proto_version.minor < 0 || req->proto_version.minor > 1) {
        ci_debug_printf(7, "Http request protocol versions not supported: %.15s\n", str);
        return EC_400; /*Bad request, protocol version not supported*/
    }
    return EC_200;
}

static int http_server_exec_service_handler(ci_request_t *req)
{
    const struct http_service_handler *handler;
    handler = ci_array_search(ServiceHandlers, req->service);
    if (!handler)
        return 0;
    req->return_code = EC_200;
    req->service_data = ci_membuf_new_sized(32768);
    if (!handler->handler(req)) {
        req->return_code = EC_500;
        ci_membuf_t *body = (ci_membuf_t *)req->service_data;
        ci_membuf_free(body);
        req->service_data = NULL;
    }
    http_server_response_send(req);
    return 1;
}

static int http_do_request(ci_request_t *req)
{
    int status = ci_read_http_header(req, TIMEOUT);
    if (status != EC_200) {
        if (req->request_header->bufused == 0)
            return -1; /* Did not read anything */
        goto http_do_request_bad_status;
    }
    if ((status = ci_headers_unpack(req->request_header)) != EC_100) {
        goto http_do_request_bad_status;
    }
    if ((status = http_server_request_parse_first_line(req)) != EC_200)
        goto http_do_request_bad_status;

    int auth_status = CI_ACCESS_ALLOW;
    if ((auth_status = access_check_request(req)) == CI_ACCESS_DENY) {
        status = (req->auth_required ? EC_401 : EC_403);
        ci_debug_printf(3, "HTTP request not allowed/authenticated, status: %d\n", auth_status);
        goto http_do_request_bad_status;
    }

    if (http_server_exec_service_handler(req)) {
        return 1;
    }

    req->keepalive = 0;
    req->return_code = EC_404; /*Service Not found*/
    http_server_response_send(req);
    return 0;

http_do_request_bad_status:
    req->keepalive = 0;
    req->return_code = status;
    http_server_response_send(req);
    return 0;
}

int http_process_request(ci_request_t *req)
{
    int status = 1;

    status = http_do_request(req);
    if (req->service_data) {
        ci_membuf_t *body = (ci_membuf_t *)req->service_data;
        ci_membuf_free(body);
        req->service_data = NULL;
    }

    if (status < 0 && req->request_header->bufused == 0) /*Did not read anything*/
        return CI_NO_STATUS;

    STATS_INT64_INC(STAT_WS_REQUESTS, 1);
    if (!status)
        STATS_INT64_INC(STAT_WS_FAILED_REQUESTS, 1);

    STATS_KBS_INC(STAT_WS_BYTES_IN, req->bytes_in);
    STATS_KBS_INC(STAT_WS_BYTES_OUT, req->bytes_out);
    STATS_KBS_INC(STAT_WS_BODY_BYTES_IN, req->body_bytes_in);
    STATS_KBS_INC(STAT_WS_BODY_BYTES_OUT, req->body_bytes_out);
    return status;
}
