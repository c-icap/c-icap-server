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
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#ifdef _NOTUSED
#include <sys/ioctl.h>
#endif

#include "debug.h"
#include "request.h"
#include "service.h"
#include "access.h"
#include "util.h"
#include "simple_api.h"
#include "cfg_param.h"
#include "stats.h"
#include "body.h"


extern int TIMEOUT;
extern int KEEPALIVE_TIMEOUT;
extern const char *DEFAULT_SERVICE;
extern int PIPELINING;
extern int CHECK_FOR_BUGGY_CLIENT;
extern int ALLOW204_AS_200OK_ZERO_ENCAPS;
extern int FAKE_ALLOW204;

/*This variable defined in mpm_server.c and become 1 when the child must 
  halt imediatelly:*/
extern int CHILD_HALT;

#define FORBITTEN_STR "ICAP/1.0 403 Forbidden\r\n\r\n"
/*#define ISTAG         "\"5BDEEEA9-12E4-2\""*/

static int STAT_REQUESTS = -1;
static int STAT_FAILED_REQUESTS = -1;
static int STAT_BYTES_IN = -1;
static int STAT_BYTES_OUT = -1;
static int STAT_HTTP_BYTES_IN = -1;
static int STAT_HTTP_BYTES_OUT = -1;
static int STAT_BODY_BYTES_IN = -1;
static int STAT_BODY_BYTES_OUT = -1;
static int STAT_REQMODS = -1;
static int STAT_RESPMODS = -1;
static int STAT_OPTIONS = -1;
static int STAT_ALLOW204 = -1;

void request_stats_init()
{
  STAT_REQUESTS = ci_stat_entry_register("REQUESTS", STAT_INT64_T, "General");
  STAT_REQMODS = ci_stat_entry_register("REQMODS", STAT_INT64_T, "General");
  STAT_RESPMODS = ci_stat_entry_register("RESPMODS", STAT_INT64_T, "General");
  STAT_OPTIONS = ci_stat_entry_register("OPTIONS", STAT_INT64_T, "General");
  STAT_FAILED_REQUESTS = ci_stat_entry_register("FAILED REQUESTS", STAT_INT64_T, "General");
  STAT_ALLOW204 = ci_stat_entry_register("ALLOW 204", STAT_INT64_T, "General");
  STAT_BYTES_IN = ci_stat_entry_register("BYTES IN", STAT_KBS_T, "General");
  STAT_BYTES_OUT = ci_stat_entry_register("BYTES OUT", STAT_KBS_T, "General");
  STAT_HTTP_BYTES_IN = ci_stat_entry_register("HTTP BYTES IN", STAT_KBS_T, "General");
  STAT_HTTP_BYTES_OUT = ci_stat_entry_register("HTTP BYTES OUT", STAT_KBS_T, "General");
  STAT_BODY_BYTES_IN = ci_stat_entry_register("BODY BYTES IN", STAT_KBS_T, "General");
  STAT_BODY_BYTES_OUT = ci_stat_entry_register("BODY BYTES OUT", STAT_KBS_T, "General");
}

static int wait_for_data(ci_connection_t *conn, int secs, int what_wait)
{
    int wait_status;

    /*if we are going down do not wait....*/
    if (CHILD_HALT) 
        return -1;

    do {
        wait_status = ci_connection_wait(conn, secs, what_wait);
        if (wait_status < 0)
            return -1;
        if (wait_status == 0 && CHILD_HALT) /*abort*/
            return -1;
    } while (wait_status & ci_wait_should_retry);

    if (wait_status == 0) /* timeout */
        return -1;

    return wait_status;
}

ci_request_t *newrequest(ci_connection_t * connection)
{
     ci_request_t *req;
     int access;
     int len;
     ci_connection_t *conn;

     conn = (ci_connection_t *) malloc(sizeof(ci_connection_t));
     assert(conn);
     ci_copy_connection(conn, connection);
     req = ci_request_alloc(conn);

     if ((access = access_check_client(req)) == CI_ACCESS_DENY) { /*Check for client access */
          len = strlen(FORBITTEN_STR);
          ci_connection_write(connection, FORBITTEN_STR, len, TIMEOUT);
	  ci_request_destroy(req);
          return NULL;          /*Or something that means authentication error */
     }


     req->access_type = access;
     return req;
}


int recycle_request(ci_request_t * req, ci_connection_t * connection)
{
     int access;
     int len;

     ci_request_reset(req);
     ci_copy_connection(req->connection, connection);

     if ((access = access_check_client(req)) == CI_ACCESS_DENY) { /*Check for client access */
          len = strlen(FORBITTEN_STR);
          ci_connection_write(connection, FORBITTEN_STR, len, TIMEOUT);
          return 0;             /*Or something that means authentication error */
     }
     req->access_type = access;
     return 1;
}

int keepalive_request(ci_request_t *req)
{
    /* Preserve extra read bytes*/
    char *pstrblock = req->pstrblock_read;
    int pstrblock_len = req->pstrblock_read_len;
    // Just reset without change or free memory
    ci_request_reset(req);
    if (PIPELINING) {
        req->pstrblock_read = pstrblock;
        req->pstrblock_read_len = pstrblock_len;
    }

    if (req->pstrblock_read && req->pstrblock_read_len > 0)
        return 1;
    return wait_for_data(req->connection, KEEPALIVE_TIMEOUT, ci_wait_for_read);
}

/*Here we want to read in small blocks icap header becouse in most cases
 it will not bigger than 512-1024 bytes.
 So we are going to do small reads and small increments in icap headers size,
 to save some space and keep small the number of over-read bytes
*/
#define ICAP_HEADER_READSIZE 512

/*this function check if there is enough space in buffer buf ....*/
static int icap_header_check_realloc(char **buf, int *size, int used, int mustadded)
{
     char *newbuf;
     int len;
     if (*size - used < mustadded) {
          len = *size + ICAP_HEADER_READSIZE;
          newbuf = realloc(*buf, len);
          if (!newbuf) {
               return EC_500;
          }
          *buf = newbuf;
          *size = *size + ICAP_HEADER_READSIZE;
     }
     return EC_100;
}


static int ci_read_icap_header(ci_request_t * req, ci_headers_list_t * h, int timeout)
{
     int bytes, request_status = EC_100, i, eoh = 0, startsearch = 0, readed = 0;
     int wait_status = 0;
     char *buf_end;
     int dataPrefetch = 0;

     buf_end = h->buf;
     readed = 0;
     bytes = 0;

     if (PIPELINING && req->pstrblock_read && req->pstrblock_read_len > 0) {
         if ((request_status =
              icap_header_check_realloc(&(h->buf), &(h->bufsize), req->pstrblock_read_len,
                                        ICAP_HEADER_READSIZE)) != EC_100)
             return request_status;
         memmove(h->buf, req->pstrblock_read, req->pstrblock_read_len);
         readed = req->pstrblock_read_len;
         buf_end = h->buf;
         bytes = readed;
         dataPrefetch = 1;
         req->pstrblock_read = NULL;
         req->pstrblock_read_len = 0;
         ci_debug_printf(5, "Get data from previous request read.\n");
     }

     do {

         if (!dataPrefetch) {
             if ((wait_status = wait_for_data(req->connection, timeout, ci_wait_for_read)) < 0)
                 return EC_408;

             bytes = ci_connection_read_nonblock(req->connection, buf_end, ICAP_HEADER_READSIZE);
             if (bytes < 0)
                 return EC_408;

             if (bytes == 0) /*NOP? should retry?*/
                 continue;

             readed += bytes;
             req->bytes_in += bytes;
         } else
             dataPrefetch = 0;

          for (i = startsearch; i < bytes - 3; i++) {   /*search for end of header.... */
               if (strncmp(buf_end + i, "\r\n\r\n", 4) == 0) {
                    buf_end = buf_end + i + 2;
                    eoh = 1;
                    break;
               }
          }
          if (eoh)
               break;

          if ((request_status =
               icap_header_check_realloc(&(h->buf), &(h->bufsize), readed,
                                         ICAP_HEADER_READSIZE)) != EC_100)
               break;
          buf_end = h->buf + readed;
          if (startsearch > -3)
               startsearch = (readed > 3 ? -3 : -readed);       /*Including the last 3 char ellements ....... */
     } while(1);

     h->bufused = buf_end - h->buf;     /* -1 ; */
     req->pstrblock_read = buf_end + 2; /*after the \r\n\r\n. We keep the first \r\n and the other dropped.... */
     req->pstrblock_read_len = readed - h->bufused - 2; /*the 2 of the 4 characters \r\n\r\n and the '\0' character */
     req->request_bytes_in = h->bufused + 2; /*This is include the "\r\n\r\n" sequence*/
     return request_status;
}

static int read_encaps_header(ci_request_t * req, ci_headers_list_t * h, int size)
{
     int bytes = 0, remains, readed = 0;
     char *buf_end = NULL;

     if (!ci_headers_setsize(h, size + (CHECK_FOR_BUGGY_CLIENT != 0 ? 2 : 0)))
          return EC_500;
     buf_end = h->buf;

     if (req->pstrblock_read_len > 0) {
          readed =
              (size > req->pstrblock_read_len ? req->pstrblock_read_len : size);
          memcpy(h->buf, req->pstrblock_read, readed);
          buf_end = h->buf + readed;
          if (size <= req->pstrblock_read_len) {        /*We have readed all this header....... */
               req->pstrblock_read = (req->pstrblock_read) + readed;
               req->pstrblock_read_len = (req->pstrblock_read_len) - readed;
          }
          else {
               req->pstrblock_read = NULL;
               req->pstrblock_read_len = 0;
          }
     }

     remains = size - readed;
     while (remains > 0) {
          if (wait_for_data(req->connection, TIMEOUT, ci_wait_for_read) < 0)
               return CI_ERROR;
          if ((bytes = ci_connection_read_nonblock(req->connection, buf_end, remains)) < 0)
               return CI_ERROR;
          remains -= bytes;
          buf_end += bytes;
          req->bytes_in += bytes;
     }

     h->bufused = buf_end - h->buf;     // -1 ;
     if (strncmp(buf_end - 4, "\r\n\r\n", 4) == 0) {
          h->bufused -= 2;      /*eat the last 2 bytes of "\r\n\r\n" */
     } else if(CHECK_FOR_BUGGY_CLIENT && strncmp(buf_end - 2, "\r\n", 2) != 0) {
         // Some icap clients missing the "\r\n\r\n" after end of headers
         // when null-body is present.
         *buf_end = '\r';
         *(buf_end + 1) = '\n';
         h->bufused += 2;
     }
     /*Currently we are counting only successfull http headers read.*/
     req->http_bytes_in += size;
     req->request_bytes_in += size;
     return EC_100;
}

static int get_method(char *buf, char **end)
{
     if (!strncmp(buf, "OPTIONS", 7)) {
          *end = buf + 7;
          return ICAP_OPTIONS;
     }
     else if (!strncmp(buf, "REQMOD", 6)) {
          *end = buf + 6;
          return ICAP_REQMOD;
     }
     else if (!strncmp(buf, "RESPMOD", 7)) {
          *end = buf + 7;
          return ICAP_RESPMOD;
     }
     else {
          *end = buf;
          return -1;
     }
}

static int parse_request(ci_request_t * req, char *buf)
{
     char *start, *end;
     int servnamelen, len, args_len;
     int vmajor, vminor;
     ci_service_module_t *service = NULL;
     service_alias_t *salias = NULL;

     if ((req->type = get_method(buf, &end)) < 0)
         return EC_400;

     while (*end == ' ') end++;
     start = end;

     if (strncasecmp(start, "icap://", 7) == 0)
         start = start + 7;
     else if (strncasecmp(start, "icaps://", 8) == 0)
         start = start + 8;
     else
         return EC_400;

     len = strcspn(start, "/ ");
     end = start + len;
     servnamelen =
         (CI_MAXHOSTNAMELEN > len ? len : CI_MAXHOSTNAMELEN);
     memcpy(req->req_server, start, servnamelen);
     req->req_server[servnamelen] = '\0';
     if (*end == '/') { /*we are expecting service name*/
         start = ++end;
         while (*end && *end != ' ' && *end != '?')
             end++;
         len = end - start;

         len =
             (len < MAX_SERVICE_NAME ? len : MAX_SERVICE_NAME);
         if (len) {
             strncpy(req->service, start, len);
             req->service[len] = '\0';
         }

         if (*end == '?') {     /*args */
             start = ++end;
             if ((end = strchr(start, ' ')) != NULL) {
                 args_len = strlen(req->args);
                 len = end - start;
                 if (args_len && len) {
                     req->args[args_len] = '&';
                     args_len++;
                 }
                 len = (len < (MAX_SERVICE_ARGS - args_len) ?
                        len : (MAX_SERVICE_ARGS - args_len));
                 strncpy(req->args + args_len, start, len);
                 req->args[args_len + len] = '\0';
             }
         }      /*end of parsing args */
     }

     while (*end == ' ')
         end++;
     start = end;

     vminor = vmajor = -1;
     if (strncmp(start, "ICAP/", 5) == 0) {
         start += 5;
         vmajor = strtol(start, &end, 10);
         if (vmajor > 0 && *end == '.') {
             start = end + 1;
             vminor = strtol(start, &end, 10);
             if (end == start) /*no chars parsed*/
                 vminor = -1;
         }
     }

     if (vminor == -1 || vmajor < 1)
         return EC_400;

     if (req->service[0] == '\0' && DEFAULT_SERVICE) { /*No service name defined*/
         strncpy(req->service, DEFAULT_SERVICE, MAX_SERVICE_NAME);
     }

     if (req->service[0] != '\0') {
         if (!(service = find_service(req->service))) { /*else search for an alias */
             if ((salias = find_service_alias(req->service))) {
                 service = salias->service;
                 if (salias->args[0] != '\0')
                     strcpy(req->args, salias->args);
             }
         }
     }
     req->current_service_mod = service;

     if (!req->current_service_mod)
         return EC_404; /*Service not found*/

     if (!ci_method_support
         (req->current_service_mod->mod_type, req->type)
         && req->type != ICAP_OPTIONS) {
         return EC_405;    /* Method not allowed for service. */
     }

     return EC_100;
}

static int check_request(ci_request_t *req)
{
    /*Check encapsulated header*/
    if (req->entities[0] == NULL && !req->type == ICAP_OPTIONS) /*No encapsulated header*/
        return EC_400;

    ci_debug_printf(6, "\n type:%d Entities: %d %d %d %d \n",
                    req->type,
                    req->entities[0] ? req->entities[0]->type : -1,
                    req->entities[1] ? req->entities[1]->type : -1,
                    req->entities[2] ? req->entities[2]->type : -1,
                    req->entities[3] ? req->entities[3]->type : -1
        );
    if (req->type == ICAP_REQMOD) {
        if (req->entities[2] != NULL)
            return EC_400;
        else if (req->entities[1] != NULL) {
            if (req->entities[0]->type != ICAP_REQ_HDR)
                return EC_400;
            if (req->entities[1]->type != ICAP_REQ_BODY && req->entities[1]->type != ICAP_NULL_BODY)
                return EC_400;
        } else {
            /*If it has only one encapsulated object it must be body data*/
            if (req->entities[0]->type != ICAP_REQ_BODY)
                return EC_400;

        }
    } else if (req->type == ICAP_RESPMOD) {
        if (req->entities[3] != NULL)
            return EC_400;
        else if (req->entities[2] != NULL) {
            if (req->entities[0]->type != ICAP_REQ_HDR)
                return EC_400;
            if (req->entities[1]->type != ICAP_RES_HDR)
                return EC_400;
            if (req->entities[2]->type != ICAP_RES_BODY && req->entities[2]->type != ICAP_NULL_BODY)
                return EC_400;
        } else if (req->entities[1] != NULL) {
            if (req->entities[0]->type != ICAP_RES_HDR && req->entities[0]->type != ICAP_REQ_HDR)
                return EC_400;
            if (req->entities[1]->type != ICAP_RES_BODY && req->entities[1]->type != ICAP_NULL_BODY)
                return EC_400;
        } else {
            /*If it has only one encapsulated object it must be body data*/
            if (req->entities[0]->type != ICAP_RES_BODY)
                return EC_400;
        }
    }
    return EC_100;
}

static int parse_header(ci_request_t * req)
{
     int i, request_status = EC_100, result;
     ci_headers_list_t *h;
     char *val;

     h = req->request_header;
     if ((request_status = ci_read_icap_header(req, h, TIMEOUT)) != EC_100)
          return request_status;

     if ((request_status = ci_headers_unpack(h)) != EC_100)
         return request_status;

     if ((request_status = parse_request(req, h->headers[0])) != EC_100)
         return request_status;

     for (i = 1; i < h->used && request_status == EC_100; i++) {
          if (strncasecmp("Preview:", h->headers[i], 8) == 0) {
               val = h->headers[i] + 8;
               for (;isspace(*val) && *val != '\0'; ++val);
               errno = 0;
               result = strtol(val, NULL, 10);
               if (errno != EINVAL && errno != ERANGE) {
                    req->preview = result;
                    if (result >= 0)
                         ci_buf_reset_size(&(req->preview_data), result + 64);
               }
          }
          else if (strncasecmp("Encapsulated:", h->headers[i], 13) == 0)
               request_status = process_encapsulated(req, h->headers[i]);
          else if (strncasecmp("Connection:", h->headers[i], 11) == 0) {
               val = h->headers[i] + 11;
               for (;isspace(*val) && *val != '\0'; ++val);
/*             if(strncasecmp(val,"keep-alive",10)==0)*/
               if (strncasecmp(val, "close", 5) == 0)
                    req->keepalive = 0;
               /*else the default behaviour of keepalive ..... */
          }
          else if (strncasecmp("Allow:", h->headers[i], 6) == 0) {
	       if (strstr(h->headers[i]+6, "204"))
		    req->allow204 = 1;
	       if (strstr(h->headers[i]+6, "206"))
		    req->allow206 = 1;
          }
     }

     if (request_status != EC_100)
         return request_status;

     return check_request(req);
}


static int parse_encaps_headers(ci_request_t * req)
{
     int size, i, request_status = 0;
     ci_encaps_entity_t *e = NULL;
     for (i = 0; (e = req->entities[i]) != NULL; i++) {
          if (e->type > ICAP_RES_HDR)   //res_body,req_body or opt_body so the end of the headers.....process_encapsulated
               return EC_100;

          if (req->entities[i + 1] == NULL)
               return EC_400;

          size = req->entities[i + 1]->start - e->start;

          if ((request_status =
               read_encaps_header(req, (ci_headers_list_t *) e->entity,
                                  size)) != EC_100)
               return request_status;

          if ((request_status =
               ci_headers_unpack((ci_headers_list_t *) e->entity)) != EC_100)
               return request_status;
     }
     return EC_100;
}

/*
  In read_preview_data I must check if readed data are more than 
  those client said in preview header
*/
static int read_preview_data(ci_request_t * req)
{
     int ret;
     char *wdata;

     req->current_chunk_len = 0;
     req->chunk_bytes_read = 0;
     req->write_to_module_pending = 0;

     if (req->pstrblock_read_len == 0) {
         if(wait_for_data(req->connection, TIMEOUT, ci_wait_for_read) < 0)
             return CI_ERROR;
             
          if (net_data_read(req) == CI_ERROR)
               return CI_ERROR;
     }

     do {
          do {
               if ((ret = parse_chunk_data(req, &wdata)) == CI_ERROR) {
                    ci_debug_printf(1,
                                    "Error parsing chunks, current chunk len: %d readed:%d, str:%s\n",
                                    req->current_chunk_len,
                                    req->chunk_bytes_read, req->pstrblock_read);
                    return CI_ERROR;
               }
               if (ci_buf_write
                   (&(req->preview_data), wdata,
                    req->write_to_module_pending) < 0)
                    return CI_ERROR;
               req->write_to_module_pending = 0;

               if (ret == CI_EOF) {
                    req->pstrblock_read = NULL;
                    req->pstrblock_read_len = 0;
                    if (req->eof_received)
                         return CI_EOF;
                    return CI_OK;
               }
          } while (ret != CI_NEEDS_MORE);

          if (wait_for_data(req->connection, TIMEOUT, ci_wait_for_read) < 0)
               return CI_ERROR;
          if (net_data_read(req) == CI_ERROR)
               return CI_ERROR;
     } while (1);

     return CI_ERROR;
}

static void ec_responce_simple(ci_request_t * req, int ec)
{
     char buf[256];
     int len;
     snprintf(buf, 256, "ICAP/1.0 %d %s\r\n\r\n",
              ci_error_code(ec), ci_error_code_string(ec));
     buf[255] = '\0';
     len = strlen(buf);
     ci_connection_write(req->connection, buf, len, TIMEOUT);
     req->bytes_out += len;
     req->return_code = ec;
}

static int ec_responce(ci_request_t * req, int ec)
{
     char buf[256];
     ci_service_xdata_t *srv_xdata = NULL;
     int len, allow204to200OK = 0;
     if (req->current_service_mod)
         srv_xdata = service_data(req->current_service_mod);
     ci_headers_reset(req->response_header);

     if (ec == EC_204 && ALLOW204_AS_200OK_ZERO_ENCAPS) {
         allow204to200OK = 1;
         ec = EC_200;
     }
     snprintf(buf, 256, "ICAP/1.0 %d %s",
              ci_error_code(ec), ci_error_code_string(ec));
     ci_headers_add(req->response_header, buf);
     ci_headers_add(req->response_header, "Server: C-ICAP/" VERSION);
     if (req->keepalive)
          ci_headers_add(req->response_header, "Connection: keep-alive");
     else
          ci_headers_add(req->response_header, "Connection: close");

     if (srv_xdata) {
         ci_service_data_read_lock(srv_xdata);
         ci_headers_add(req->response_header, srv_xdata->ISTag);
         ci_service_data_read_unlock(srv_xdata);
     }
     if (!ci_headers_is_empty(req->xheaders)) {
	  ci_headers_addheaders(req->response_header, req->xheaders);
     }
     if (allow204to200OK) {
         if (req->type == ICAP_REQMOD)
             ci_headers_add(req->response_header, "Encapsulated: req-hdr=0, null-body=0");
         else
             ci_headers_add(req->response_header, "Encapsulated: res-hdr=0, null-body=0");
     }
     /*
       TODO: Release req->entities (ci_request_release_entity())
      */
     ci_headers_pack(req->response_header);
     req->return_code = ec;

     len = ci_connection_write(req->connection, 
		    req->response_header->buf, req->response_header->bufused, 
		    TIMEOUT);

     /*We are finishing sending*/
     req->status = SEND_EOF;

     if (len < 0)
	 return -1;

     req->bytes_out += len;
     return len;
}

extern char MY_HOSTNAME[];
static int mk_responce_header(ci_request_t * req)
{
     ci_headers_list_t *head;
     ci_encaps_entity_t **e_list;
     ci_service_xdata_t *srv_xdata;
     char buf[512];
     srv_xdata = service_data(req->current_service_mod);
     ci_headers_reset(req->response_header);
     head = req->response_header;
     assert(req->return_code >= EC_100 && req->return_code < EC_MAX);
     snprintf(buf, 512, "ICAP/1.0 %d %s",
              ci_error_code(req->return_code), ci_error_code_string(req->return_code));
     ci_headers_add(head, buf);
     ci_headers_add(head, "Server: C-ICAP/" VERSION);
     if (req->keepalive)
          ci_headers_add(head, "Connection: keep-alive");
     else
          ci_headers_add(head, "Connection: close");
     ci_service_data_read_lock(srv_xdata);
     ci_headers_add(head, srv_xdata->ISTag);
     ci_service_data_read_unlock(srv_xdata);
     if (!ci_headers_is_empty(req->xheaders)) {
          ci_headers_addheaders(head, req->xheaders);
     }

     e_list = req->entities;
     if (req->type == ICAP_RESPMOD) {
          if (e_list[0]->type == ICAP_REQ_HDR) {
               ci_request_release_entity(req, 0);
               e_list[0] = e_list[1];
               e_list[1] = e_list[2];
               e_list[2] = NULL;
          }
     }

     snprintf(buf, 512, "Via: ICAP/1.0 %s (C-ICAP/" VERSION " %s )",
              MY_HOSTNAME,
              (req->current_service_mod->mod_short_descr ? req->
               current_service_mod->mod_short_descr : req->current_service_mod->
               mod_name));
     buf[511] = '\0';
     /*Here we must append it to an existsing Via header not just add a new header */
     if (req->type == ICAP_RESPMOD) {
          ci_http_response_add_header(req, buf);
     }
     else if (req->type == ICAP_REQMOD) {
          ci_http_request_add_header(req, buf);
     }

     ci_response_pack(req);
     return 1;
}


/****************************************************************/
/* New  functions to send responce */

const char *eol_str = "\r\n";
const char *eof_str = "0\r\n\r\n";


static int send_current_block_data(ci_request_t * req)
{
     int bytes;
     if (req->remain_send_block_bytes == 0)
          return 0;
     if ((bytes =
          ci_connection_write_nonblock(req->connection, req->pstrblock_responce,
                                       req->remain_send_block_bytes)) < 0) {
         ci_debug_printf(5, "Error writing to socket (errno:%d, bytes:%d. string:\"%s\")", errno, req->remain_send_block_bytes, req->pstrblock_responce);
          return CI_ERROR;
     }

/*
     if (bytes == 0) {
         ci_debug_printf(5, "Can not write to the client. Is the connection closed?");
         return CI_ERROR;
     }
*/

     req->pstrblock_responce += bytes;
     req->remain_send_block_bytes -= bytes;
     req->bytes_out += bytes;
     if(req->status >= SEND_HEAD1 &&  req->status <= SEND_HEAD3)
       req->http_bytes_out +=bytes;
     return req->remain_send_block_bytes;
}


static int format_body_chunk(ci_request_t * req)
{
     int def_bytes;
     char *wbuf = NULL;
     char tmpbuf[EXTRA_CHUNK_SIZE];

     if (!req->responce_hasbody)
          return CI_EOF;
     if (req->remain_send_block_bytes > 0) {
          assert(req->remain_send_block_bytes <= MAX_CHUNK_SIZE);

	  /*The data are not written yet but I hope there is not any problem.
	    It is difficult to compute data sent */
	  req->http_bytes_out += req->remain_send_block_bytes;
	  req->body_bytes_out += req->remain_send_block_bytes;

          wbuf = req->wbuf + EXTRA_CHUNK_SIZE + req->remain_send_block_bytes;
          /*Put the "\r\n" sequence at the end of chunk */
          *(wbuf++) = '\r';
          *wbuf = '\n';
          def_bytes =
              snprintf(tmpbuf, EXTRA_CHUNK_SIZE, "%x\r\n",
                       req->remain_send_block_bytes);
          wbuf = req->wbuf + EXTRA_CHUNK_SIZE - def_bytes;      /*Copy the chunk define in the beggining of chunk ..... */
          memcpy(wbuf, tmpbuf, def_bytes);
          req->pstrblock_responce = wbuf;
          req->remain_send_block_bytes += def_bytes + 2;
     }
     else if (req->remain_send_block_bytes == CI_EOF) {
	  if (req->return_code == EC_206 && req->i206_use_original_body >= 0) {
               def_bytes = sprintf(req->wbuf, "0; use-original-body=%ld\r\n\r\n", 
				    req->i206_use_original_body );
	       req->pstrblock_responce = req->wbuf;
	       req->remain_send_block_bytes = def_bytes;
	  }
          else {
	       strcpy(req->wbuf, "0\r\n\r\n");
	       req->pstrblock_responce = req->wbuf;
	       req->remain_send_block_bytes = 5;
	  }
          return CI_EOF;
     }
     return CI_OK;
}



static int resp_check_body(ci_request_t * req)
{
     int i;
     ci_encaps_entity_t **e = req->entities;
     for (i = 0; e[i] != NULL; i++)
          if (e[i]->type == ICAP_NULL_BODY)
               return 0;
     return 1;
}

/*
The 
if((ret=send_current_block_data(req))!=0)
  return ret;

must called after this function....
*/

static int update_send_status(ci_request_t * req)
{
     int i, status;
     ci_encaps_entity_t *e;

     if (req->status == SEND_NOTHING) { //If nothing has send start sending....
          if (!mk_responce_header(req)) {
               ci_debug_printf(1, "Error constructing the responce headers!\n");
               return CI_ERROR;
          }
          req->responce_hasbody = resp_check_body(req);

          req->pstrblock_responce = req->response_header->buf;
          req->remain_send_block_bytes = req->response_header->bufused;
          req->status = SEND_RESPHEAD;
	  ci_debug_printf(9, "Going to send response headers\n");
          return CI_OK;
     }

     if (req->status == SEND_EOF) {
	 ci_debug_printf(9, "The req->status is EOF (remain to send bytes:%d)\n", 
			 req->remain_send_block_bytes);
          if (req->remain_send_block_bytes == 0)
               return CI_EOF;
          else
               return CI_OK;
     }
     if (req->status == SEND_BODY) {
	  ci_debug_printf(9, "Send status is SEND_BODY return\n");
          return CI_OK;
     }

     if ((status = req->status) < SEND_HEAD3) {
          status++;
     }

     if (status > SEND_RESPHEAD && status < SEND_BODY) {        /*status is SEND_HEAD1 SEND_HEAD2 or SEND_HEAD3    */
          i = status - SEND_HEAD1;      /*We have to send next headers block .... */
          if ((e = req->entities[i]) != NULL
              && (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR)) {

               req->pstrblock_responce = ((ci_headers_list_t *) e->entity)->buf;
               req->remain_send_block_bytes =
                   ((ci_headers_list_t *) e->entity)->bufused;

               req->status = status;
	       ci_debug_printf(9, "Going to send http headers on entity :%d\n", i);
               return CI_OK;
          }
          else if (req->responce_hasbody) {     /*end of headers, going to send body now.A body always follows the res_hdr or req_hdr..... */
               req->status = SEND_BODY;
               return CI_OK;
          }
          else {
               req->status = SEND_EOF;
               req->pstrblock_responce = (char *) NULL;
               req->remain_send_block_bytes = 0;
               return CI_EOF;
          }
     }

     return CI_ERROR;           /*Can not be reached (I thing)...... */
}

static int mod_null_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                ci_request_t *req)
{
     if (iseof)
          *rlen = CI_EOF;
     else
          *rlen = 0;
     return CI_OK;
}

static int mod_echo_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
                       ci_request_t *req)
{
     if (!req->echo_body)
         return CI_ERROR;

     if (rlen && rbuf) {
         *rlen = ci_ring_buf_write(req->echo_body, rbuf, *rlen);
         if (*rlen < 0)
            return CI_ERROR;
     }

     if (wbuf && wlen) {
          *wlen = ci_ring_buf_read(req->echo_body, wbuf, *wlen);
          if(*wlen == 0 && req->eof_received)
              *wlen = CI_EOF;
     }

     return CI_OK;
}

static int get_send_body(ci_request_t * req, int parse_only)
{
     char *wchunkdata = NULL, *rchunkdata = NULL;
     int ret, parse_chunk_ret, has_formated_data = 0;
     int (*service_io) (char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                        ci_request_t *);
     int action = 0, rchunkisfull = 0, service_eof = 0, wbytes, rbytes;
     int lock_status;
     int no_io;

     if (parse_only)
          service_io = mod_null_io;
     else if (req->echo_body)
          service_io = mod_echo_io;
     else
          service_io = req->current_service_mod->mod_service_io;
     if (!service_io)
          return CI_ERROR;

     req->status = SEND_NOTHING;
     /*in the case we did not have preview data and body is small maybe
        the c-icap already read the body with the headers so do not read
        if there are unparsed bytes in pstrblock buffer
      */
     if (req->pstrblock_read_len == 0)
          action = ci_wait_for_read;
     do {
          ret = 0;
          if (action) {
               ci_debug_printf(9, "Going to %s/%s data\n",
                               (action & ci_wait_for_read ? "Read" : "-"),
                               (action & ci_wait_for_write ? "Write" : "-")
                   );
               if ((ret =
                    wait_for_data(req->connection, TIMEOUT,
                                     action)) < 0)
                    break;
               if (ret & ci_wait_for_read) {
                    if (net_data_read(req) == CI_ERROR)
                         return CI_ERROR;
               }
               if (ret & ci_wait_for_write) {
                    if (!req->data_locked && req->status == SEND_NOTHING) {
                         update_send_status(req);
                    }
                    if (send_current_block_data(req) == CI_ERROR)
                         return CI_ERROR;
               }
               ci_debug_printf(9,
                               "OK done reading/writing going to process\n");
          }

          if (!req->data_locked && req->remain_send_block_bytes == 0) {
               if (update_send_status(req) == CI_ERROR)
                    return CI_ERROR;
               // if(update_send_status==CI_EOF)/*earlier responce from icap server???...*/
          }

          /*Store lock status. If it is changed during module io, we need
            to update send status.*/
          lock_status = req->data_locked;

          /*In the following loop, parses the chunks from readed data
             and try to write data to the service.
             At the same time reads the data from module and try to fill
             the req->wbuf
           */
          if (req->remain_send_block_bytes)
               has_formated_data = 1;
          else
               has_formated_data = 0;
          parse_chunk_ret = 0;
          do {
               if (req->pstrblock_read_len != 0
                   && req->write_to_module_pending == 0) {
                    if ((parse_chunk_ret =
                         parse_chunk_data(req, &wchunkdata)) == CI_ERROR) {
                         ci_debug_printf(1, "Error parsing chunks!\n");
                         return CI_ERROR;
                    }

                    if (parse_chunk_ret == CI_EOF)
                         req->eof_received = 1;
               }
               if (wchunkdata && req->write_to_module_pending)
                    wbytes = req->write_to_module_pending;
               else
                    wbytes = 0;

               if (req->status == SEND_BODY && !service_eof) {
                    if (req->remain_send_block_bytes == 0) {
                         /*Leave space for chunk spec.. */
                         rchunkdata = req->wbuf + EXTRA_CHUNK_SIZE;
                         req->pstrblock_responce = rchunkdata;  /*does not needed! */
                         rchunkisfull = 0;
                    }
                    if ((MAX_CHUNK_SIZE - req->remain_send_block_bytes) > 0
                        && has_formated_data == 0) {
                         rbytes = MAX_CHUNK_SIZE - req->remain_send_block_bytes;
                    }
                    else {
                         rchunkisfull = 1;
                         rbytes = 0;
                    }
               }
               else
                    rbytes = 0;

               no_io = 0;
	       ci_debug_printf(9, "get send body: going to write/read: %d/%d bytes\n", wbytes, rbytes);
               if ((*service_io)
                   (rchunkdata, &rbytes, wchunkdata, &wbytes, req->eof_received,
                    req) == CI_ERROR)
                    return CI_ERROR;
	       ci_debug_printf(9, "get send body: written/read: %d/%d bytes (eof: %d)\n", wbytes, rbytes, req->eof_received);
               no_io = (rbytes==0 && wbytes==0);
               if (wbytes) {
                    wchunkdata += wbytes;
                    req->write_to_module_pending -= wbytes;
               }
               if (rbytes > 0) {
                    rchunkdata += rbytes;
                    req->remain_send_block_bytes += rbytes;
               }
               else if (rbytes == CI_EOF)
                    service_eof = 1;
          } while (no_io == 0 && req->pstrblock_read_len != 0
                   && parse_chunk_ret != CI_NEEDS_MORE && parse_chunk_ret != CI_EOF && !rchunkisfull);

          action = 0;
          if (!req->write_to_module_pending) {
               action = ci_wait_for_read;
               wchunkdata = NULL;
          }

          if (req->status == SEND_BODY) {
               if (req->remain_send_block_bytes == 0 && service_eof == 1)
                    req->remain_send_block_bytes = CI_EOF;
               if (has_formated_data == 0) {
                    if (format_body_chunk(req) == CI_EOF)
                         req->status = SEND_EOF;
               }
          }

          if (req->remain_send_block_bytes) {
               action = action | ci_wait_for_write;
          }
	  
     } while ((!req->eof_received || (req->eof_received && req->write_to_module_pending)) && (action || lock_status != req->data_locked));

     if (req->eof_received)
          return CI_OK;

     if (!action) {
          ci_debug_printf(1,
                          "Bug in the service. Please report to the service author!!!!\n");
     }
     else {
          ci_debug_printf(5, "Error reading from network......\n");
     }
     return CI_ERROR;
}


/*Return CI_ERROR on error or CI_OK on success*/
static int send_remaining_response(ci_request_t * req)
{
     int ret = 0;
     int (*service_io) (char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                        ci_request_t *);
     if (req->echo_body)
          service_io = mod_echo_io;
     else
          service_io = req->current_service_mod->mod_service_io;

     if (!service_io)
          return CI_ERROR;


     if (req->status == SEND_EOF && req->remain_send_block_bytes == 0) {
          ci_debug_printf(5, "OK sending all data\n");
          return CI_OK;
     }
     do {
          while (req->remain_send_block_bytes > 0) {
               if ((ret =
                    wait_for_data(req->connection, TIMEOUT,
                                     ci_wait_for_write)) < 0) {
                    ci_debug_printf(3,
                                    "Timeout sending data. Ending .......\n");
                    return CI_ERROR;
               }
               if (send_current_block_data(req) == CI_ERROR)
                    return CI_ERROR;
          }

          if (req->status == SEND_BODY && req->remain_send_block_bytes == 0) {
               req->pstrblock_responce = req->wbuf + EXTRA_CHUNK_SIZE;  /*Leave space for chunk spec.. */
               req->remain_send_block_bytes = MAX_CHUNK_SIZE;
	       ci_debug_printf(9, "rest response: going to read: %d bytes\n", req->remain_send_block_bytes);
               service_io(req->pstrblock_responce,
                          &(req->remain_send_block_bytes), NULL, NULL, 1, req);
	       ci_debug_printf(9, "rest response: read: %d bytes\n", req->remain_send_block_bytes);
               if (req->remain_send_block_bytes == CI_ERROR)    /*CI_EOF of CI_ERROR, stop sending.... */
                    return CI_ERROR;
               if (req->remain_send_block_bytes == 0)
                    break;

               if ((ret = format_body_chunk(req)) == CI_EOF) {
                    req->status = SEND_EOF;
               }
          }

     } while ((ret = update_send_status(req)) >= 0);    /*CI_EOF is < 0 */

     if (ret == CI_ERROR)
          return ret;

     return CI_OK;
}

static void options_responce(ci_request_t * req)
{
     char buf[MAX_HEADER_SIZE + 1];
     const char *str;
     ci_headers_list_t *head;
     ci_service_xdata_t *srv_xdata;
     unsigned int xopts;
     int preview, allow204, allow206, max_conns, xlen;
     int hastransfer = 0;
     int ttl;
     req->return_code = EC_200;
     head = req->response_header;
     srv_xdata = service_data(req->current_service_mod);
     ci_headers_reset(head);
     ci_headers_add(head, "ICAP/1.0 200 OK");
     strcpy(buf, "Methods: ");
     if (ci_method_support(req->current_service_mod->mod_type, ICAP_RESPMOD)) {
          strcat(buf, "RESPMOD");
          if (ci_method_support
              (req->current_service_mod->mod_type, ICAP_REQMOD)) {
               strcat(buf, ", REQMOD");
          }
     }
     else {                     /*At least one method must supported. A check for error must exists here..... */
          strcat(buf, "REQMOD");
     }

     ci_headers_add(head, buf);
     snprintf(buf, MAX_HEADER_SIZE, "Service: C-ICAP/" VERSION " server - %s",
              ((str =
                req->current_service_mod->mod_short_descr) ? str : req->
               current_service_mod->mod_name));
     buf[MAX_HEADER_SIZE] = '\0';
     ci_headers_add(head, buf);

     ci_service_data_read_lock(srv_xdata);
     ci_headers_add(head, srv_xdata->ISTag);
     if (srv_xdata->TransferPreview[0] != '\0' && srv_xdata->preview_size >= 0) {
          ci_headers_add(head, srv_xdata->TransferPreview);
          hastransfer++;
     }
     if (srv_xdata->TransferIgnore[0] != '\0') {
          ci_headers_add(head, srv_xdata->TransferIgnore);
          hastransfer++;
     }
     if (srv_xdata->TransferComplete[0] != '\0') {
          ci_headers_add(head, srv_xdata->TransferComplete);
          hastransfer++;
     }
     /*If none of the Transfer-* headers configured but preview configured  send all requests*/
     if (!hastransfer && srv_xdata->preview_size >= 0)
         ci_headers_add(head, "Transfer-Preview: *");
     /*Get service options before close the lock.... */
     xopts = srv_xdata->xopts;
     preview = srv_xdata->preview_size;
     allow204 = srv_xdata->allow_204;
     allow206 = srv_xdata->allow_206;
     max_conns = srv_xdata->max_connections;
     ttl = srv_xdata->options_ttl;
     ci_service_data_read_unlock(srv_xdata);

     ci_debug_printf(5, "Options responce:\n"
		     " Preview :%d\n"
		     " Allow 204:%s\n"
		     " Allow 206:%s\n"
		     " TransferPreview:\"%s\"\n"
		     " TransferIgnore:%s\n"
		     " TransferComplete:%s\n"
		     " Max-Connections:%d\n",
		     preview,(allow204?"yes":"no"),
		     (allow206?"yes":"no"),
		     srv_xdata->TransferPreview,
		     srv_xdata->TransferIgnore,
		     srv_xdata->TransferComplete,
		     max_conns
	             );

     /* ci_headers_add(head, "Max-Connections: 20"); */
     if (ttl > 0) {
         sprintf(buf, "Options-TTL: %d", ttl);
         ci_headers_add(head, buf);
     }
     else
         ci_headers_add(head, "Options-TTL: 3600");
     strcpy(buf, "Date: ");
     ci_strtime_rfc822(buf + strlen(buf));
     ci_headers_add(head, buf);
     if (preview >= 0) {
          sprintf(buf, "Preview: %d", srv_xdata->preview_size);
          ci_headers_add(head, buf);
     }
     if(max_conns >= 0) {
	 sprintf(buf, "Max-Connections: %d", max_conns);
	 ci_headers_add(head, buf);
     }
     if (allow204 && allow206) {
          ci_headers_add(head, "Allow: 204, 206");
     }
     else if (allow204) {
	  ci_headers_add(head, "Allow: 204");
     }
     if (xopts) {
          strcpy(buf, "X-Include: ");
          xlen = 11;            /*sizeof("X-Include: ") */
          if ((xopts & CI_XCLIENTIP)) {
               strcat(buf, "X-Client-IP");
               xlen += sizeof("X-Client-IP");
          }
          if ((xopts & CI_XSERVERIP)) {
               if (xlen > 11) {
                    strcat(buf, ", ");
                    xlen += 2;
               }
               strcat(buf, "X-Server-IP");
               xlen += sizeof("X-Server-IP");
          }
          if ((xopts & CI_XSUBSCRIBERID)) {
               if (xlen > 11) {
                    strcat(buf, ", ");
                    xlen += 2;
               }
               strcat(buf, "X-Subscriber-ID");
               xlen += sizeof("X-Subscriber-ID");
          }
          if ((xopts & CI_XAUTHENTICATEDUSER)) {
               if (xlen > 11) {
                    strcat(buf, ", ");
                    xlen += 2;
               }
               strcat(buf, "X-Authenticated-User");
               xlen += sizeof("X-Authenticated-User");
          }
          if ((xopts & CI_XAUTHENTICATEDGROUPS)) {
               if (xlen > 11) {
                    strcat(buf, ", ");
                    xlen += 2;
               }
               strcat(buf, "X-Authenticated-Groups");
               xlen += sizeof("X-Authenticated-Groups");
          }
          if (xlen > 11)
               ci_headers_add(head, buf);
     }
     ci_response_pack(req);

     req->pstrblock_responce = head->buf;
     req->remain_send_block_bytes = head->bufused;

     do {
          if ((wait_for_data(req->connection, TIMEOUT, ci_wait_for_write))
              < 0) {
               ci_debug_printf(3, "Timeout sending data. Ending .......\n");
               return;
          }
          if (send_current_block_data(req) == CI_ERROR) {
               ci_debug_printf(3, "Error sending data. Ending .....\n");
               return;
          }
     } while (req->remain_send_block_bytes > 0);

//     if(responce_body)
//        send_body_responce(req,responce_body);

}

/*Read preview data, call preview handler and respond with error,  "204" or 
  "100 Continue" if required.
  Returns:
  - CI_OK on success and 100 Continue,
  - CI_EOF on ieof chunk response (means all body data received,
     inside preview, no need to read more data from the client)
  - CI_ERROR on error
*/
static int do_request_preview(ci_request_t *req){
    int preview_read_status;
    int res;

    ci_debug_printf(8,"Read preview data if there are and process request\n");

    /*read_preview_data returns CI_OK, CI_EOF or CI_ERROR */
    if (!req->hasbody)
        preview_read_status = CI_EOF;
    else if ((preview_read_status = read_preview_data(req)) == CI_ERROR) {
        ci_debug_printf(5,
                        "An error occured while reading preview data (propably timeout)\n");
        req->keepalive = 0;
        ec_responce(req, EC_408);
        return CI_ERROR;
    }
    
    if (!req->current_service_mod->mod_check_preview_handler) {
        /*We have not a preview data handler. We are responding with "100 Continue"
          assuming that the service needs to process all data.
          The preview data are stored in req->preview_data.buf, if the service needs them.
         */
        ci_debug_printf(3, "Preview request but no preview data handler. Respond with \"100 Continue\"\n");
        res =  CI_MOD_CONTINUE;
    }
    else {
        /*We have a preview handler and we are going to call it*/
        res = req->current_service_mod->mod_check_preview_handler(
            req->preview_data.buf, req->preview_data.used, req);
    }

    if (res == CI_MOD_ALLOW204) {
        if (ec_responce(req, EC_204) < 0) {
            req->keepalive = 0; /*close the connection*/
            return CI_ERROR;
        }

        ci_debug_printf(5,"Preview handler return allow 204 response\n");
        /*we are finishing here*/
        return CI_OK;
    }

    if (res == CI_MOD_ALLOW206 && req->allow206) {
        req->return_code = EC_206;
        ci_debug_printf(5,"Preview handler return 206 response\n");
        return CI_OK;
    }

    /*The CI_MOD_CONTINUE is the only remaining valid answer */
    if (res != CI_MOD_CONTINUE) {        
        ci_debug_printf(5, "An error occured in preview handler!"
                        " return code: %d , req->allow204=%d, req->allow206=%d\n",
                        res, req->allow204, req->allow206);
        req->keepalive = 0;
        ec_responce(req, EC_500);
        return CI_ERROR;
    }

    if (preview_read_status != CI_EOF)  {
        ec_responce_simple(req, EC_100);     /*if 100 Continue and not "0;ieof"*/
    }
    /* else 100 Continue and "0;ieof" received. Do not send "100 Continue"*/

    ci_debug_printf(5,"Preview handler %s\n", 
                    (preview_read_status == CI_EOF ? "receives all body data" : "continue reading more body data"));

    return preview_read_status;
}

/* 
   Call the preview handler in the case there is not preview request.
   
*/
static int do_fake_preview(ci_request_t * req)
{
    int res;
    /*We are outside preview. The preview handler will be called but it needs
      special handle.
      Currently the preview data handler called with no preview data.In the future we 
      should add code to read data from client and pass them to the service.
      Also in the future the service should not need to know if preview supported
      by the client or not
    */

    if (!req->current_service_mod->mod_check_preview_handler) {
        req->return_code = req->hasbody ? EC_100 : EC_200;
        return CI_OK; /*do nothing*/
    }
    
    ci_debug_printf(8,"Preview does not supported. Call the preview handler with no preview data.\n");
    res = req->current_service_mod->mod_check_preview_handler(NULL, 0, req);

    /*We are outside preview. The client should support allow204 outside preview
      to support it.
     */
    if (res == CI_MOD_ALLOW204 && req->allow204) {
        ci_debug_printf(5,"Preview handler return allow 204 response, and allow204 outside preview supported\n");
        if (ec_responce(req, EC_204) < 0) {
            req->keepalive = 0; /*close the connection*/
            return CI_ERROR;
        }

        /*And now parse body data we have read and data the client going to send us,
          but do not pass them to the service (second argument of the get_send_body)*/
        if (req->hasbody) {
            res = get_send_body(req, 1);
            if (res == CI_ERROR)
                return res;
        }
        req->return_code = EC_204;
        return CI_OK;
    }

    if (res == CI_MOD_ALLOW204) {
        if (req->hasbody) {
            ci_debug_printf(5,"Preview handler return allow 204 response, allow204 outside preview does NOT supported, and body data\n");
            if (FAKE_ALLOW204) {
                ci_debug_printf(5,"Fake allow204 supported, echo data back\n");
                req->echo_body = ci_ring_buf_new(32768);
                req->return_code = EC_100;
                return CI_OK;
            }
        } else {        
            ci_debug_printf(5,"Preview handler return allow 204 response, allow204 outside preview does NOT supported, but no body data\n");
            /*Just copy http headers to icap response*/
            req->return_code = EC_200;
            return CI_OK;
        }
    }

    if (res == CI_MOD_ALLOW206 && req->allow204 && req->allow206) {
        ci_debug_printf(5,"Preview handler return allow 204 response, allow204 outside preview and allow206 supported by t");
        req->return_code = EC_206;
        return CI_OK;
    }

    if (res == CI_MOD_CONTINUE) {
        req->return_code = req->hasbody ? EC_100 : EC_200;
        return CI_OK;
    }

    ci_debug_printf(1, "An error occured in preview handler (outside preview)!"
                    " return code: %d, req->allow204=%d, req->allow206=%d\n", 
                    res, req->allow204, req->allow206);
    req->keepalive = 0;
    ec_responce(req, EC_500);
    return CI_ERROR;
}

/*
  Return CI_ERROR or CI_OK
*/
static int do_end_of_data(ci_request_t * req) {
    int res;

    if (!req->current_service_mod->mod_end_of_data_handler)
        return CI_OK; /*Nothing to do*/

    res = req->current_service_mod->mod_end_of_data_handler(req);
/* 
     while( req->current_service_mod->mod_end_of_data_handler(req)== CI_MOD_NOT_READY){
     //can send some data here .........
     }
*/
    if (res == CI_MOD_ALLOW204 && req->allow204 && !ci_req_sent_data(req)) {
        if (ec_responce(req, EC_204) < 0) {
            ci_debug_printf(5, "An error occured while sending allow 204 response\n");
            return CI_ERROR;
        }

        return CI_OK;
    }

    if (res == CI_MOD_ALLOW206 && req->allow204 && req->allow206 && !ci_req_sent_data(req)) {
        req->return_code = EC_206;
        return CI_OK;
    }

    if (res != CI_MOD_DONE) {
        ci_debug_printf(1, "An error occured in end-of-data handler !"
                        "return code : %d, req->allow204=%d, req->allow206=%d\n",
                        res, req->allow204, req->allow206);

        if (!ci_req_sent_data(req)) {
            req->keepalive = 0;
            ec_responce(req, EC_500);
        }
        return CI_ERROR;
    }

    return CI_OK;
}


static int do_request(ci_request_t * req)
{
     ci_service_xdata_t *srv_xdata = NULL;
     int res, preview_status = 0, auth_status = CI_ACCESS_ALLOW;
     int ret_status = CI_OK; /*By default ret_status is CI_OK, on error must set to CI_ERROR*/
     res = parse_header(req);
     if (res != EC_100) {
         /*if read some data, bad request or Service not found or Server error or what else,
           else connection timeout, or client closes the connection*/
          req->return_code = res;
          req->keepalive = 0;   // Error occured, close the connection ......
          if (res > EC_100 && req->request_header->bufused > 0)
               ec_responce(req, res);
          ci_debug_printf((req->request_header->bufused ? 5 : 11), "Error %d while parsing headers :(%d)\n",
			  res ,req->request_header->bufused);
          return CI_ERROR;
     }
     assert(req->current_service_mod);
     srv_xdata = service_data(req->current_service_mod);
     if (!srv_xdata || srv_xdata->status != CI_SERVICE_OK) {
         ci_debug_printf(2, "Service %s not initialized\n", req->current_service_mod->mod_name);
         req->keepalive = 0;
         ec_responce(req, EC_500);
         return CI_ERROR;
     }

     auth_status = req->access_type;
     if ((auth_status = access_check_request(req)) == CI_ACCESS_DENY) {
         req->keepalive = 0;
	 if (req->auth_required) {
	      ec_responce(req, EC_407); /*Responce with authentication required */
	 }
	 else {
             ec_responce(req, EC_403); /*Forbitten*/
	 }
         return CI_ERROR;      /*Or something that means authentication error */
     }

     if (res == EC_100) {
          res = parse_encaps_headers(req);
          if (res != EC_100) {
              req->keepalive = 0;
              ec_responce(req, EC_400);
              return CI_ERROR;
          }
     }

     if (req->current_service_mod->mod_init_request_data)
          req->service_data =
              req->current_service_mod->mod_init_request_data(req);
     else
          req->service_data = NULL;

     ci_debug_printf(8, "Requested service: %s\n",
                     req->current_service_mod->mod_name);

     switch (req->type) {
     case ICAP_OPTIONS:
          options_responce(req);
          ret_status = CI_OK;
          break;
     case ICAP_REQMOD:
     case ICAP_RESPMOD:
          preview_status = CI_NO_STATUS;
          if (req->preview >= 0) /*we are inside preview*/
              preview_status = do_request_preview(req);
          else {
              /* do_fake_preview return CI_OK or CI_ERROR. */
              preview_status = do_fake_preview(req);
          }
          
          if (preview_status == CI_ERROR) {
              ret_status = CI_ERROR;
              break;
          }
          else if (preview_status == CI_EOF)
              req->return_code = EC_200; /*Equivalent to "100 Continue"*/

          if (req->return_code == EC_204) /*Allow 204,  Stop processing here*/
              break;
          /*else 100 continue or 206  response or Internal error*/
          else if (req->return_code != EC_100 && req->return_code != EC_200 && req->return_code != EC_206) {
              ec_responce(req, EC_500);
              ret_status = CI_ERROR;
              break;
          }
          
          if (req->return_code == EC_100 && req->hasbody && preview_status != CI_EOF) {
               req->return_code = EC_200; /*We have to repsond with "200 OK"*/
               ci_debug_printf(9, "Going to get/send body data.....\n");
               ret_status = get_send_body(req, 0);
               if (ret_status == CI_ERROR) {
                    req->keepalive = 0; /*close the connection*/
                    ci_debug_printf(5,
                                    "An error occured. Parse error or the client closed the connection (res:%d, preview status:%d)\n",
                                    ret_status, preview_status);
                    break;
               }
          }

          /*We have received all data from the client. Call the end-of-data service handler and process*/
          ret_status = do_end_of_data(req);
          if (ret_status == CI_ERROR) {
              req->keepalive = 0; /*close the connection*/
              break;
          }
          
          if (req->return_code == EC_204)
              break;  /* Nothing to be done, stop here*/
          /*else we have to send response to the client*/


          unlock_data(req); /*unlock data if locked so that it can be send to the client*/
          ret_status = send_remaining_response(req);
          if (ret_status == CI_ERROR) {
              req->keepalive = 0; /*close the connection*/
              ci_debug_printf(5, "Error while sending rest responce or client closed the connection\n");
          }
          /*We are finished here*/
          break;
     default:
          req->keepalive = 0; /*close the connection*/
          ret_status = CI_ERROR;
          break;
     }

     if (req->current_service_mod->mod_release_request_data
         && req->service_data)
          req->current_service_mod->mod_release_request_data(req->service_data);

//     debug_print_request(req);
     return ret_status;
}

int process_request(ci_request_t * req)
{
    int res;
    ci_service_xdata_t *srv_xdata;
    res = do_request(req);

    if (req->pstrblock_read_len) {
        ci_debug_printf(5, "There are unparsed data od size %d: \"%s\"\n. Move to connection buffer\n", req->pstrblock_read_len, req->pstrblock_read);
    }
   
    if (res<0 && req->request_header->bufused == 0) /*Did not read anything*/
	return CI_NO_STATUS;

    if (STATS) {
        if (req->return_code != EC_404 && req->current_service_mod)
            srv_xdata = service_data(req->current_service_mod);
        else
            srv_xdata = NULL;

        STATS_LOCK();
        if (STAT_REQUESTS >= 0) STATS_INT64_INC(STAT_REQUESTS,1);
        
        if (req->type == ICAP_REQMOD) {
            STATS_INT64_INC(STAT_REQMODS, 1);
            if (srv_xdata)
                STATS_INT64_INC(srv_xdata->stat_reqmods, 1);
        }
        else if (req->type == ICAP_RESPMOD) {
            STATS_INT64_INC(STAT_RESPMODS, 1);
            if (srv_xdata)
                STATS_INT64_INC(srv_xdata->stat_respmods, 1);
        }
        else if (req->type == ICAP_OPTIONS) {
            STATS_INT64_INC(STAT_OPTIONS, 1);
            if (srv_xdata)
                STATS_INT64_INC(srv_xdata->stat_options, 1);
        }

        if (res <0 && STAT_FAILED_REQUESTS >= 0)
            STATS_INT64_INC(STAT_FAILED_REQUESTS,1);
        else if (req->return_code == EC_204) {
            STATS_INT64_INC(STAT_ALLOW204, 1);
            if (srv_xdata)
                STATS_INT64_INC(srv_xdata->stat_allow204, 1);
        }


        if (STAT_BYTES_IN >= 0) STATS_KBS_INC(STAT_BYTES_IN, req->bytes_in);
        if (STAT_BYTES_OUT >= 0) STATS_KBS_INC(STAT_BYTES_OUT, req->bytes_out);
        if (STAT_HTTP_BYTES_IN >= 0) STATS_KBS_INC(STAT_HTTP_BYTES_IN, req->http_bytes_in);
        if (STAT_HTTP_BYTES_OUT >= 0) STATS_KBS_INC(STAT_HTTP_BYTES_OUT, req->http_bytes_out);
        if (STAT_BODY_BYTES_IN >= 0) STATS_KBS_INC(STAT_BODY_BYTES_IN, req->body_bytes_in);
        if (STAT_BODY_BYTES_OUT >= 0) STATS_KBS_INC(STAT_BODY_BYTES_OUT, req->body_bytes_out);

        if (srv_xdata) {
            if (srv_xdata->stat_bytes_in >= 0) 
                STATS_KBS_INC(srv_xdata->stat_bytes_in, req->bytes_in);
            if (srv_xdata->stat_bytes_out >= 0) 
                STATS_KBS_INC(srv_xdata->stat_bytes_out, req->bytes_out);
            if (srv_xdata->stat_http_bytes_in >= 0) 
                STATS_KBS_INC(srv_xdata->stat_http_bytes_in, req->http_bytes_in);
            if (srv_xdata->stat_http_bytes_out >= 0) 
                STATS_KBS_INC(srv_xdata->stat_http_bytes_out, req->http_bytes_out);
            if (srv_xdata->stat_body_bytes_in >= 0) 
                STATS_KBS_INC(srv_xdata->stat_body_bytes_in, req->body_bytes_in);
            if (srv_xdata->stat_body_bytes_out >= 0) 
                STATS_KBS_INC(srv_xdata->stat_body_bytes_out, req->body_bytes_out);
        }
        STATS_UNLOCK();
    }

    return res; /*Allow to log even the failed requests*/
}
