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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include "request.h"
#include "simple_api.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"
#if defined(USE_OPENSSL)
#include "net_io_ssl.h"
#endif


/*Must declared ....*/
int CONN_TIMEOUT = 300;

void printhead(void *d, const char *head, const char *value)
{
    if (!head || !*head) {
        ci_debug_printf(1, "\t%s\n", value);
    } else {
        ci_debug_printf(1, "\t%s: %s\n", head, value);
    }
}

void print_headers(ci_request_t * req)
{
    int type;
    ci_headers_list_t *headers;
    ci_debug_printf(1, "\nICAP HEADERS:\n");
    ci_headers_iterate(req->response_header, NULL, printhead);
    ci_debug_printf(1, "\n");

    if ((headers =  ci_http_response_headers(req)) == NULL) {
        headers = ci_http_request_headers(req);
        type = ICAP_REQMOD;
    } else
        type = ICAP_RESPMOD;

    if (headers) {
        ci_debug_printf(1, "%s HEADERS:\n", ci_method_string(type));
        ci_headers_iterate(headers, NULL, printhead);
        ci_debug_printf(1, "\n");
    }
}

void build_respmod_headers(int fd, ci_headers_list_t *headers)
{
    struct stat filestat;
    int filesize;
    char lbuf[512];
//     struct tm ltime;
    time_t ltimet;

    ci_headers_add(headers, "HTTP/1.0 200 OK");
    fstat(fd, &filestat);
    filesize = filestat.st_size;

    strcpy(lbuf, "Date: ");
    time(&ltimet);
    ctime_r(&ltimet, lbuf + strlen(lbuf));
    lbuf[strlen(lbuf) - 1] = '\0';
    ci_headers_add(headers, lbuf);

    strcpy(lbuf, "Last-Modified: ");
    ctime_r(&ltimet, lbuf + strlen(lbuf));
    lbuf[strlen(lbuf) - 1] = '\0';
    ci_headers_add(headers, lbuf);

    sprintf(lbuf, "Content-Length: %d", filesize);
    ci_headers_add(headers, lbuf);

}


void build_reqmod_headers(char *url, const char *method, int fd, ci_headers_list_t *headers)
{
    struct stat filestat;
    int filesize;
    char lbuf[1024];
    time_t ltimet;

    snprintf(lbuf,1024, "%s %s HTTP/1.0", method, url);
    lbuf[1023] = '\0';
    ci_headers_add(headers, lbuf);

    strcpy(lbuf, "Date: ");
    time(&ltimet);
    ctime_r(&ltimet, lbuf + strlen(lbuf));
    lbuf[strlen(lbuf) - 1] = '\0';

    if (fd > 0) {
        fstat(fd, &filestat);
        filesize = filestat.st_size;
        strcpy(lbuf, "Last-Modified: ");
        ctime_r(&ltimet, lbuf + strlen(lbuf));
        lbuf[strlen(lbuf) - 1] = '\0';
        ci_headers_add(headers, lbuf);

        sprintf(lbuf, "Content-Length: %d", filesize);
        ci_headers_add(headers, lbuf);
    }

    ci_headers_add(headers, lbuf);
    ci_headers_add(headers, "User-Agent: C-ICAP-Client/x.xx");

}


int fileread(void *fd, char *buf, int len)
{
    int ret;
    ret = read(*(int *) fd, buf, len);
    if (ret == 0)
        return CI_EOF;
    return ret;
}

int filewrite(void *fd, char *buf, int len)
{
    int ret;
    ret = write(*(int *) fd, buf, len);
    return ret;
}

void copy_data(int fd_in, int fd_out, ci_off_t copy_from)
{
    char buf[4095];
    size_t len;
    int ret;
    lseek(fd_in, copy_from, SEEK_SET);
    while ((len = read(fd_in, buf, sizeof(buf))) > 0) {
        ret = write(fd_out, buf, len);
        assert(ret == len);
    }
}

int add_xheader(const char *directive, const char **argv, void *setdata)
{
    ci_headers_list_t **xh = (ci_headers_list_t **)setdata;
    const char *h;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }
    h = argv[0];

    if (!strchr(h, ':')) {
        printf("The header :%s should have the form \"header: value\" \n", h);
        return 0;
    }

    if (*xh == NULL)
        *xh = ci_headers_create();

    ci_headers_add(*xh, h);
    return 1;
}


char *icap_server = "localhost";
int port = 1344;
int preview_size = -1;
char *service = "echo";
char *input_file = NULL;
char *output_file = NULL;
char *request_url = NULL;
char *resp_url = NULL;
char *method_str = "GET";
int send_headers = 1;
int send_preview = 1;
int allow204 = 1;
int allow206 = 0;
int verbose = 0;
ci_headers_list_t *xheaders = NULL;
ci_headers_list_t *http_xheaders = NULL;
ci_headers_list_t *http_resp_xheaders = NULL;
#if defined(USE_OPENSSL)
int use_tls = 0;
int tls_verify = 1;
const char *tls_method = NULL;
#endif
int VERSION_MODE = 0;
int USE_DEBUG_LEVEL = -1;

static struct ci_options_entry options[] = {
    {"-V", NULL, &VERSION_MODE, ci_cfg_version, "Print version and exits"},
    {"-VV", NULL, &VERSION_MODE, ci_cfg_build_info, "Print version and build informations and exits"},
    {
        "-i", "icap_servername", &icap_server, ci_cfg_set_str,
        "The icap server name"
    },
    {"-p", "port", &port, ci_cfg_set_int, "The server port"},
    {"-s", "service", &service, ci_cfg_set_str, "The service name"},
#if defined(USE_OPENSSL)
    {"-tls", NULL, &use_tls, ci_cfg_enable, "Use TLS"},
    {"-tls-method", "tls_method", &tls_method, ci_cfg_set_str, "Use TLS method"},
    {"-tls-no-verify", NULL, &tls_verify, ci_cfg_disable, "Disable server certificate verify"},
#endif
    {
        "-f", "filename", &input_file, ci_cfg_set_str,
        "Send this file to the icap server.\nDefault is to send an options request"
    },
    {
        "-o", "filename", &output_file, ci_cfg_set_str,
        "Save output to this file.\nDefault is to send to stdout"
    },
    {"-method", "method", &method_str, ci_cfg_set_str,"Use 'method' as method of the request modification"},
    {"-req","url",&request_url,ci_cfg_set_str,"Send a request modification instead of response modification"},
    {"-resp","url",&resp_url,ci_cfg_set_str,"Send a responce modification request with request url the 'url'"},
    {
        "-d", "level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "debug level info to stdout"
    },
    {
        "-noreshdr", NULL, &send_headers, ci_cfg_disable,
        "Do not send reshdr headers"
    },
    {"-nopreview", NULL, &send_preview, ci_cfg_disable, "Do not send preview data"},
    {"-no204", NULL, &allow204, ci_cfg_disable, "Do not allow204 outside preview"},
    {"-206", NULL, &allow206, ci_cfg_enable, "Support allow206"},
    {"-x", "xheader", &xheaders, add_xheader, "Include xheader in icap request headers"},
    {"-hx", "xheader", &http_xheaders, add_xheader, "Include xheader in http request headers"},
    {"-rhx", "xheader", &http_resp_xheaders, add_xheader, "Include xheader in http response headers"},
    {"-w", "preview", &preview_size, ci_cfg_set_int, "Sets the maximum preview data size"},
    {"-v", NULL, &verbose, ci_cfg_enable, "Print response headers"},
    {NULL, NULL, NULL, NULL}
};

void log_errors(ci_request_t * req, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

void vlog_errors(ci_request_t * req, const char *format, va_list ap)
{
    vfprintf(stderr, format, ap);
}

int main(int argc, char **argv)
{
    int fd_in = 0, fd_out = 0;
    int ret;
    char ip[CI_IPLEN];
    ci_connection_t *conn;
    ci_request_t *req;
    ci_headers_list_t *req_headers = NULL;
    ci_headers_list_t *resp_headers = NULL;

    ci_client_library_init();
    CI_DEBUG_LEVEL = 1;        /*Default debug level is 1 */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    if (VERSION_MODE)
        exit(0);

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

#if ! defined(_WIN32)
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */
#else
    __vlog_error = vlog_errors;        /*set c-icap library  log function for win32..... */
#endif

#if defined(USE_OPENSSL)
    if (use_tls) {
        ci_tls_client_options_t tlsOpts;
        ci_tls_init();

        memset((void *)&tlsOpts, 0, sizeof(ci_tls_client_options_t));
        tlsOpts.method = tls_method;
        tlsOpts.verify = tls_verify;
        ci_tls_pcontext_t ctx = ci_tls_create_context(&tlsOpts);

        if (!(conn = ci_tls_connect(icap_server, port, 0, ctx, CONN_TIMEOUT))) {
            ci_debug_printf(1, "Failed to establish SSL connection to the icap server.\n");
            exit(-1);
        }
    } else
#endif
        if (!(conn = ci_connect_to(icap_server, port, 0, CONN_TIMEOUT))) {
            ci_debug_printf(1, "Failed to connect to icap server.....\n");
            exit(-1);
        }

    req = ci_client_request(conn, icap_server, service);

    if (xheaders)
        ci_icap_append_xheaders(req, xheaders);

    ci_client_get_server_options(req, CONN_TIMEOUT);
    ci_debug_printf(10, "OK done with options!\n");
    ci_conn_remote_ip(conn, ip);
    ci_debug_printf(1, "ICAP server:%s, ip:%s, port:%d\n\n", icap_server, ip,
                    port);

    if (!send_preview) {
        req->preview = -1;
    } else if (preview_size >= 0 && preview_size < req->preview) {
        req->preview = preview_size;
    }

    /*If service does not support allow 204 disable it*/
    if (!req->allow204)
        allow204 = 0;

    if (!input_file && !request_url && !resp_url) {
        ci_debug_printf(1, "OPTIONS:\n");
        ci_debug_printf(1,
                        "\tAllow 204: %s\n\tPreview: %d\n\tKeep alive: %s\n",
                        (req->allow204 ? "Yes" : "No"), req->preview,
                        (req->keepalive ? "Yes" : "No")
                       );
        print_headers(req);
    } else {
        if (input_file && (fd_in = open(input_file, O_RDONLY)) < 0) {
            ci_debug_printf(1, "Error opening file %s\n", input_file);
            exit(-1);
        }

        if (output_file) {
            if ((fd_out =
                        open(output_file, O_CREAT | O_RDWR | O_EXCL,
                             S_IRWXU | S_IRGRP)) < 0) {
                ci_debug_printf(1, "Error opening output file %s\n",
                                output_file);
                exit(-1);
            }
        } else {
            fd_out = fileno(stdout);
        }

        ci_client_request_reuse(req);

        ci_debug_printf(10, "Preview:%d keepalive:%d,allow204:%d\n",
                        req->preview, req->keepalive, req->allow204);

        ci_debug_printf(10, "OK allocating request going to send request\n");

        req->type = ICAP_RESPMOD;
        if (allow204)
            req->allow204 = 1;
        if (allow206)
            req->allow206 = 1;

        if (xheaders)
            ci_icap_append_xheaders(req, xheaders);

        if (request_url) {
            req_headers = ci_headers_create();
            build_reqmod_headers(request_url, method_str, fd_in, req_headers);
            req->type = ICAP_REQMOD;
        } else if (send_headers) {
            resp_headers = ci_headers_create();
            build_respmod_headers(fd_in, resp_headers);
            if (resp_url) {
                req_headers = ci_headers_create();
                build_reqmod_headers(resp_url, method_str, 0, req_headers);
            }
        }

        if (req_headers && http_xheaders)
            ci_headers_addheaders(req_headers, http_xheaders);

        if (resp_headers && http_resp_xheaders)
            ci_headers_addheaders(resp_headers, http_resp_xheaders);

        ret = ci_client_icapfilter(req,
                                   CONN_TIMEOUT,
                                   req_headers,
                                   resp_headers,
                                   (fd_in > 0 ? (&fd_in): NULL),
                                   (int (*)(void *, char *, int)) fileread,
                                   &fd_out,
                                   (int (*)(void *, char *, int)) filewrite);

        if (ret == 206) {
            ci_debug_printf(1, "Partial modification (Allow 206 response): "
                            "use %ld from the original body data\n",
                            req->i206_use_original_body);
            copy_data(fd_in, fd_out, req->i206_use_original_body);

        }

        close(fd_in);
        close(fd_out);

        if (ret == 204) {
            ci_debug_printf(1,
                            "No modification needed (Allow 204 response)\n");
            if (output_file)
                unlink(output_file);
        }

        if (verbose)
            print_headers(req);

        ci_debug_printf(2, "Done\n");
    }
    ci_connection_destroy(conn);
    ci_client_library_release();
    return 0;
}
