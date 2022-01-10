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

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup HTTP API for HTTP object manipulation
 \ingroup API
 * Macros, functions and structures used for manipulating the encupsulated
 * HTTP objects (HTTP requests or HTTP responses).
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
 * Lock a ci_request_t object. After called the c-icap server stops sending
 * body data to the ICAP client.
 \param req is pointer to an object of type ci_request_t
 */
#define ci_req_lock_data(req) ((req)->data_locked = 1)

/**
 \def ci_req_unlock_data(ci_request_t)
 \ingroup REQUEST
 * Unlock a ci_request_t object. When called the c-icap server will start
 * sending body data to the ICAP client.
 \param req is pointer to an object of type ci_request_t
 */
#define ci_req_unlock_data(req) ((req)->data_locked = 0)

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
 \return  ICAP_OPTIONS, ICAP_REQMOD or ICAP_RESPMOD if the ICAP request is
 * options, request modification or response modification ICAP request
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
#define ci_req_allow204(req) ((req)->allow204)

/**
 \def ci_req_allow206(ci_request_t)
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return True (non zero int) if the ICAP request supports "Allow 206"
 */
#define ci_req_allow206(req) ((req)->allow206)

/**
 \def ci_req_allow206_outside_preview(ci_request_t)
 \ingroup REQUEST
 \param req  is pointer to an object of type ci_request_t
 \return True (non zero int) if the ICAP request supports "Allow 206" outside
 *       preview requests
 */
#define ci_req_allow206_outside_preview(req) ((req)->allow206 && (req)->allow204)


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
 \return True (non zero int) if the ICAP client has sent all the data
 *       (headers and body data) to the ICAP server
 */
#define ci_req_hasalldata(req)((req)->eof_received)

/**
 \ingroup HTTP
 The HTTP ptotocol methods
*/
enum ci_http_method {
    CI_HTTP_METHOD_NONE,
    CI_HTTP_METHOD_GET,
    CI_HTTP_METHOD_POST,
    CI_HTTP_METHOD_PUT,
    CI_HTTP_METHOD_HEAD,
    CI_HTTP_METHOD_CONNECT,
    CI_HTTP_METHOD_TRACE,
    CI_HTTP_METHOD_OPTIONS,
    CI_HTTP_METHOD_DELETE,
    CI_HTTP_METHOD_MAX,
};

/**
 * Returns the HTTP response headers.
 \ingroup HTTP
 * The string representation of the method which is of type ci_http_method
 */
CI_DECLARE_FUNC(const char *)ci_http_method_string(int method);

/**
 * Decodes a base64 encoded string.
 \ingroup UTILITY
 *
 \param str   is a buffer which holds the base64 encoded data
 \param result    is a buffer where the decoded data will be stored
 \param len    is the length of the result buffer
 \return the number of decoded bytes
 */
CI_DECLARE_FUNC(int) ci_base64_decode(const char *str,char *result,int len);

/**
 * Produces a base64 encoded string.
 \ingroup UTILITY
 *
 \param data   is a buffer which holds the data to be encoded
 \param datalen    is the length of the data buffer
 \param out    is a buffer where the encoded data will be stored
 \param outlen    is the length of the out buffer
 \return the number of decoded bytes
 */
CI_DECLARE_FUNC(int) ci_base64_encode(const unsigned char *data, size_t datalen, char *out, size_t outlen);

enum {
    CI_ENCODE_UNKNOWN = -1,
    CI_ENCODE_NONE = 0,
    CI_ENCODE_GZIP,
    CI_ENCODE_DEFLATE,
    CI_ENCODE_BZIP2,
    CI_ENCODE_BROTLI,
    CI_ENCODE_ZSTD
};

/**
 * Return the encoding method integer representation from string.
 \ingroup UTILITY
 *
 \param content_encoding The content encoding name
 \return the CI_ENCODE_* representation
*/
CI_DECLARE_FUNC(int) ci_encoding_method(const char *content_encoding);

/**
 * Uncompress a zipped string.
 \ingroup UTILITY
 *
 \param compress_method CI_ENCODE_GZIP, CI_ENCODED_DEFLATE or CI_CI_ENCODE_BZIP2
 \param buf   is a buffer which holds the zipped data
 \param len is the length of the buffer buf
 \param unzipped_buf  is the buffer where to store unzipped data
 \param unzipped_buf_len  is the length of the buffer to store unzipped data,
 *      and updated with the length of unzipped data
 \return CI_OK on success CI_ERROR on error
 */
CI_DECLARE_FUNC(int) ci_uncompress_preview(int compress_method, const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len);

enum CI_UNCOMPRESS_ERRORS {
    CI_UNCOMP_ERR_BOMB = -4,
    CI_UNCOMP_ERR_CORRUPT = -3,
    CI_UNCOMP_ERR_OUTPUT = -2,
    CI_UNCOMP_ERR_ERROR = -1,
    CI_UNCOMP_ERR_NONE = 0,
    CI_UNCOMP_OK = 1,
};

enum CI_COMPRESS_ERRORS {
    CI_COMP_ERR_BOMB = -4,
    CI_COMP_ERR_CORRUPT = -3,
    CI_COMP_ERR_OUTPUT = -2,
    CI_COMP_ERR_ERROR = -1,
    CI_COMP_ERR_NONE = 0,
    CI_COMP_OK = 1,
};

/**
 * Return a string representation of a decompress error code.
 \ingroup UTILITY
 *
 \param err a CI_UNCOMPRESS_ERRORS error code
*/
CI_DECLARE_FUNC(const char *) ci_decompress_error(int err);

/**
   Deprecated
 */
CI_DECLARE_FUNC(const char *) ci_inflate_error(int err);

struct ci_membuf;
struct ci_simple_file;

/*  Data Decompression core functions */

/**
 * Uncompress any compressed data that c-icap understands and writes the output to the outbuf
 * object, regardless of algorithm
 \ingroup UTILITY
 *
 \param encoding_format   is the enum for the encoding type
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_decompress_to_membuf(int encoding_format, const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_decompress_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_decompress_to_simple_file(int encoding_format, const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress deflate/gzip compressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_inflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_inflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_inflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress bzip2 compressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_bzunzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_bzunzip_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_bzunzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress brotli compressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_brinflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_brinflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_brinflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/*  Data Compression core functions */

/**
 * Compress any uncompressed data that c-icap understands and writes the output to the outbuf
 * object, regardless of algorithm
 \ingroup UTILITY
 *
 \param encoding_format   is the enum for the encoding type
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_compress_to_membuf(int encoding_format, const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_compress_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_compress_to_simple_file(int encoding_format, const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress deflate uncompressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_deflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_deflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_deflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress gzip uncompressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_gzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_deflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_gzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress bzip2 uncompressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_bzzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_bzzip_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_bzzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress brotli uncompressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_brdeflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_brdeflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_brdeflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Decodes a base64 encoded string, and also allocate memory for the result.
 \ingroup UTILITY
 *
 \param str is a buffer which holds the base64 encoded data
 \return a pointer to the decoded string. It uses malloc to allocate space for
 *       the decoded string so the free function should used to release the
 *       allocated memory.
 */
CI_DECLARE_FUNC(char *) ci_base64_decode_dup(const char *str);

/**
 */
/**
 * Returns the HTTP response headers.
 \ingroup HTTP
 *
 * This function is only valid for an ICAP responce modification request. If
 * the ICAP request is not responce modification ICAP request or there are
 * not response headers (HTTP 0.9) the function returns NULL.
 \param req A pointer to the current ICAP request object.
 \return Pointer to the HTTP response headers or NULL.
 */
CI_DECLARE_FUNC(ci_headers_list_t *) ci_http_response_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Returns the HTTP request headers.
 *
 * This function can used for an responce or request modification ICAP request
 * to get the HTTP request headers
 \param req is a pointer to the current ICAP request object.
 \return Pointer to the HTTP request headers or NULL if fails.
 */
CI_DECLARE_FUNC(ci_headers_list_t *) ci_http_request_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Add a custom header to the HTTP response headers.
 *
 * This function can used to add custom headers to the HTTP response and can
 * be used only for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header in the form "Header: value"
 \return Pointer to the header or NULL if fails.
 */
CI_DECLARE_FUNC(const char *) ci_http_response_add_header(ci_request_t *req, const char *header);

/**
 \ingroup HTTP
 \brief Add a custom header to the HTTP request headers.
 *
 * This function can used to add custom headers to the HTTP request and can be
 * used only for request modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header in the form "Header: value"
 \return Pointer to the header or NULL if fails.
 */
CI_DECLARE_FUNC(const char *) ci_http_request_add_header(ci_request_t *req, const char *header);

/**
 \ingroup HTTP
 \brief Remove a header from the HTTP response headers.
 *
 * This function can used to remove a header from the HTTP response and can be
 * used only for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header name
 \return Non zero if success or zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_response_remove_header(ci_request_t *req, const char *header);

/**
 \ingroup HTTP
 \brief Remove a header from the HTTP request headers.
 *
 * This function can used to remove a header from the HTTP request and can be
 * used only for request modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param header is a string contains the header name
 \return Non zero if success or zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_request_remove_header(ci_request_t *req, const char *header);

/**
 \ingroup HTTP
 \brief Get the value of the requested header from the HTTP response headers.
 *
 * This function can used to get the value of a header from the HTTP response
 * headers. It can be used only for response modification ICAP requests
 \param req is a pointer to the current ICAP request object.
 \param head_name is a string contains the header name
 \return A string with the header value on success NULL otherwise
 */
CI_DECLARE_FUNC(const char *) ci_http_response_get_header(ci_request_t *req, const char *head_name);

/**
 \ingroup HTTP
 \brief Get the value of the requested header from the HTTP request headers.
 *
 * This function can used to get the value of a header from the HTTP request
 * headers. It can be used on both request and response modification ICAP
 * requests.
 \param req is a pointer to the current ICAP request object.
 \param head_name is a string contains the header name
 \return A string with the header value on success NULL otherwise
 */
CI_DECLARE_FUNC(const char *) ci_http_request_get_header(ci_request_t *req, const char *head_name);

/**
 \ingroup HTTP
 \brief Completelly erase and initialize the  HTTP response headers.
 *
 * This function is usefull when the full rewrite of the HTTP response is
 * required. After this function called, the HTTP response should be filled
 * with new HTTP headers, before send back to the ICAP client.
 * An example of usage of this function is in antivirus service when a
 * virus detected in HTTP response, so the service blocks the response and
 * sends a new HTTP object (a new html page, with HTTP headers) informing
 * the user about the virus.
 * It can be used with response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_response_reset_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Completelly erase and initialize the  HTTP request headers.
 *
 * This function is usefull when an HTTP request required should replaced by
 * an other.After this function called, the HTTP request should filled with
 * new HTTP headers, before send back to the ICAP client.
 * An example use is to implement an HTTP redirector.
 * It can be used with request modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_request_reset_headers(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Creates a new HTTP response.
 *
 * This function is usefull when the service wants to respond with a self
 * created message to a response or request modification ICAP request.
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \param has_reshdr if it is non zero the HTTP response contrains HTTP headers
 *      (a non HTTP 0.9 response)
 \param has_body if it is non zero the HTTP response contains HTTP body data
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_response_create(ci_request_t *req, int has_reshdr, int has_body);

/**
 \ingroup HTTP
 \brief Creates a new HTTP request.
 *
 * This function is usefull to develop icap clients
 \param req is a pointer to the current ICAP request object.
 \param has_body if it is non zero the HTTP request contains HTTP body data
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int) ci_http_request_create(ci_request_t *req, int has_body);

/**
 \ingroup HTTP
 \brief Returns the value of the Content-Length header of the HTTP response
 *      or HTTP request for a response modification or request modification ICAP
 *      requests respectively.
 *
 * If the header Content-Length is not included in HTTP response
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \return The content length on success or a negative number otherwise
 */
CI_DECLARE_FUNC(ci_off_t) ci_http_content_length(ci_request_t *req);

/**
 * Return the encoding method integer representation from string.
 \ingroup UTILITY
 *
 \param req is a pointer to the current ICAP request object.
 \return the content encoding, CI_ENCODE_NONE for no encoding or CI_ENCODE_UNKNOWN for non RESPMOD ICAP requests
*/
CI_DECLARE_FUNC(int) ci_http_response_content_encoding(ci_request_t *req);

/**
 \ingroup HTTP
 \brief Returns the request line (e.g "GET /index.html HTTP 1.0") from http
 *      request headers
 *
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \return The request line in success or NULL otherwise
 */
CI_DECLARE_FUNC(const char *) ci_http_request(ci_request_t *req);

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
 \ingroup HTTP
 \brief Return the http client ip address if this information is available
 \param req is a pointer to the current ICAP request object.
 \return A const pointer to a ci_ip_t object contain the client ip address
 *       or NULL
 */
CI_DECLARE_FUNC(const ci_ip_t *) ci_http_client_ip(ci_request_t * req);

/**
 \ingroup REQUEST
 \brief Add an icap X-header to the icap response headers
 *
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \param header is the header to add in the form "Header: Value"
 \return pointer to the header in success or NULL otherwise
 */
CI_DECLARE_FUNC(const char *) ci_icap_add_xheader(ci_request_t *req, const char *header);

/**
 \ingroup REQUEST
 \brief Append the icap X-headers to the icap response headers
 *
 * It can be used with both request and response modification ICAP requests.
 \param req is a pointer to the current ICAP request object.
 \param headers is a pointer to the headers object to add
 \return pointer to the header in success or NULL otherwise
 */
CI_DECLARE_FUNC(int) ci_icap_append_xheaders(ci_request_t *req, ci_headers_list_t *headers);


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

#ifdef __cplusplus
}
#endif

#endif

