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


#ifndef __SIMPLE_API_H
#define __SIMPLE_API_H

#include "c-icap.h"
#include "request.h"


/**
 \defgroup HTTP API for HTTP object manipulation
 \ingroup API
 * Macros, functions and structures used for manipulating the encupsulated HTTP objects 
 * (HTTP requests or HTTP responses).
 */

/**
 \defgroup UTILITY utility funtions
 \ingroup API
 * Utility functions
 */

/*The following defines are request related and should be moved to request.h include file*/
/**
 \def ci_req_lock_data(ci_request_t)
 \ingroup REQUEST
 * Lock a ci_request_t object. After called the c-icap server stops sending body data
 * to the ICAP client.
 \param req is pointer to an object of type ci_request_t
 */
#define ci_req_lock_data(req) ((req)->data_locked=1)

/**
 \def ci_req_unlock_data(ci_request_t)
 \ingroup REQUEST
 * Unlock a ci_request_t object. When called the c-icap server will start sending body data 
 * to the ICAP client.
 \param req is pointer to an object of type ci_request_t
 */
#define ci_req_unlock_data(req) ((req)->data_locked=0)

/**
 \def ci_req_hasbody(ci_request_t)
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return true (non zero int) if the ICAP request contains body data else zero
 */
#define ci_req_hasbody(req) ((req)->hasbody)

/**
 \def ci_req_type(ci_request_t) 
 \ingroup REQUEST
 \return  ICAP_OPTIONS, ICAP_REQMOD or ICAP_RESPMOD if the ICAP request is options, 
 * request modification or response modification ICAP request
 */
#define ci_req_type(req) ((req)->type)

/**
 \def ci_req_preview_size(ci_request_t) 
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return The ICAP preview size
 */
#define ci_req_preview_size(req) ((req)->preview) /*The preview data size*/

/**
 \def ci_req_allow204(ci_request_t) 
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return True (non zero int) if the ICAP request supports "Allow 204"
 */
#define ci_req_allow204(req)    ((req)->allow204)

/**
 \def ci_req_sent_data(ci_request_t) 
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return True (non zero int) if the c-icap server has send data to the client
 */
#define ci_req_sent_data(req)((req)->status)

/**
 \def ci_req_hasalldata(ci_request_t) 
 \ingroup REQUEST
 \param req is pointer to an object of type ci_request_t
 \return True (non zero int) if the ICAP client has sent all the data (headers and body data) 
 * to the ICAP server
 */
#define ci_req_hasalldata(req)((req)->eof_received)

/**
 * Decodes a base64 encoded string.
 \ingroup UTILITY
 *
 \param str   is a buffer which holds the base64 encoded data
 \param result    is a buffer where the decoded data will be stored
 \param len    is the length of the result buffer
 \return the number of decoded bytes
 */
CI_DECLARE_FUNC(int)                ci_base64_decode(char *str,char *result,int len);

/**
 * Decodes a base64 encoded string, and also allocate memory for the result.
 \ingroup UTILITY
 *
 \param str is a buffer which holds the base64 encoded data
 \return a pointer to the decoded string. It uses malloc to allocate space for the 
 * decoded string so the free function should used to release allocated memory.
 */
CI_DECLARE_FUNC(char *)             ci_base64_decode_dup(char *str);

/**
 */
/**
 * Returns the HTTP response headers.
 \ingroup HTTP
 *
 * This function is only valid for an ICAP responce modification request. If the ICAP request is not
 * responce modification ICAP request or there are not response headers (HTTP 0.9) 
 * the function returns NULL.
 \param req A pointer to the current ICAP request object.
 \return Pointer to the HTTP response headers or NULL.
 */
CI_DECLARE_FUNC(ci_headers_list_t *) ci_http_response_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Returns the HTTP request headers.
 *
 * This function can used for an responce or request modification ICAP request to get
 * the HTTP request headers
 \param req is a pointer to the current ICAP request object.
 \return Pointer to the HTTP request headers or NULL if fails.
 */
CI_DECLARE_FUNC(ci_headers_list_t *) ci_http_request_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Add a custom header to the HTTP response headers.
 *
 * This function can used to add custom headers to the HTTP response and can be used only 
 * for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header in the form "Header: value"
 \return Pointer to the header or NULL if fails.
 */
CI_DECLARE_FUNC(char *)              ci_http_response_add_header(ci_request_t *req,char *header);

/**
 \ingroup HTTP
 \brief Add a custom header to the HTTP request headers.
 *
 * This function can used to add custom headers to the HTTP request and can be used only 
 * for request modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header in the form "Header: value"
 \return Pointer to the header or NULL if fails.
 */
CI_DECLARE_FUNC(char *)              ci_http_request_add_header(ci_request_t *req,char *header);

/**
 \ingroup HTTP
 \brief Remove a header from the HTTP response headers.
 *
 * This function can used to remove a header from the HTTP response and can be used only 
 * for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header name
 \return Non zero if success or zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_response_remove_header(ci_request_t *req,char *header);

/**
 \ingroup HTTP
 \brief Remove a header from the HTTP request headers.
 *
 * This function can used to remove a header from the HTTP request and can be used only 
 * for request modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header name
 \return Non zero if success or zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_request_remove_header(ci_request_t *req,char *header);

/**
 \ingroup HTTP
 \brief Get the value of the requested header from the HTTP response headers.
 *
 * This function can used to get the value of a header from the HTTP response headers. It can be used only 
 * for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param head_name is a string contains the header name
 \return A string with the header value on success NULL otherwise
 */
CI_DECLARE_FUNC(char *)              ci_http_response_get_header(ci_request_t *req,char *head_name);

/**
 \ingroup HTTP
 \brief Get the value of the requested header from the HTTP request headers.
 *
 * This function can used to get the value of a header from the HTTP request headers. It can be used 
 * on both request and response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \param head_name is a string contains the header name
 \return A string with the header value on success NULL otherwise
 */
CI_DECLARE_FUNC(char *)              ci_http_request_get_header(ci_request_t *req,char *head_name);

/**
 \ingroup HTTP
 \brief Completelly erase and initialize the  HTTP response headers.
 *
 * This function is usefull when the full rewrite of the HTTP response required. After this function called,
 * the HTTP response should filled with new HTTP headers, before send back to the ICAP client.
 * An example of usage of this function is in antivirus service when a virus detected in HTTP response, 
 * so the service blocks the response and sends a new HTTP object (a new html page, with HTTP headers) 
 * informing the user about the virus.
 * It can be used with response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_response_reset_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Completelly erase and initialize the  HTTP request headers.
 *
 * This function is usefull when an HTTP request required should replaced by an other.After this function called,
 * the HTTP request should filled with new HTTP headers, before send back to the ICAP client.
 * It can be used to implement a HTTP redirector.
 * It can be used with request modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_request_reset_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Creates a new HTTP response.
 *
 * This function is usefull when the service wants to respond with a self created message to a response or
 * request modification ICAP request.
 * It can be used with both request and response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \param has_reshdr if it is non zero the HTTP response contrains HTTP headers (a non HTTP 0.9 response)
 \param has_body if it is non zero the HTTP response contains HTTP body data
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_response_create(ci_request_t *req, int has_reshdr ,int has_body);

/**
 \ingroup HTTP
 \brief Creates a new HTTP request.
 *
 * This function is usefull to develop icap clients
 \param req is a pointer to the current ICAP request object.
 \param has_body if it is non zero the HTTP request contains HTTP body data
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int)                 ci_http_request_create(ci_request_t * req, int has_body);

/**
 \ingroup HTTP
 \brief Returns the value of the Content-Length header of the HTTP response or HTTP request for a 
 * response modification or request modification ICAP requests respectively.
 *
 * If the header Content-Length is not included in HTTP response 
 * It can be used with both request and response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \return The content length on success zero otherwise
 */
CI_DECLARE_FUNC(ci_off_t)            ci_http_content_length(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Returns the request line (e.g "GET /index.html HTTP 1.0") from http request headers
 *
 * It can be used with both request and response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \return The request line in success or NULL otherwise
 */
CI_DECLARE_FUNC(char *)              ci_http_request(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Returns the URL (e.g "http://www.chtsanti.net") from http request
 *
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \param buf a buffer to store the url
 \param buf_size the "buf" buffer size
 \return The bytes written to the "buf" buffer
 */
CI_DECLARE_FUNC(int) ci_http_request_url(ci_request_t * req, char *buf, int buf_size);

/**
 \ingroup REQUEST
 \brief Add an icap X-header to the icap response headers 
 *
 * It can be used with both request and response modification ICAP requests. 
 \param req is a pointer to the current ICAP request object.
 \param header is the header to add in the form "Header: Value"
 \return pointer to the header in success or NULL otherwise
 */
CI_DECLARE_FUNC(char *)              ci_icap_add_xheader(ci_request_t *req,char *header);


#ifdef __CI_COMPAT
#define ci_respmod_headers           ci_http_response_headers
#define ci_reqmod_headers            ci_http_request_headers
#define ci_respmod_add_header        ci_http_response_add_header
#define ci_reqmod_add_header         ci_http_request_add_header
#define ci_respmod_remove_header     ci_http_response_remove_header
#define ci_reqmod_remove_header      ci_http_request_remove_header
#define ci_respmod_get_header        ci_http_response_get_header
#define ci_reqmod_get_header         ci_http_request_get_header
#define ci_respmod_reset_headers     ci_http_response_reset_headers
#define ci_reqmod_reset_headers      ci_http_request_reset_headers
#define ci_request_create_respmod    ci_http_response_create
#define ci_content_lenght            ci_http_content_length
#define ci_request_add_xheader       ci_icap_add_xheader
#endif


#endif

