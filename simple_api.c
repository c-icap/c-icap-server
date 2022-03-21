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
#include "encoding.h"
#include "request_util.h"
#include "debug.h"
#include <ctype.h>
#include <errno.h>

const char *ci_http_methods_str[] = {
    "UNKNOWN",
    "GET",
    "POST",
    "PUT",
    "HEAD",
    "CONNECT",
    "TRACE",
    "OPTIONS",
    "DELETE",
    NULL
};

const char *ci_http_method_string(int method)
{
    if (method <= CI_HTTP_METHOD_NONE || method >= CI_HTTP_METHOD_MAX)
        return "UNKNWON";
    return ci_http_methods_str[method];
}

/*
int ci_resp_check_body(ci_request_t *req){
     int i;
     ci_encaps_entity_t **e=req->entities;
     for(i = 0; e[i] != NULL; i++)
      if(e[i]->type == ICAP_NULL_BODY)
           return 0;
     return 1;
}
*/


ci_headers_list_t * ci_http_response_headers(ci_request_t * req)
{
    int i;
    ci_encaps_entity_t **e_list;
    e_list = req->entities;
    for (i = 0; e_list[i] != NULL && i < 3; i++) {     /*It is the first or second ellement */
        if (e_list[i]->type == ICAP_RES_HDR)
            return (ci_headers_list_t *) e_list[i]->entity;

    }
    return NULL;
}

ci_headers_list_t *ci_http_request_headers(ci_request_t * req)
{
    ci_encaps_entity_t **e_list;
    e_list = req->entities;
    if (e_list[0] != NULL && e_list[0]->type == ICAP_REQ_HDR)  /*It is always the first ellement */
        return (ci_headers_list_t *) e_list[0]->entity;

    /*We did not found the request headers but maybe there are in req->trash objects
      Trying to retrieve hoping that it were not desstroyed yet
     */
    if (req->trash_entities[ICAP_REQ_HDR] &&
            req->trash_entities[ICAP_REQ_HDR]->entity &&
            ((ci_headers_list_t *) req->trash_entities[ICAP_REQ_HDR]->entity)->used)
        return (ci_headers_list_t *) req->trash_entities[ICAP_REQ_HDR]->entity;

    return NULL;
}

int ci_http_response_reset_headers(ci_request_t * req)
{
    ci_headers_list_t *heads;
    if (!(heads =  ci_http_response_headers(req)))
        return 0;
    ci_headers_reset(heads);
    return 1;
}

int ci_http_request_reset_headers(ci_request_t * req)
{
    ci_headers_list_t *heads;
    if (!(heads = ci_http_request_headers(req)))
        return 0;
    ci_headers_reset(heads);
    return 1;
}

/*
 This function will be used when we want to responce with an error message
 to an reqmod request or respmod request.
 ICAP  rfc says that we must responce as:
 REQMOD  response encapsulated_list: {[reshdr] resbody}
 RESPMOD response encapsulated_list: [reshdr] resbody

 */
int ci_http_response_create(ci_request_t * req, int has_reshdr, int has_body)
{
    int i = 0;

    for (i = 0; i < 4; i++) {
        if (req->entities[i]) {
            ci_request_release_entity(req, i);
        }
    }
    i = 0;
    if (has_reshdr)
        req->entities[i++] = ci_request_alloc_entity(req, ICAP_RES_HDR, 0);
    if (has_body)
        req->entities[i] = ci_request_alloc_entity(req, ICAP_RES_BODY, 0);
    else
        req->entities[i] = ci_request_alloc_entity(req, ICAP_NULL_BODY, 0);

    return 1;
}


int ci_http_request_create(ci_request_t * req, int has_body)
{
    int i = 0;

    for (i = 0; i < 4; i++) {
        if (req->entities[i]) {
            ci_request_release_entity(req, i);
        }
    }
    i = 0;
    req->entities[i++] = ci_request_alloc_entity(req, ICAP_REQ_HDR, 0);
    if (has_body)
        req->entities[i] = ci_request_alloc_entity(req, ICAP_REQ_BODY, 0);
    else
        req->entities[i] = ci_request_alloc_entity(req, ICAP_NULL_BODY, 0);

    return 1;
}


const char *ci_http_response_add_header(ci_request_t * req, const char *header)
{
    ci_headers_list_t *heads;
    if (req->packed)  /*Not in edit mode*/
        return NULL;
    if (!(heads =  ci_http_response_headers(req)))
        return NULL;
    return ci_headers_add(heads, header);
}


const char *ci_http_request_add_header(ci_request_t * req, const char *header)
{
    ci_headers_list_t *heads;
    if (req->packed)  /*Not in edit mode*/
        return NULL;
    if (!(heads = ci_http_request_headers(req)))
        return NULL;
    return ci_headers_add(heads, header);
}

int ci_http_response_remove_header(ci_request_t * req, const char *header)
{
    ci_headers_list_t *heads;
    if (req->packed)  /*Not in edit mode*/
        return 0;
    if (!(heads =  ci_http_response_headers(req)))
        return 0;
    return ci_headers_remove(heads, header);
}


int ci_http_request_remove_header(ci_request_t * req, const char *header)
{
    ci_headers_list_t *heads;
    if (req->packed)  /*Not in edit mode*/
        return 0;
    if (!(heads = ci_http_request_headers(req)))
        return 0;
    return ci_headers_remove(heads, header);
}


const char *ci_http_response_get_header(ci_request_t * req, const char *head_name)
{
    ci_headers_list_t *heads;
    const char *val;
    if (!(heads =  ci_http_response_headers(req)))
        return NULL;
    if (!(val = ci_headers_value(heads, head_name)))
        return NULL;
    return val;
}

const char *ci_http_request_get_header(ci_request_t * req, const char *head_name)
{
    ci_headers_list_t *heads;
    const char *val;
    if (!(heads = ci_http_request_headers(req)))
        return NULL;
    if (!(val = ci_headers_value(heads, head_name)))
        return NULL;
    return val;
}


ci_off_t ci_http_content_length(ci_request_t * req)
{
    ci_headers_list_t *heads;
    const char *val;
    ci_off_t res = 0;
    char *e;
    if (!(heads =  ci_http_response_headers(req))) {
        /*Then maybe is a reqmod reauest, try to get request headers */
        if (!(heads = ci_http_request_headers(req)))
            return 0;
    }
    if (!(val = ci_headers_value(heads, "Content-Length")))
        return -1;

    errno = 0;
    res = ci_strto_off_t(val, &e, 10);
    if (errno == ERANGE && (res == CI_STRTO_OFF_T_MAX || res == CI_STRTO_OFF_T_MIN)) {
        ci_debug_printf(4, "Content-Length: overflow\n");
        return -2;
    }
    if (val == e) {
        ci_debug_printf(4, "Content-Length: not valid value: '%s' \n", val);
        return -2;
    }
    return res;
}

const char *ci_http_request(ci_request_t * req)
{
    ci_headers_list_t *heads;
    if (!(heads = ci_http_request_headers(req)))
        return NULL;

    if (!heads->used)
        return NULL;

    return heads->headers[0];
}

const char *ci_icap_add_xheader(ci_request_t * req, const char *header)
{
    return ci_headers_add(req->xheaders, header);
}

int ci_icap_append_xheaders(ci_request_t *req,ci_headers_list_t *headers)
{
    return ci_headers_addheaders(req->xheaders, headers);
}

CI_DECLARE_FUNC(const char *) ci_icap_request_get_header(ci_request_t *req, const char *header)
{
    if (req && req->request_header)
        return ci_headers_value(req->request_header, header);
    return NULL;
}

CI_DECLARE_FUNC(const char *) ci_icap_response_get_header(ci_request_t *req, const char *header)
{
    if (req && req->response_header)
        return ci_headers_value(req->response_header, header);
    return NULL;
}

#define header_end(e) (e == '\0' || e == '\n' || e == '\r')
int ci_http_request_url2(ci_request_t * req, char *buf, int buf_size, int flags)
{
    ci_headers_list_t *heads;
    const char *str, *host;
    int i, bytes;
    /*The request must have the form:
         GET url HTTP/X.X
    */
    if (!(heads = ci_http_request_headers(req)))
        return 0;

    if (!heads->used)
        return 0;

    str = heads->headers[0];

    if ((str = strchr(str, ' ')) == NULL) { /*Ignore method i*/
        return 0;
    }
    while (*str == ' ') /*ignore spaces*/
        str++;

    bytes = 0;
    if (*str == '/' && (host = ci_headers_value(heads,"Host"))) {
        /*Looks like a transparent proxy, we do not know the protocol lets try
          to preserve the major part of the url....
         */
        for (i = 0; (i < buf_size-1) && !header_end(host[i]) && !isspace((int)host[i]); i++) {
            buf[i] = host[i];
        }
        buf += i;
        buf_size -= i;
        bytes = i;
    }

    /*copy the url...*/
    if (flags & CI_HTTP_REQUEST_URL_ARGS)
        for (i = 0; (i < buf_size-1) && !header_end(str[i]) && !isspace((int)str[i]); i++)
            buf[i] = str[i];
    else
        for (i = 0; (i < buf_size-1) && !header_end(str[i]) && !isspace((int)str[i]) && str[i] != '?'; i++)
            buf[i] = str[i];

    buf[i] = '\0';
    bytes += i;
    return bytes;
}

int ci_http_request_url(ci_request_t * req, char *buf, int buf_size) {
    return ci_http_request_url2(req, buf, buf_size, 0);
}


const ci_ip_t * ci_http_client_ip(ci_request_t * req)
{
    const char *ip;
    if (!req)
        return NULL;

    if (req->xclient_ip.family == -1) /*Already check and failed to read it*/
        return NULL;

    if (req->xclient_ip.family != 0)  /*Already check, return cached*/
        return &req->xclient_ip;

    if (!(ip = ci_headers_value(req->request_header, "X-Client-IP")))
        return NULL;

#ifdef HAVE_IPV6
    if (strchr(ip, ':')) {
        if (ci_inet_aton(AF_INET6, ip, &(req->xclient_ip.address))) {
            req->xclient_ip.family = AF_INET6;
            ci_ipv6_inaddr_hostnetmask(req->xclient_ip.netmask);
        } else
            req->xclient_ip.family = -1;
    } else
#endif
    {
        if (ci_inet_aton(AF_INET, ip, &(req->xclient_ip.address))) {
            req->xclient_ip.family = AF_INET;
            ci_ipv4_inaddr_hostnetmask(req->xclient_ip.netmask);
        } else
            req->xclient_ip.family = -1;
    }

    if (req->xclient_ip.family == -1) /*Failed to read correctly*/
        return NULL;

    return &req->xclient_ip;
}

CI_DECLARE_FUNC(int) ci_http_response_content_encoding(ci_request_t *req)
{
    ci_headers_list_t *headers = ci_http_response_headers(req);
    if (headers) {
        const char *content_encoding = ci_headers_value(headers, "Content-Encoding");
        if (content_encoding)
            return ci_encoding_method(content_encoding);
        return CI_ENCODE_NONE;
    }
    return CI_ENCODE_UNKNOWN;
}
