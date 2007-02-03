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


extern int TIMEOUT;

 /**/
    void send_headers_block(request_t * req, ci_headers_list_t * responce_head);


#define FORBITTEN_STR "ICAP/1.0 403 Forbidden\r\n\r\n"
#define ISTAG         "\"5BDEEEA9-12E4-2\""

request_t *newrequest(ci_connection_t * connection)
{
     request_t *req;
     int access;
     ci_connection_t *conn;

     if ((access = access_check_client(connection)) == CI_ACCESS_DENY) {        /*Check for client access */
          ci_write(connection->fd, FORBITTEN_STR, strlen(FORBITTEN_STR),
                   TIMEOUT);
          return NULL;          /*Or something that means authentication error */
     }

     conn = (ci_connection_t *) malloc(sizeof(ci_connection_t));
     ci_copy_connection(conn, connection);
     req = ci_request_alloc(conn);

     req->access_type = access;
     return req;
}


int recycle_request(request_t * req, ci_connection_t * connection)
{
     int access;

     if ((access = access_check_client(connection)) == CI_ACCESS_DENY) {        /*Check for client access */
          ci_write(connection->fd, FORBITTEN_STR, strlen(FORBITTEN_STR),
                   TIMEOUT);
          return 0;             /*Or something that means authentication error */
     }
     req->access_type = access;
     ci_request_reset(req);
     ci_copy_connection(req->connection, connection);
     return 1;
}

/*Here we want to read in small blocks icap header becouse in most cases
 it will not bigger than 512-1024 bytes.
 So we are going to do small reads and small increments in icap headers size,
 to save some space and keep small the number of over-read bytes
*/
#define ICAP_HEADER_READSIZE 512

/*this function check if there is enough space in buffer buf ....*/
int icap_header_check_realloc(char **buf, int *size, int used, int mustadded)
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
     return 0;
}


int ci_read_icap_header(request_t * req, ci_headers_list_t * h, int timeout)
{
     int bytes, request_status = 0, i, eoh = 0, startsearch = 0, readed = 0;
     char *buf_end;

     buf_end = h->buf;
     readed = 0;

     while ((bytes =
             ci_read(req->connection->fd, buf_end, ICAP_HEADER_READSIZE,
                     timeout)) > 0) {
          readed += bytes;
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
                                         ICAP_HEADER_READSIZE)) != 0)
               break;
          buf_end = h->buf + readed;
          if (startsearch > -3)
               startsearch = (readed > 3 ? -3 : -readed);       /*Including the last 3 char ellements ....... */
     }
     if (bytes < 0)
          return bytes;
     h->bufused = buf_end - h->buf;     /* -1 ; */
     req->pstrblock_read = buf_end + 2; /*after the \r\n\r\n. We keep the first \r\n and the other dropped.... */
     req->pstrblock_read_len = readed - h->bufused - 2; /*the 2 of the 4 characters \r\n\r\n and the '\0' character */
     return request_status;
}


int read_encaps_header(request_t * req, ci_headers_list_t * h, int size)
{
     int bytes = 0, remains, readed = 0;
     char *buf_end = NULL;

     if (!ci_headers_setsize(h, size))
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
          if ((bytes =
               ci_read(req->connection->fd, buf_end, remains, TIMEOUT)) < 0)
               return bytes;
          remains -= bytes;
          buf_end += bytes;
     }

     h->bufused = buf_end - h->buf;     // -1 ;
     if (strncmp(buf_end - 4, "\r\n\r\n", 4) == 0) {
          h->bufused -= 2;      /*eat the last 2 bytes of "\r\n\r\n" */
     }
     return EC_100;
}


int parse_request(request_t * req, char *buf)
{
     char *start, *end;
     int servnamelen, len, args_len;
     service_module_t *service = NULL;
     service_alias_t *salias = NULL;

     if ((start = strstr(buf, "icap://")) != NULL) {
          start = start + 7;
          if ((end = strchr(start, '/')) != NULL || (end = strchr(start, ' ')) != NULL) {       /*server */
               len = end - start;
               servnamelen =
                   (CI_MAXHOSTNAMELEN > len ? len : CI_MAXHOSTNAMELEN);
               memcpy(req->req_server, start, servnamelen);
               req->req_server[servnamelen] = '\0';
               if (*end == '/') {       /*service */
                    start = ++end;
                    while (*end != ' ' && *end != '?')
                         end++;
                    len = end - start;
                    if (len > 0) {
                         len =
                             (len < MAX_SERVICE_NAME ? len : MAX_SERVICE_NAME);
                         strncpy(req->service, start, len);
                         req->service[len] = '\0';
                         if (!(service = find_service(req->service))) { /*else search for an alias */
                              if (!(salias = find_service_alias(req->service)))
                                   return EC_404;       /* Service not found ..... */
                              service = salias->service;
                              if (salias->args)
                                   strcpy(req->args, salias->args);
                         }
                         req->current_service_mod = service;
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
                         if (!ci_method_support
                             (req->current_service_mod->mod_type, req->type)
                             || req->type != ICAP_OPTIONS)
                              return EC_405;    /* Method not allowed for service. */
                    }
                    else {
                         return EC_400;
                    }
               }                //service

          }                     //server
     }
     return EC_100;
}


int get_method(char *buf)
{
     if (!strncmp(buf, "OPTIONS", 7)) {
          return ICAP_OPTIONS;
     }
     else if (!strncmp(buf, "REQMOD", 6)) {
          return ICAP_REQMOD;
     }
     else if (!strncmp(buf, "RESPMOD", 7)) {
          return ICAP_RESPMOD;
     }
     else {
          return -1;
     }
}

int parse_header(request_t * req)
{
     int i, request_status = 0, result;
     ci_headers_list_t *h;

     h = req->head;
     if ((request_status = ci_read_icap_header(req, h, TIMEOUT)) < 0)
          return request_status;

     if ((result = get_method(h->buf)) >= 0) {
          req->type = result;
          request_status = parse_request(req, h->buf);
     }
     else
          return EC_400;

     if ((request_status = ci_headers_unpack(h)) != EC_100)
          return request_status;

     for (i = 1; i < h->used; i++) {
          if (strncmp("Preview:", h->headers[i], 8) == 0) {
               result = strtol(h->headers[i] + 9, NULL, 10);
               if (errno != EINVAL && errno != ERANGE) {
                    req->preview = result;
                    if (result >= 0)
                         ci_buf_reset_size(&(req->preview_data), result + 64);
               }
          }
          else if (strncmp("Encapsulated: ", h->headers[i], 14) == 0)
               request_status = process_encapsulated(req, h->headers[i]);
          else if (strncmp("Connection: ", h->headers[i], 12) == 0) {
/*	       if(strncasecmp(h->headers[i]+12,"keep-alive",10)==0)*/
               if (strncasecmp(h->headers[i] + 12, "close", 5) == 0)
                    req->keepalive = 0;
               /*else the default behaviour of keepalive ..... */
          }
          else if (strncmp("Allow: 204", h->headers[i], 10) == 0) {
               req->allow204 = 1;
          }
     }

     return request_status;
}


int parse_encaps_headers(request_t * req)
{
     int size, i, request_status = 0;
     ci_encaps_entity_t *e = NULL;
     for (i = 0; (e = req->entities[i]) != NULL; i++) {
          if (e->type > ICAP_RES_HDR)   //res_body,req_body or opt_body so the end of the headers.....
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
int read_preview_data(request_t * req)
{
     int ret;
     char *wdata;

     req->current_chunk_len = 0;
     req->chunk_bytes_read = 0;
     req->write_to_module_pending = 0;

     if (req->pstrblock_read_len == 0) {
          if (!ci_wait_for_incomming_data(req->connection->fd, TIMEOUT))
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

          if (!ci_wait_for_incomming_data(req->connection->fd, TIMEOUT))
               return CI_ERROR;
          if (net_data_read(req) == CI_ERROR)
               return CI_ERROR;
     } while (1);

     return CI_ERROR;
}

void ec_responce(request_t * req, int ec)
{
     char buf[256];
     snprintf(buf, 256, "ICAP/1.0 %d %s\r\n\r\n",
              ci_error_code(ec), ci_error_code_string(ec));
     buf[255] = '\0';
     ci_write(req->connection->fd, buf, strlen(buf), TIMEOUT);
}

void ec_responce_with_istag(request_t * req, int ec)
{
     char buf[256];
     snprintf(buf, 256, "ICAP/1.0 %d %s\r\nISTag: " ISTAG "\r\n\r\n",
              ci_error_code(ec), ci_error_code_string(ec));
     buf[255] = '\0';
     ci_write(req->connection->fd, buf, strlen(buf), TIMEOUT);
}

extern char MY_HOSTNAME[];
int mk_responce_header(request_t * req)
{
     ci_headers_list_t *head;
     ci_encaps_entity_t **e_list;
     char buf[512];

     head = req->head;
     ci_headers_reset(req->head);
     ci_headers_add(head, "ICAP/1.0 200 OK");
     ci_headers_add(head, "Server: C-ICAP/" VERSION);
     if (req->keepalive)
          ci_headers_add(head, "Connection: keep-alive");
     else
          ci_headers_add(head, "Connection: close");
     ci_headers_add(head, "ISTag: " ISTAG);

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

     snprintf(buf, 512, "Via: 1.0 %s (C-ICAP/" VERSION " %s )", MY_HOSTNAME,
              (req->current_service_mod->mod_short_descr ? req->
               current_service_mod->mod_short_descr : req->current_service_mod->
               mod_name));
     buf[511] = '\0';
     /*Here we must append it to an existsing Via header not just add a new header */
     if (req->type == ICAP_RESPMOD) {
          ci_respmod_add_header(req, buf);
     }
     else if (req->type == ICAP_REQMOD) {
          ci_reqmod_add_header(req, buf);
     }

     ci_request_pack(req);
     return 1;
}


/****************************************************************/
/* New  functions to send responce */

const char *eol_str = "\r\n";
const char *eof_str = "0\r\n\r\n";


int send_current_block_data(request_t * req)
{
     int bytes;
     if (req->remain_send_block_bytes == 0)
          return 0;
     if ((bytes =
          ci_write_nonblock(req->connection->fd, req->pstrblock_responce,
                            req->remain_send_block_bytes)) < 0) {
          ci_debug_printf(5, "Error writing to server (errno:%d)", errno);
          return CI_ERROR;
     }
     req->pstrblock_responce += bytes;
     req->remain_send_block_bytes -= bytes;
     return req->remain_send_block_bytes;
}


int format_body_chunk(request_t * req)
{
     int def_bytes;
     char *wbuf = NULL;
     char tmpbuf[EXTRA_CHUNK_SIZE];

     if (!req->responce_hasbody)
          return CI_EOF;
     if (req->remain_send_block_bytes > 0) {
          assert(req->remain_send_block_bytes <= MAX_CHUNK_SIZE);
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
          strcpy(req->wbuf, "0\r\n\r\n");
          req->pstrblock_responce = req->wbuf;
          req->remain_send_block_bytes = 5;
          return CI_EOF;
     }
     return CI_OK;
}



int resp_check_body(request_t * req)
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

int update_send_status(request_t * req)
{
     int i, status;
     ci_encaps_entity_t *e;

     if (req->status == SEND_NOTHING) { //If nothing has send start sending....
          if (!mk_responce_header(req)) {
               ci_debug_printf(1, "Error constructing the responce headers!\n");
               return CI_ERROR;
          }
          req->responce_hasbody = resp_check_body(req);

          req->pstrblock_responce = req->head->buf;
          req->remain_send_block_bytes = req->head->bufused;
          req->status = SEND_RESPHEAD;
          return CI_OK;
     }

     if (req->status == SEND_EOF) {
          if (req->remain_send_block_bytes == 0)
               return CI_EOF;
          else
               return CI_OK;
     }
     if (req->status == SEND_BODY) {
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


int get_send_body(request_t * req)
{
     char *wchunkdata = NULL, *rchunkdata = NULL;
     int ret, parse_chunk_ret, has_formated_data = 0;
     int (*service_io) (char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                        struct request *);
     int action = 0, rchunkisfull = 0, service_eof = 0, wbytes, rbytes;

     service_io = req->current_service_mod->mod_service_io;
     if (!service_io)
          return CI_ERROR;

     req->status = SEND_NOTHING;
     /*in the case we did not have preview data and body is small maybe
        the c-icap already read the body with the headers so do not read
        if there are unparsed bytes in pstrblock buffer
      */
     if (req->pstrblock_read_len == 0)
          action = wait_for_read;
     do {
          ret = 0;
          if (action) {
               if ((ret =
                    ci_wait_for_data(req->connection->fd, TIMEOUT,
                                     action)) <= 0)
                    break;
               if (ret & wait_for_read) {
                    if (net_data_read(req) == CI_ERROR)
                         return CI_ERROR;
               }
               if (ret & wait_for_write) {
                    if (!req->data_locked && req->status == SEND_NOTHING) {
                         update_send_status(req);
                    }
                    send_current_block_data(req);
               }
          }

          if (!req->data_locked && req->remain_send_block_bytes == 0) {
               if (update_send_status(req) == CI_ERROR)
                    return CI_ERROR;
               // if(update_send_status==CI_EOF)/*earlier responce from icap server???...*/
          }

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
               if ((*service_io)
                   (rchunkdata, &rbytes, wchunkdata, &wbytes, req->eof_received,
                    req) == CI_ERROR)
                    return CI_ERROR;
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
          } while (req->pstrblock_read_len != 0
                   && parse_chunk_ret != CI_NEEDS_MORE && !rchunkisfull);

          action = 0;
          if (!req->write_to_module_pending) {
               action = wait_for_read;
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
               action = action | wait_for_write;
          }

     } while (!req->eof_received && action);

     if (req->eof_received)
          return CI_OK;

     if (!action) {
          ci_debug_printf(1,
                          "Bug in the service. Please report to the servive author!!!!\n");
     }
     else {
          ci_debug_printf(5, "Error reading from network......\n");
     }
     return CI_ERROR;
}



int rest_responce(request_t * req)
{
     int ret = 0;
     int (*service_io) (char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                        struct request *);
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
                    ci_wait_for_data(req->connection->fd, TIMEOUT,
                                     wait_for_write)) <= 0) {
                    ci_debug_printf(1,
                                    "Timeout sending data. Ending .......\n");
                    return CI_ERROR;
               }
               send_current_block_data(req);
          }

          if (req->status == SEND_BODY && req->remain_send_block_bytes == 0) {
               req->pstrblock_responce = req->wbuf + EXTRA_CHUNK_SIZE;  /*Leave space for chunk spec.. */
               req->remain_send_block_bytes = MAX_CHUNK_SIZE;
               service_io(req->pstrblock_responce,
                          &(req->remain_send_block_bytes), NULL, NULL, 1, req);

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

void options_responce(request_t * req)
{
     char buf[256];
     char *str;
     ci_headers_list_t *head;
     int i;
     head = req->head;

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
     snprintf(buf, 255, "Service: C-ICAP/" VERSION " server - %s",
              ((str =
                req->current_service_mod->mod_short_descr) ? str : req->
               current_service_mod->mod_name));
     buf[255] = '\0';
     ci_headers_add(head, buf);
     ci_headers_add(head, "ISTag: " ISTAG);
     ci_headers_add(head, "Max-Connections: 20");
     ci_headers_add(head, "Options-TTL: 3600");

     strcpy(buf, "Date: ");
     ci_strtime_rfc822(buf + strlen(buf));
     ci_headers_add(head, buf);


     if (req->current_service_mod->mod_options_header) {
          for (i = 0;
               (str = req->current_service_mod->mod_options_header[i]) != NULL
               && i < 30; i++)
               ci_headers_add(head, str);
     }
     ci_request_pack(req);

     req->pstrblock_responce = head->buf;
     req->remain_send_block_bytes = head->bufused;

     do {
          if ((ci_wait_for_data(req->connection->fd, TIMEOUT, wait_for_write))
              <= 0) {
               ci_debug_printf(1, "Timeout sending data. Ending .......\n");
               return;
          }
          send_current_block_data(req);

     } while (req->remain_send_block_bytes > 0);

//     if(responce_body)
//        send_body_responce(req,responce_body);

}

int process_request(request_t * req)
{
     int res, preview_status = 0, auth_status = CI_ACCESS_ALLOW;
     res = parse_header(req);
     if (res != EC_100) {
          if (res >= 0)
               ec_responce(req, res);   /*Bad request or Service not found or Server error or what else...... */
          req->keepalive = 0;   // Error occured, close the connection ......
          ci_debug_printf(5, "Error parsing headers :(%d)\n",
                          req->head->bufused);
          return CI_ERROR;
     }

     auth_status = req->access_type;
     if (req->access_type == CI_ACCESS_PARTIAL
         && (auth_status = access_check_request(req)) == CI_ACCESS_DENY) {
          ec_responce(req, EC_401);     /*Responce with bad request */
          return CI_ERROR;      /*Or something that means authentication error */
     }

     if (res == EC_100)
          res = parse_encaps_headers(req);

     if (auth_status == CI_ACCESS_HTTP_AUTH
         && access_authenticate_request(req) == CI_ACCESS_DENY) {
          ec_responce(req, EC_401);     /*Responce with bad request */
          return CI_ERROR;      /*Or something that means authentication error */
     }

     if (!req->current_service_mod) {
          ci_debug_printf(1, "Service not found\n");
          ec_responce(req, EC_404);
          return CI_ERROR;
     }

     if (req->current_service_mod->mod_init_request_data)
          req->service_data =
              req->current_service_mod->mod_init_request_data(req->
                                                              current_service_mod,
                                                              req);
     else
          req->service_data = NULL;

     ci_debug_printf(8, "Requested service: %s\n",
                     req->current_service_mod->mod_name);

     switch (req->type) {
     case ICAP_OPTIONS:
          options_responce(req);
          res = CI_OK;
          break;
     case ICAP_REQMOD:
     case ICAP_RESPMOD:
          preview_status = 0;
          if (req->hasbody && req->preview >= 0) {
               /*read_preview_data returns CI_OK, CI_EOF or CI_ERROR */
               if ((preview_status = read_preview_data(req)) == CI_ERROR) {
                    ci_debug_printf(5,
                                    "An error occured while reading preview data (propably timeout)\n");
                    ec_responce(req, EC_408);
                    /*Responce with error..... */
                    break;
               }
               else if (req->current_service_mod->mod_check_preview_handler) {
                    res =
                        req->current_service_mod->
                        mod_check_preview_handler(req->preview_data.buf,
                                                  req->preview_data.used, req);
                    if (res == CI_MOD_ALLOW204) {
                         ec_responce(req, EC_204);
                         break; //Need no any modification.
                    }
                    if (res == CI_ERROR) {
                         ci_debug_printf(10,
                                         "An error occured in preview handler!!");
                         ec_responce(req, EC_500);
                         break;
                    }
                    if (preview_status > 0)
                         ec_responce(req, EC_100);
               }
          }
          else if (req->current_service_mod->mod_check_preview_handler) {
               res =
                   req->current_service_mod->mod_check_preview_handler(NULL, 0,
                                                                       req);
               if (req->allow204 && res == CI_MOD_ALLOW204) {
                    ec_responce_with_istag(req, EC_204);
                    break;      //Need no any modification.
               }
          }

          if (req->hasbody && preview_status >= 0) {
               ci_debug_printf(10, "Going to get_send_data.....\n");
               res = get_send_body(req);
               if (res != CI_OK) {
                    ci_debug_printf(5,
                                    "An error occured. Parse error or the client closed the connection (res:%d, preview status:%d)\n",
                                    res, preview_status);
                    break;
               }
          }

          if (req->current_service_mod->mod_end_of_data_handler) {
               res = req->current_service_mod->mod_end_of_data_handler(req);
/*	       while( req->current_service_mod->mod_end_of_data_handler(req,b)== CI_MOD_NOT_READY){
		    //can send some data here .........
		    }
*/
               if (req->allow204 && res == CI_MOD_ALLOW204) {
                    ec_responce_with_istag(req, EC_204);
                    break;      //Need no any modification.
               }
          }
          unlock_data(req);
          if ((res = rest_responce(req)) != CI_OK)
               ci_debug_printf(5,
                               "An error occured while sending rest responce. The client closed the connection (res:%d)\n",
                               res);

          break;
     default:
          res = CI_ERROR;
          break;
     }

     if (req->current_service_mod->mod_release_request_data
         && req->service_data)
          req->current_service_mod->mod_release_request_data(req->service_data);

//     debug_print_request(req);
     return res;
}
