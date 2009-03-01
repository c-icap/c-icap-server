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
#include "request.h"
#include "simple_api.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"


/*Must declared ....*/
int CONN_TIMEOUT = 300;

void print_headers(ci_request_t * req)
{
     int i;
     int type;
     ci_headers_list_t *headers;
     ci_debug_printf(1, "\nICAP HEADERS:\n");
     for (i = 0; i < req->response_header->used; i++) {
          ci_debug_printf(1, "\t%s\n", req->response_header->headers[i]);
     }
     ci_debug_printf(1, "\n");

     if ((headers =  ci_http_response_headers(req)) == NULL) {
          headers = ci_http_request_headers(req);
          type = ICAP_REQMOD;
     }
     else
          type = ICAP_RESPMOD;

     if (headers) {
          ci_debug_printf(1, "%s HEADERS:\n", ci_method_string(type));
          for (i = 0; i < headers->used; i++) {
               if (headers->headers[i])
                    ci_debug_printf(1, "\t%s\n", headers->headers[i]);
          }
          ci_debug_printf(1, "\n");
     }
}


int fileread(void *fd, char *buf, int len)
{
     int ret;
     ret = read(*(int *) fd, buf, len);
     return ret;
}

int filewrite(void *fd, char *buf, int len)
{
     int ret;
     ret = write(*(int *) fd, buf, len);
     return ret;
}

char *icap_server = "localhost";
int port = 1344;
char *service = "echo";
char *input_file = NULL;
char *output_file = NULL;
int RESPMOD = 1;
int send_headers = 1;
int verbose = 0;

static struct ci_options_entry options[] = {
     {"-i", "icap_servername", &icap_server, ci_cfg_set_str,
      "The icap server name"},
     {"-p", "port", &port, ci_cfg_set_int, "The server port"},
     {"-s", "service", &service, ci_cfg_set_str, "The service name"},
     {"-f", "filename", &input_file, ci_cfg_set_str,
      "Send this file to the icap server.\nDefault is to send an options request"},
     {"-o", "filename", &output_file, ci_cfg_set_str,
      "Save output to this file.\nDefault is to send to the stdout"},
/*     {"-req",NULL,&RESPMOD,ci_cfg_disable,"Send a request modification instead of responce modification"},*/
     {"-d", "level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
      "debug level info to stdout"},
     {"-noreshdr", NULL, &send_headers, ci_cfg_disable,
      "Do not send reshdr headers"},
     {"-v", NULL, &verbose, ci_cfg_enable, "Print responce headers"},
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
     int fd_in, fd_out;
     int ret;
     char ip[CI_IPLEN];
     ci_connection_t *conn;
     ci_request_t *req;
     ci_headers_list_t *headers;

     CI_DEBUG_LEVEL = 1;        /*Default debug level is 1 */
     ci_cfg_lib_init();

     if (!ci_args_apply(argc, argv, options)) {
          ci_args_usage(argv[0], options);
          exit(-1);
     }

#if ! defined(_WIN32)
     __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */
#else
     __vlog_error = vlog_errors;        /*set c-icap library  log function for win32..... */
#endif


     if (!(conn = ci_client_connect_to(icap_server, port, AF_INET))) {
          ci_debug_printf(1, "Failed to connect to icap server.....\n");
          exit(-1);
     }

     req = ci_client_request(conn, icap_server, service);

     ci_client_get_server_options(req, CONN_TIMEOUT);
     ci_debug_printf(10, "OK done with options!\n");
     ci_conn_remote_ip(conn, ip);
     ci_debug_printf(1, "ICAP server:%s, ip:%s, port:%d\n\n", icap_server, ip,
                     port);

     if (!input_file) {
          ci_debug_printf(1, "OPTIONS:\n");
          ci_debug_printf(1,
                          "\tAllow 204: %s\n\tPreview: %d\n\tKeep alive: %s\n",
                          (req->allow204 ? "Yes" : "No"), req->preview,
                          (req->keepalive ? "Yes" : "No")
              );
          print_headers(req);
     }
     else {
          if ((fd_in = open(input_file, O_RDONLY)) < 0) {
               ci_debug_printf(1, "Error openning file %s\n", input_file);
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
          }
          else {
               fd_out = fileno(stdout);
          }

          ci_client_request_reuse(req);

          ci_debug_printf(10, "Preview:%d keepalive:%d,allow204:%d\n",
                          req->preview, req->keepalive, req->allow204);

          ci_debug_printf(10, "OK allocating request going to send request\n");

          if (send_headers) {
               headers = ci_headers_create();
               ci_headers_add(headers, "Filetype: Unknown");
               ci_headers_add(headers, "User: chtsanti");
          }
          else
               headers = NULL;

          ret = ci_client_icapfilter(req,
                                     CONN_TIMEOUT,
                                     headers,
                                     &fd_in,
                                     (int (*)(void *, char *, int)) fileread,
                                     &fd_out,
                                     (int (*)(void *, char *, int)) filewrite);
          close(fd_in);
          close(fd_out);

          if (ret == 204) {
               ci_debug_printf(1,
                               "No modification needed (Allow 204 responce)\n");
               if (output_file)
                    unlink(output_file);
          }

          if (verbose)
               print_headers(req);

          ci_debug_printf(2, "Done\n");
     }
     close(conn->fd);
     return 0;
}
