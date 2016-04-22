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
#include "debug.h"
#include "request.h"
#include "simple_api.h"

static void * _os_malloc(int size)
{
     return malloc(size);
}

static void _os_free(void *ptr)
{
     free(ptr);
}

void *(*__intl_malloc)(int) = _os_malloc;
void (*__intl_free)(void *)  = _os_free;


/* struct buf functions*/
void ci_buf_init(struct ci_buf *buf)
{
     buf->buf = NULL;
     buf->size = 0;
     buf->used = 0;
}

void ci_buf_reset(struct ci_buf *buf)
{
     buf->used = 0;
}
int ci_buf_mem_alloc(struct ci_buf *buf, int size)
{
     if (!(buf->buf = __intl_malloc(size * sizeof(char))))
          return 0;
     buf->size = size;
     buf->used = 0;
     return size;
}

void ci_buf_mem_free(struct ci_buf *buf)
{
     __intl_free(buf->buf);
     buf->buf = NULL;
     buf->size = 0;
     buf->used = 0;
}


int ci_buf_write(struct ci_buf *buf, char *data, int len)
{
     if (len > (buf->size - buf->used))
          return -1;
     memcpy(buf->buf + buf->used, data, len);
     buf->used += len;
     return len;
}

int ci_buf_reset_size(struct ci_buf *buf, int req_size)
{
     if (buf->size > req_size)
          return req_size;
     if (buf->buf)
          __intl_free(buf->buf);
     return ci_buf_mem_alloc(buf, req_size);
}

void ci_request_t_pack(ci_request_t * req, int is_request)
{
     ci_encaps_entity_t **elist, *e;
     char buf[256];

     req->packed = 1;

     if (is_request && req->preview >= 0) {
          sprintf(buf, "Preview: %d", req->preview);
          ci_headers_add(req->request_header, buf);
     }

     elist = req->entities;

     if (elist[0] != NULL)
          elist[0]->start = 0;

     if (elist[1] != NULL) {
          elist[1]->start = sizeofencaps(elist[0]);
     }

     if (elist[2] != NULL) {
          elist[2]->start = sizeofencaps(elist[1]) + elist[1]->start;
     }


     if (elist[0] == NULL) {
          sprintf(buf, "Encapsulated: null-body=0");
     }
     else if (elist[2] != NULL) {
          sprintf(buf, "Encapsulated: %s=%d, %s=%d, %s=%d",
                  ci_encaps_entity_string(elist[0]->type), elist[0]->start,
                  ci_encaps_entity_string(elist[1]->type), elist[1]->start,
                  ci_encaps_entity_string(elist[2]->type), elist[2]->start);
     }
     else if (elist[1] != NULL) {
          sprintf(buf, "Encapsulated: %s=%d, %s=%d",
                  ci_encaps_entity_string(elist[0]->type), elist[0]->start,
                  ci_encaps_entity_string(elist[1]->type), elist[1]->start);
     }
     else {                     /*Only req->entities[0] exists */
          sprintf(buf, "Encapsulated: %s=%d",
                  ci_encaps_entity_string(elist[0]->type), elist[0]->start);
     }
     if(is_request)
	 ci_headers_add(req->request_header, buf);
     else
	 ci_headers_add(req->response_header, buf);

     while ((e = *elist++) != NULL) {
          if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR)
               ci_headers_pack((ci_headers_list_t *) e->entity);
     }
     /*e_list is not usable now !!!!!!! */
     if(is_request)
	 ci_headers_pack(req->request_header);
     else
	 ci_headers_pack(req->response_header);
}

void ci_request_pack(ci_request_t *req){
     ci_request_t_pack(req, 1);
}

void ci_response_pack(ci_request_t *req){
     ci_request_t_pack(req, 0);
}


/*
Valid forms of encapsulated entities

   REQMOD  request  encapsulated_list: [reqhdr] reqbody
   REQMOD  response encapsulated_list: {[reqhdr] reqbody} |
                                       {[reshdr] resbody}
   RESPMOD request  encapsulated_list: [reqhdr] [reshdr] resbody
   RESPMOD response encapsulated_list: [reshdr] resbody
   OPTIONS request  encapsulated_list: [optbody]
   OPTIONS response encapsulated_list: optbody

TODO: 
   The following function must chech request and if the encapsulated entity is valid
   must put it to the right position in req->entities .........
*/

//alloc_an_entity
ci_encaps_entity_t *ci_request_alloc_entity(ci_request_t * req, int type, int val)
{
     ci_encaps_entity_t *e = NULL;

     if (type > ICAP_OPT_BODY || type < 0) {    //
          return NULL;
     }

     if (req->trash_entities[type]) {
          e = req->trash_entities[type];
          req->trash_entities[type] = NULL;
          e->type = type;
          e->start = val;
          if (e->type == ICAP_REQ_HDR
              || e->type == ICAP_RES_HDR) {
              if (e->entity)
                  ci_headers_reset((ci_headers_list_t *)e->entity);
          }
          ci_debug_printf(8, "Get entity from trash....\n");
          return e;
     }

     //Else there is no available entity to trash_entities so make a new....
     ci_debug_printf(8, "Allocate a new entity of type %d\n", type);
     return mk_encaps_entity(type, val);
}


int ci_request_release_entity(ci_request_t * req, int pos)
{
     int type = 0;
     if (!req->entities[pos])
          return 0;

     type = req->entities[pos]->type;
     if (type > ICAP_OPT_BODY || type < 0) {    //?????????
          destroy_encaps_entity(req->entities[pos]);
          req->entities[pos] = NULL;
          return 0;
     }

     if (req->trash_entities[type] != NULL) {
          ci_debug_printf(3,
                          "ERROR!!!!! There is an entity of type %d to trash..... ",
                          type);
          destroy_encaps_entity(req->trash_entities[type]);
     }
     req->trash_entities[type] = req->entities[pos];
     req->entities[pos] = NULL;
     return 1;
}


ci_request_t *ci_request_alloc(ci_connection_t * connection)
{
     ci_request_t *req;
     int i;
     req = (ci_request_t *) __intl_malloc(sizeof(ci_request_t));
     if (!req)
         return NULL;
    
     req->connection = connection;
     req->packed = 0;
     req->user[0] = '\0';

     req->access_type = 0;

     req->service[0] = '\0';
     req->req_server[0] = '\0';
     req->current_service_mod = NULL;
     req->service_data = NULL;
     req->args[0] = '\0';
     req->type = -1;
     req->preview = -1;
     ci_buf_init(&(req->preview_data));

     req->keepalive = 1;        /*Keep alive connection is the default behaviour for icap protocol. */
     req->allow204 = 0;
     req->allow206 = 0;
     req->hasbody = 0;
     req->responce_hasbody = 0;
     req->eof_received = 0;

     req->request_header = ci_headers_create();
     req->response_header = ci_headers_create();
     req->xheaders = ci_headers_create();
     req->status = SEND_NOTHING;
     req->return_code = -1;

     req->pstrblock_read = NULL;
     req->pstrblock_read_len = 0;
     req->current_chunk_len = 0;
     req->chunk_bytes_read = 0;
     req->write_to_module_pending = 0;

     req->pstrblock_responce = NULL;
     req->remain_send_block_bytes = 0;
     req->data_locked = 1;
     req->i206_use_original_body = -1;

     req->preview_data_type = -1;
     req->auth_required = 0;

     
     req->log_str = NULL;
     req->attributes = NULL;
     memset(&(req->xclient_ip), 0, sizeof(ci_ip_t));

     req->bytes_in = 0;
     req->bytes_out = 0;
     req->request_bytes_in = 0;
     req->http_bytes_in = 0;
     req->http_bytes_out = 0;
     req->body_bytes_in = 0;
     req->body_bytes_out = 0;

     for (i = 0; i < 5; i++)    //
          req->entities[i] = NULL;
     for (i = 0; i < 7; i++)    //
          req->trash_entities[i] = NULL;

     return req;

}

/*reset_request simply reset request to use it with tunneled requests
  The req->access_type must not be reset!!!!!

  Also buffers where the pstrblock_read points must not free or reset.
*/
void ci_request_reset(ci_request_t * req)
{
     int i;
     /*     memset(req->connections,0,sizeof(ci_connection)) *//*Not really needed... */

     req->packed = 0;
     req->user[0] = '\0';
     req->service[0] = '\0';
     req->current_service_mod = NULL;
     req->service_data = NULL;
     req->args[0] = '\0';
     req->type = -1;
     req->preview = -1;
     ci_buf_reset(&(req->preview_data));

     req->keepalive = 1;        /*Keep alive connection is the default behaviour for icap protocol. */
     req->allow204 = 0;
     req->allow206 = 0;
     req->hasbody = 0;
     req->responce_hasbody = 0;
     ci_headers_reset(req->request_header);
     ci_headers_reset(req->response_header);
     ci_headers_reset(req->xheaders);
     req->eof_received = 0;
     req->status = SEND_NOTHING;
     req->return_code = -1;

     req->pstrblock_read = NULL;
     req->pstrblock_read_len = 0;
     req->current_chunk_len = 0;
     req->chunk_bytes_read = 0;
     req->pstrblock_responce = NULL;
     req->remain_send_block_bytes = 0;
     req->write_to_module_pending = 0;
     req->data_locked = 1;
     req->i206_use_original_body = -1;

     req->preview_data_type = -1;
     req->auth_required = 0;

     if (req->log_str)
         __intl_free(req->log_str);
     req->log_str = NULL;

     if (req->attributes)
         ci_array_destroy(req->attributes);
     req->attributes = NULL;
     memset(&(req->xclient_ip), 0, sizeof(ci_ip_t));

     req->bytes_in = 0;
     req->bytes_out = 0;
     req->request_bytes_in = 0;
     req->http_bytes_in = 0;
     req->http_bytes_out = 0;
     req->body_bytes_in = 0;
     req->body_bytes_out = 0;

     for (i = 0; req->entities[i] != NULL; i++) {
          ci_request_release_entity(req, i);
     }

     /*Reset the encapsulated response or request headers*/
     if (req->trash_entities[ICAP_REQ_HDR] && 
         req->trash_entities[ICAP_REQ_HDR]->entity)
         ci_headers_reset((ci_headers_list_t *)req->trash_entities[ICAP_REQ_HDR]->entity);
     if (req->trash_entities[ICAP_RES_HDR] && 
         req->trash_entities[ICAP_RES_HDR]->entity)
         ci_headers_reset((ci_headers_list_t *)req->trash_entities[ICAP_RES_HDR]->entity);
}

void ci_request_destroy(ci_request_t * req)
{
     int i;
     if (req->connection)
          __intl_free(req->connection);

     ci_buf_mem_free(&(req->preview_data));
     ci_headers_destroy(req->request_header);
     ci_headers_destroy(req->response_header);
     ci_headers_destroy(req->xheaders);
     for (i = 0; req->entities[i] != NULL; i++)
          destroy_encaps_entity(req->entities[i]);

     for (i = 0; i < 7; i++) {
          if (req->trash_entities[i])
               destroy_encaps_entity(req->trash_entities[i]);
     }

     if (req->log_str)
         __intl_free(req->log_str);

     if (req->attributes)
         ci_array_destroy(req->attributes);

     __intl_free(req);
}

char *ci_request_set_log_str(ci_request_t *req, char *logstr)
{
     int size;
     if (req->log_str)
         __intl_free(req->log_str);

     size = strlen(logstr) + 1;
     req->log_str = __intl_malloc(size*sizeof(char));
     if (!req->log_str)
         return NULL;
     strcpy(req->log_str, logstr);
     return req->log_str;
}

int  ci_request_set_str_attribute(ci_request_t *req, const char *name, const char *value)
{
    if (req->attributes == NULL) {
        req->attributes = ci_array_new(4096);
        if (!req->attributes) {
            ci_debug_printf(1, "Error allocating request attributes array!\n");
            return 0;
        }
    }

    if (!ci_str_array_add(req->attributes, name, value)) {
        ci_debug_printf(1, "Not enough space to add attribute %s:%s for service %s\n", name, value, req->service);
        return 0;
    }
    return 1;
}

int ci_request_206_origin_body(ci_request_t *req, uint64_t offset)
{
    if (!req)
        return 0;
    
    if (!req->allow206) {
        ci_debug_printf(1, "Request does not support allow206 responses! Can not set use-original-body extension\n");
        return 0;
    }

#if 0
    if (req->body_bytes_in < offset) {
        ci_debug_printf(1, "Can not set use-original-body extension to offset longer than the known body size");
        return 0;
    }
#endif
    req->i206_use_original_body = offset;
    return 1;
}

int process_encapsulated(ci_request_t * req, const char *buf)
{
     const char *start;
     const char *pos;
     char *end;
     int type = 0, num = 0, val = 0;
     int hasbody = 1;           /*Assume that we have a resbody or reqbody or optbody */
     start = buf + 13; /*strlen("Encapsulated:")*/
     pos = start;
     end = (char *)start;
     for (;isspace(*pos) && *pos != '\0'; ++pos);
     while (*pos != '\0') {
          type = get_encaps_type(pos, &val, &end);
          if (type < 0) /*parse error - return "400 bad request"*/
              return EC_400;
          if (num > 5)          /*In practice there is an error here .... */
               break;
          if (type == ICAP_NULL_BODY)
               hasbody = 0;     /*We have not a body */
          req->entities[num++] = ci_request_alloc_entity(req, type, val);

          assert(start != end);
          pos = end;            /* points after the value of entity.... */
          for (;(isspace(*pos) || *pos == ',') && *pos != '\0'; ++pos);
     }
     req->hasbody = hasbody;
     return EC_100;
}


enum chunk_status { READ_CHUNK_DEF = 1, READ_CHUNK_DATA };


/*
  maybe the wdata must moved to the ci_request_t and write_to_module_pending must replace wdata_len
*/
int parse_chunk_data(ci_request_t * req, char **wdata)
{
     char *end;
     const char *eofChunk;
     int chunkLen, remains, tmp;
     int read_status = 0;

     *wdata = NULL;
     if (req->write_to_module_pending) {
          /*We must not here if the chunk buffer did not flashed */
          return CI_ERROR;
     }

     while (1) {
          if (req->current_chunk_len == req->chunk_bytes_read)
               read_status = READ_CHUNK_DEF;
          else
               read_status = READ_CHUNK_DATA;

          if (read_status == READ_CHUNK_DEF) {
               if ((eofChunk = strnstr(req->pstrblock_read, "\r\n", req->pstrblock_read_len)) == NULL) {
                   /*Check for wrong protocol data, or possible parse error*/                   
                   if (req->pstrblock_read_len >= BUFSIZE)
                       return CI_ERROR; /* To big chunk definition?*/
                   return CI_NEEDS_MORE;
               }
               eofChunk += 2;
               chunkLen = eofChunk - req->pstrblock_read;
               // Count parse data
               req->request_bytes_in += (eofChunk - req->pstrblock_read);

               errno = 0;
               tmp = strtol(req->pstrblock_read, &end, 16);
               /*here must check for erron */
               if (tmp == 0 && req->pstrblock_read == end) {    /*Oh .... an error ... */
                    ci_debug_printf(5, "Parse error:count=%d,start=%c\n",
                                    tmp, req->pstrblock_read[0]);
                    return CI_ERROR;
               }
               req->current_chunk_len = tmp;
               req->chunk_bytes_read = 0;

               while(*end == ' ' || *end == '\t') ++end; /*ignore spaces*/
               if (req->current_chunk_len == 0) {
                    remains = req->pstrblock_read_len - chunkLen;
                    if (remains < 2) /*missing the \r\n of the 0[; ...]\r\n\r\n of the eof chunk*/
                         return CI_NEEDS_MORE;

                    if (*eofChunk != '\r' && *(eofChunk + 1) != '\n')
                        return CI_ERROR; /* Not an \r\n\r\n eof chunk definition. Parse Error*/

                    eofChunk += 2;
                    chunkLen += 2;
                    req->request_bytes_in += 2; /*count 2 extra chars on parsed data*/

                    if (*end == ';') {
                         end++;
                         while(*end == ' ' || *end == '\t') ++end; /*ignore spaces*/
                         remains = req->pstrblock_read_len - (end - req->pstrblock_read);
			 if (remains >= 18 && strncmp(end, "use-original-body=", 18) == 0) {
			     req->i206_use_original_body = strtol(end + 18, &end, 10);
		         }
			 else if (remains >=4 && strncmp(end, "ieof", 4) != 0)
                              return CI_ERROR;
                         // ignore any char after ';'
                         while(*end != '\r') ++end;
                         req->eof_received = 1;
                    }

               }
               else {
                    read_status = READ_CHUNK_DATA;
                    /*include the \r\n end of chunk data */
                    req->current_chunk_len += 2;
               }

               /*The end pointing after the number and extensions. Should point to \r\n*/
               if (*end != '\r' || *(end + 1) != '\n') {
                   /*Extra chars in chunk definition!*/
                   return CI_ERROR;
               }

               req->pstrblock_read_len -= chunkLen;
               req->pstrblock_read += chunkLen;
          }
          if (req->current_chunk_len == 0) /*zero chunk received, stop for now*/
              return CI_EOF;

          /*if we have data for service leaving this function now */
          if (req->write_to_module_pending)
               return CI_OK;
          if (read_status == READ_CHUNK_DATA) {
               if (req->pstrblock_read_len <= 0) {
                    return CI_NEEDS_MORE;
               }
               *wdata = req->pstrblock_read;
               remains = req->current_chunk_len - req->chunk_bytes_read;
               if (remains <= req->pstrblock_read_len) {        /*we have all the chunks data */
                   if (remains > 2) {
                        req->write_to_module_pending = remains - 2;
                        req->http_bytes_in += req->write_to_module_pending;
                        req->body_bytes_in += req->write_to_module_pending;
                   } 
                   else        /*we are in all or part of the \r\n end of chunk data */
                        req->write_to_module_pending = 0;
                   req->chunk_bytes_read += remains;
                   req->pstrblock_read += remains;
                   req->pstrblock_read_len -= remains;
                   req->request_bytes_in += remains; //append parsed data
               }
               else {
                    tmp = remains - req->pstrblock_read_len;
                    if (tmp < 2) {
                         req->write_to_module_pending =
                             req->pstrblock_read_len - tmp;
                    }
                    else {
                         req->write_to_module_pending = req->pstrblock_read_len;
		    }
                    req->http_bytes_in += req->write_to_module_pending;
                    req->body_bytes_in += req->write_to_module_pending;
                    req->request_bytes_in += req->pstrblock_read_len; //append parsed data

                    req->chunk_bytes_read += req->pstrblock_read_len;
                    req->pstrblock_read += req->pstrblock_read_len;
                    req->pstrblock_read_len -= req->pstrblock_read_len;
               }
          }
          if (req->pstrblock_read_len == 0)
               return CI_NEEDS_MORE;
     }

     return CI_OK;
}

int net_data_read(ci_request_t * req)
{
     int bytes;

     if (req->pstrblock_read != req->rbuf) {
          /*... put the current data to the begining of buf .... */
          if (req->pstrblock_read_len)
               memmove(req->rbuf, req->pstrblock_read, req->pstrblock_read_len);
          req->pstrblock_read = req->rbuf;
     }

     bytes = BUFSIZE - req->pstrblock_read_len;
     if (bytes <= 0) {
          ci_debug_printf(5,
                          "Not enough space to read data! Is this a bug (%d %d)?????\n",
                          req->pstrblock_read_len, BUFSIZE);
          return CI_ERROR;
     }

     if ((bytes = ci_read_nonblock(req->connection->fd, req->rbuf + req->pstrblock_read_len, bytes)) <= 0) {    /*... read some data... */
          ci_debug_printf(5, "Error reading data (read return=%d, errno=%d) \n", bytes, errno);
          return CI_ERROR;
     }
     req->pstrblock_read_len += bytes;  /* ... (size of data is readed plus old )... */
     req->bytes_in += bytes;
     return CI_OK;
}




/*************************************************************************/
/* ICAP client functions                                                 */


ci_request_t *ci_client_request(ci_connection_t * conn, char *server,
                             char *service)
{
     ci_request_t *req;
     req = ci_request_alloc(conn);
     if (!req) {
         ci_debug_printf(1, "Error allocation ci_request_t object(ci_client_request())\n");
         return NULL;
     }
     strncpy(req->req_server, server, CI_MAXHOSTNAMELEN);
     req->req_server[CI_MAXHOSTNAMELEN] = '\0';
     strncpy(req->service, service, MAX_SERVICE_NAME);
     req->service[MAX_SERVICE_NAME] = '\0';
     return req;
}


void ci_client_request_reuse(ci_request_t * req)
{
     int i;

     req->packed = 0;
     req->args[0] = '\0';
     req->type = -1;
     ci_buf_reset(&(req->preview_data));

     req->hasbody = 0;
     req->responce_hasbody = 0;
     ci_headers_reset(req->request_header);
     ci_headers_reset(req->response_header);
     ci_headers_reset(req->xheaders);
     req->eof_received = 0;
     req->status = 0;

     req->pstrblock_read = NULL;
     req->pstrblock_read_len = 0;
     req->current_chunk_len = 0;
     req->chunk_bytes_read = 0;
     req->pstrblock_responce = NULL;
     req->remain_send_block_bytes = 0;
     req->write_to_module_pending = 0;
     req->data_locked = 1;

     req->allow204 = 0;
     req->allow206 = 0;
     req->i206_use_original_body = -1;

     req->bytes_in = 0;
     req->bytes_out = 0;
     req->http_bytes_in = 0;
     req->http_bytes_out = 0;
     req->body_bytes_in = 0;
     req->body_bytes_out = 0;

     for (i = 0; req->entities[i] != NULL; i++) {
          ci_request_release_entity(req, i);
     }

}



static int client_create_request(ci_request_t * req, char *servername, char *service,
                          int reqtype)
{
     char buf[256];

     if (reqtype != ICAP_OPTIONS && reqtype != ICAP_REQMOD
         && reqtype != ICAP_RESPMOD)
          return CI_ERROR;

     req->type = reqtype;
     snprintf(buf, 255, "%s icap://%s/%s ICAP/1.0",
              ci_method_string(reqtype), servername, service);
     buf[255] = '\0';
     ci_headers_add(req->request_header, buf);
     snprintf(buf, 255, "Host: %s", servername);
     buf[255] = '\0';
     ci_headers_add(req->request_header, buf);
     ci_headers_add(req->request_header, "User-Agent: C-ICAP-Client-Library/0.01");

     if (ci_allow204(req) && ci_allow206(req))
          ci_headers_add(req->request_header, "Allow: 204, 206");
     else if (ci_allow204(req))
          ci_headers_add(req->request_header, "Allow: 204");
     if (ci_allow206(req))
          ci_headers_add(req->request_header, "Allow: 206");

     if (!ci_headers_is_empty(req->xheaders)) {
	  ci_headers_addheaders(req->request_header, req->xheaders);
     }

     return CI_OK;
}

static int get_request_options(ci_request_t * req, ci_headers_list_t * h)
{
     const char *pstr;

     if ((pstr = ci_headers_value(h, "Preview")) != NULL) {
          req->preview = strtol(pstr, NULL, 10);
     }
     else
          req->preview = -1;


     req->allow204 = 0;
     if ((pstr = ci_headers_value(h, "Allow")) != NULL) {
          if (strtol(pstr, NULL, 10) == 204)
               req->allow204 = 1;
     }

     if ((pstr = ci_headers_value(h, "Connection")) != NULL
         && strncmp(pstr, "close", 5) == 0) {
          req->keepalive = 0;
     }

     /*Moreover we are interested for the followings */
     if ((pstr = ci_headers_value(h, "Transfer-Preview")) != NULL) {
          /*Not implemented yet */
     }

     if ((pstr = ci_headers_value(h, "Transfer-Ignore")) != NULL) {
          /*Not implemented yet */
     }

     if ((pstr = ci_headers_value(h, "Transfer-Complete")) != NULL) {
          /*Not implemented yet */
     }

     /*
        The headers Max-Connections and  Options-TTL are not needed in this client 
        but if this functions moves to a general client api must be implemented
      */

     return CI_OK;
}


static int ci_writen(int fd, char *buf, int len, int timeout)
{
     int ret = 0, remains;
     remains = len;
     while (remains) {
          if ((ret = ci_write(fd, buf, remains, timeout)) < 0)
               return ret;
          buf += ret;
          remains -= ret;
     }
     return len;
}



static int client_send_request_headers(ci_request_t * req, int has_eof, int timeout)
{
     ci_encaps_entity_t **elist, *e;
     ci_headers_list_t *headers;
     int bytes;

     ci_request_pack(req);
     if (ci_writen
         (req->connection->fd, req->request_header->buf, req->request_header->bufused, timeout) < 0)
          return CI_ERROR;
     req->bytes_out += req->request_header->bufused;
     elist = req->entities;
     while ((e = *elist++) != NULL) {
          if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR) {
               headers = (ci_headers_list_t *) e->entity;
               if (ci_writen
                   (req->connection->fd, headers->buf, headers->bufused,
                    timeout) < 0)
                    return CI_ERROR;
               req->bytes_out += headers->bufused;
          }
     }

     if (req->preview > 0 && req->preview_data.used > 0) {
          bytes = sprintf(req->wbuf, "%x\r\n", req->preview);
          if (ci_writen(req->connection->fd, req->wbuf, bytes, timeout) < 0)
               return CI_ERROR;
          req->bytes_out += bytes;
          if (ci_writen
              (req->connection->fd, req->preview_data.buf, req->preview,
               timeout) < 0)
               return CI_ERROR;
          req->bytes_out += req->preview;
          if (has_eof) {
               if (ci_writen
                   (req->connection->fd, "\r\n0; ieof\r\n\r\n", 13,
                    timeout) < 0)
                    return CI_ERROR;
               req->bytes_out += 13;
               req->eof_received = 1;

          }
          else {
               if (ci_writen(req->connection->fd, "\r\n0\r\n\r\n", 7, timeout) <
                   0)
                    return CI_ERROR;
               req->bytes_out += 7;
          }
     }
     else if (req->preview == 0) {
          if (ci_writen(req->connection->fd, "0\r\n\r\n", 5, timeout) < 0)
               return CI_ERROR;
          req->bytes_out += 5;
     }
     return CI_OK;
}

/*this function check if there is enough space in buffer buf ....*/
static int check_realloc(char **buf, int *size, int used, int mustadded)
{
     char *newbuf;
     int len;
     while (*size - used < mustadded) {
          len = *size + HEADSBUFSIZE;
          newbuf = realloc(*buf, len);
          if (!newbuf) {
               return EC_500;
          }
          *buf = newbuf;
          *size = *size + HEADSBUFSIZE;
     }
     return CI_OK;
}


static int client_parse_icap_header(ci_request_t * req, ci_headers_list_t * h)
{
     int readed = 0, eoh = 0;
     char *buf, *end;
     if (req->pstrblock_read_len < 4)   /*we need 4 bytes for the end of headers "\r\n\r\n" string */
          return CI_NEEDS_MORE;
     if ((end = strnstr(req->pstrblock_read, "\r\n\r\n", req->pstrblock_read_len)) != NULL) {
          readed = end - req->pstrblock_read + 4;
          eoh = 1;
     }
     else
          readed = req->pstrblock_read_len - 3;

     if (check_realloc(&(h->buf), &(h->bufsize), h->bufused, readed) != CI_OK)
          return CI_ERROR;

     buf = h->buf + h->bufused;
     memcpy(buf, req->pstrblock_read, readed);
     h->bufused += readed;
     req->pstrblock_read += readed;
     req->pstrblock_read_len -= readed;

     if (!eoh)
          return CI_NEEDS_MORE;

     h->bufused -= 2;           /*We keep the first \r\n  of the eohead sequence and the other dropped
                                   So stupid but for the time never mind.... */
     return CI_OK;
}

static int client_parse_encaps_header(ci_request_t * req, ci_headers_list_t * h, int size)
{
     int remains, readed = 0;
     char *buf_end = NULL;

//     readed=h->bufused;
     remains = size - h->bufused;
     if (remains < 0)           /*is it possible ????? */
          return CI_ERROR;
     if (remains == 0)
          return CI_OK;

     if (req->pstrblock_read_len > 0) {
          readed =
              (remains >
               req->pstrblock_read_len ? req->pstrblock_read_len : remains);
          memcpy(h->buf + h->bufused, req->pstrblock_read, readed);
          h->bufused += readed;
          req->pstrblock_read = (req->pstrblock_read) + readed;
          req->pstrblock_read_len = (req->pstrblock_read_len) - readed;

     }

     if (h->bufused < size)
          return CI_NEEDS_MORE;

     buf_end = h->buf + h->bufused;
     if (strncmp(buf_end - 4, "\r\n\r\n", 4) == 0) {
          h->bufused -= 2;      /*eat the last 2 bytes of "\r\n\r\n" */
          return CI_OK;
     }
     else {
          ci_debug_printf(1, "Error parsing encapsulated headers,"
                          "no \\r\\n\\r\\n at the end of headers:%s!\n",
                          buf_end);
          return CI_ERROR;
     }

}


int ci_client_get_server_options(ci_request_t * req, int timeout)
{

     if (CI_OK !=
         client_create_request(req, req->req_server, req->service,
                               ICAP_OPTIONS))
          return CI_ERROR;
     client_send_request_headers(req, 0, timeout);
/*     ci_headers_reset(req->head);*/

     do {
          ci_wait_for_incomming_data(req->connection->fd, timeout);
          if (net_data_read(req) == CI_ERROR)
               return CI_ERROR;
     } while (client_parse_icap_header(req, req->response_header) == CI_NEEDS_MORE);

     ci_headers_unpack(req->response_header);
     get_request_options(req, req->response_header);

     return CI_OK;
}

ci_connection_t *ci_client_connect_to(char *servername, int port, int proto)
{
     ci_connection_t *connection = __intl_malloc(sizeof(ci_connection_t));
     char hostname[CI_MAXHOSTNAMELEN + 1];
     unsigned int addrlen = 0;
     char errBuf[512];
     int errNo;
     
     if (!connection)
          return NULL;

     if (!ci_host_to_sockaddr_t(servername, &(connection->srvaddr), proto)) {
          ci_debug_printf(1, "Error getting address info for host '%s': %s\n",
                          servername,
                          ci_strerror(errno,  errBuf, sizeof(errBuf)));
          close(connection->fd);
          __intl_free(connection);
          return NULL;
     }
     ci_sockaddr_set_port(&(connection->srvaddr), port);

     connection->fd = socket(connection->srvaddr.ci_sin_family, SOCK_STREAM, 0);
     if (connection->fd == -1) {
          ci_debug_printf(1, "Error opening socket :%d:%s....\n",
                          errno,
                          ci_strerror(errno,  errBuf, sizeof(errBuf)));
          __intl_free(connection);
          return NULL;
     }

#ifdef USE_IPV6
     if (connection->srvaddr.ci_sin_family == AF_INET6)
         addrlen = sizeof(struct sockaddr_in6);
     else
#endif
         addrlen = sizeof(struct sockaddr_in);

     if (connect
         (connection->fd, (struct sockaddr *) &(connection->srvaddr.sockaddr),
          addrlen)) {
          errNo = errno;
          ci_sockaddr_t_to_host(&(connection->srvaddr), hostname,
                                CI_MAXHOSTNAMELEN);
          ci_debug_printf(1, "Error connecting to host  '%s': %s \n",
                          hostname,
                          ci_strerror(errNo,  errBuf, sizeof(errBuf)));
          close(connection->fd);
          __intl_free(connection);
          return NULL;
     }

     addrlen = CI_SOCKADDR_SIZE;
     if (getsockname(connection->fd,
                     (struct sockaddr *) &(connection->claddr.sockaddr), &addrlen)) {
          ci_debug_printf(1, "Error getting client sockname: %s\n",
                          ci_strerror(errno,  errBuf, sizeof(errBuf)));
          close(connection->fd);
          __intl_free(connection);
          return NULL;
     }
     ci_fill_sockaddr(&(connection->claddr));
     ci_fill_sockaddr(&(connection->srvaddr));

     ci_netio_init(connection->fd);
     return connection;
}


static int client_prepere_body_chunk(ci_request_t * req, void *data,
                              int (*readdata) (void *data, char *, int))
{
     int chunksize, def_bytes;
     char *wbuf = NULL;
     char tmpbuf[EXTRA_CHUNK_SIZE];


     wbuf = req->wbuf + EXTRA_CHUNK_SIZE;       /*Let size of EXTRA_CHUNK_SIZE space in the beggining of chunk */
     if ((chunksize = (*readdata) (data, wbuf, MAX_CHUNK_SIZE)) <= 0) {
/*	  ci_debug_printf(1,"No data to send or eof reached (%d,).......\n",chunksize);*/
          req->remain_send_block_bytes = 0;
          return chunksize;     /*Must be 0 or CI_EOF */
     }

     wbuf += chunksize;         /*Put the "\r\n" sequence at the end of chunk */
     *(wbuf++) = '\r';
     *wbuf = '\n';

     def_bytes = snprintf(tmpbuf, EXTRA_CHUNK_SIZE, "%x\r\n", chunksize);
     wbuf = req->wbuf + EXTRA_CHUNK_SIZE - def_bytes;   /*Copy the chunk define in the beggining of chunk ..... */
     memcpy(wbuf, tmpbuf, def_bytes);

     req->pstrblock_responce = wbuf;
     req->remain_send_block_bytes = def_bytes + chunksize + 2;

     return req->remain_send_block_bytes;
}


static int client_parse_incoming_data(ci_request_t * req, void *data_dest,
                               int (*dest_write) (void *, char *, int))
{
     int ret, v1, v2, status, bytes, size;
     char *buf;
     const char *val;
     ci_headers_list_t *resp_heads;

     if (req->status == GET_NOTHING) {
          /*And reading the new ..... */
          ret = client_parse_icap_header(req, req->response_header);
          if (ret != CI_OK)
               return ret;
          sscanf(req->response_header->buf, "ICAP/%d.%d %d", &v1, &v2, &status);
          ci_debug_printf(3, "Response was with status:%d \n", status);
          ci_headers_unpack(req->response_header);

          if (ci_allow204(req) && status == 204) {
	       req->status = GET_EOF;
               return 204;
	  }

          if ((val = ci_headers_search(req->response_header, "Encapsulated")) == NULL) {
               ci_debug_printf(1, "No encapsulated entities!\n");
               return CI_ERROR;
          }
          process_encapsulated(req, val);

          if (!req->entities[0])
               return CI_ERROR;

          if (!req->entities[1]) {      /*Then we have only body */
               req->status = GET_BODY;
               if (req->pstrblock_read_len == 0)
                    return CI_NEEDS_MORE;
          }
          else {
               req->status = GET_HEADERS;
               size = req->entities[1]->start - req->entities[0]->start;
               resp_heads = req->entities[0]->entity;
               if (!ci_headers_setsize(resp_heads, size))
                    return CI_ERROR;
          }

//        return CI_NEEDS_MORE;
     }

     /*read encups headers */

     /*Non option responce has one or two entities: 
        "req-headers req-body|null-body" or [resp-headers] resp-body||null-body   
        So here client_parse_encaps_header will be called for one headers block
      */

     if (req->status == GET_HEADERS) {
          size = req->entities[1]->start - req->entities[0]->start;
          resp_heads = req->entities[0]->entity;
          if ((ret =
               client_parse_encaps_header(req, resp_heads, size)) != CI_OK)
               return ret;


          ci_headers_unpack(resp_heads);
          ci_debug_printf(5, "OK reading headers, going to read body\n");

          /*reseting body chunks related variables */
          req->current_chunk_len = 0;
          req->chunk_bytes_read = 0;
          req->write_to_module_pending = 0;

	  if (req->entities[1]->type == ICAP_NULL_BODY) {
	       req->status = GET_EOF;
	       return CI_OK;
	  }
	  else {
	       req->status = GET_BODY;
	       if (req->pstrblock_read_len == 0)
	            return CI_NEEDS_MORE;
	  }
     }

     if (req->status == GET_BODY) {
          do {
               if ((ret = parse_chunk_data(req, &buf)) == CI_ERROR) {
                    ci_debug_printf(1,
                                    "Error parsing chunks, current chunk len: %d, read: %d, readlen: %d, str: %s\n",
                                    req->current_chunk_len,
                                    req->chunk_bytes_read,
                                    req->pstrblock_read_len,
                                    req->pstrblock_read);
                    return CI_ERROR;
               }

               while (req->write_to_module_pending > 0) {
                    bytes =
                        (*dest_write) (data_dest, buf,
                                       req->write_to_module_pending);
                    if (bytes < 0) {
                         ci_debug_printf(1, "Error writing to output file!\n");
                         return CI_ERROR;
                    }
                    req->write_to_module_pending -= bytes;
               }

               if (ret == CI_EOF) {
                    req->status = GET_EOF;
                    return CI_OK;
               }
          } while (ret != CI_NEEDS_MORE);

          return CI_NEEDS_MORE;
     }

     return CI_OK;
}

static const char *eof_str = "0\r\n\r\n";

static int client_send_get_data(ci_request_t * req,
                         int timeout,
                         void *data_source, int (*source_read) (void *, char *,
                                                                int),
                         void *data_dest, int (*dest_write) (void *, char *,
                                                             int)
    )
{
     int io_ret, read_status, bytes, io_action;

     if( req->pstrblock_read_len != 0) {
	 /*We have read data, parse them before enter the io loop*/
	  if ((read_status =
	        client_parse_incoming_data(req, data_dest,
					 dest_write)) == CI_ERROR)
	       return CI_ERROR;
	  if (read_status == 204)
	       return 204;
	  if (req->status == GET_EOF)
 	       return CI_OK;
     }

     if (!data_source)
       req->eof_received = 1;

     if (!req->eof_received) {
          io_action = wait_for_readwrite;
     }
     else
          io_action = wait_for_read;

     while (io_action
            && (io_ret =
                ci_wait_for_data(req->connection->fd, timeout, io_action))) {
          if (io_ret < 0)
               return CI_ERROR;
          if (io_ret & wait_for_write) {
               if (req->remain_send_block_bytes == 0) {
                    if (data_source && client_prepere_body_chunk(req, data_source, source_read)
                        <= 0) {
                         req->eof_received = 1;
                         req->pstrblock_responce = (char *) eof_str;
                         req->remain_send_block_bytes = 5;
                    }
               }
               bytes = ci_write_nonblock(req->connection->fd,
                                         req->pstrblock_responce,
                                         req->remain_send_block_bytes);
               if (bytes < 0)
                    return CI_ERROR;
               req->bytes_out += bytes;
               req->pstrblock_responce += bytes;
               req->remain_send_block_bytes -= bytes;
          }

          if (req->eof_received && req->remain_send_block_bytes == 0)
               io_action = 0;
          else
               io_action = wait_for_write;


          if (io_ret & wait_for_read) {
               if (net_data_read(req) == CI_ERROR)
                    return CI_ERROR;

               if ((read_status =
                    client_parse_incoming_data(req, data_dest,
                                               dest_write)) == CI_ERROR)
                    return CI_ERROR;

               if (read_status == 204)
                    return 204;
          }

          if (req->status != GET_EOF)
               io_action |= wait_for_read;

     }
     return CI_OK;
}

static int client_build_headers(ci_request_t *req, int has_reqhdr, int has_reshdr, int has_body)
{
     int i = 0;

     for (i = 0; i < 4; i++) {
          if (req->entities[i]) {
               ci_request_release_entity(req, i);
          }
     }
     i = 0;

     if (has_reqhdr)
         req->entities[i++] = ci_request_alloc_entity(req, ICAP_REQ_HDR, 0);
     if (has_reshdr)
          req->entities[i++] = ci_request_alloc_entity(req, ICAP_RES_HDR, 0);
     if (has_body)
          req->entities[i] = ci_request_alloc_entity(req, req->type == ICAP_RESPMOD ? ICAP_RES_BODY : ICAP_REQ_BODY, 0);
     else
          req->entities[i] = ci_request_alloc_entity(req, ICAP_NULL_BODY, 0);
     return 1;
}

int ci_client_icapfilter(ci_request_t * req,
                         int timeout,
                         ci_headers_list_t * req_headers,
                         ci_headers_list_t * resp_headers,
                         void *data_source, int (*source_read) (void *, char *,
                                                                int),
                         void *data_dest, int (*dest_write) (void *, char *,
                                                             int))
{
     int i, ret, v1, v2, remains, pre_eof = 0, preview_status;
     char *buf;
     const char *val;

     if (CI_OK !=
         client_create_request(req, req->req_server, req->service,
                               req->type)) {
          ci_debug_printf(1, "Error making respmod request ....\n");
          return CI_ERROR;
     }

     if (!data_source)
       req->preview = -1;

     if (req->preview > 0) {    /*The preview data will be send with headers.... */
          ci_buf_mem_alloc(&(req->preview_data), req->preview); /*Alloc mem for preview data */
          buf = req->preview_data.buf;
          remains = req->preview;
          while (remains && !pre_eof) { /*Getting the preview data */
               if ((ret = (*source_read) (data_source, buf, remains)) <= 0) {
                    pre_eof = 1;
                    break;
               }
               remains -= ret;

          }
          req->preview -= remains;
          req->preview_data.used = req->preview;
     }
     if (pre_eof)
          req->eof_received = 1;

     /*Bulid client request structure*/
     if (!client_build_headers(req, (req_headers != NULL), (resp_headers != NULL), (data_source != NULL)))
         return CI_ERROR;

     /*Add the user supplied headers */
     if (req_headers) {
         ci_debug_printf(5, "Going to add %d request headers\n", req_headers->used);
          for (i = 0; i < req_headers->used; i++) {
              ci_debug_printf(8, "Add request header: %s\n", req_headers->headers[i]);
               ci_http_request_add_header(req, req_headers->headers[i]);
          }
     }
     if (resp_headers) {
         ci_debug_printf(5, "Going to add %d response headers\n", resp_headers->used);
          for (i = 0; i < resp_headers->used; i++) {
              ci_debug_printf(8, "Add resp header: %s\n", resp_headers->headers[i]);
               ci_http_response_add_header(req, resp_headers->headers[i]);
          }
     }

     if ((ret = client_send_request_headers(req, pre_eof, timeout)) < 0) {
          return CI_ERROR;
     }

     /*send body */

     /* ci_headers_reset(req->head);*/
     for (i = 0; req->entities[i] != NULL; i++) {
          ci_request_release_entity(req, i);
     }
     preview_status = 100;

     if (req->preview >= 0) {   /*we must wait for ICAP responce here..... */

          do {
               ci_wait_for_incomming_data(req->connection->fd, timeout);
               if (net_data_read(req) == CI_ERROR)
                    return CI_ERROR;
          } while (client_parse_icap_header(req, req->response_header) == CI_NEEDS_MORE);

          sscanf(req->response_header->buf, "ICAP/%d.%d %d", &v1, &v2, &preview_status);
          ci_debug_printf(3, "Preview response was with status: %d \n",
                          preview_status);
          if (preview_status == 204)
              ci_headers_unpack(req->response_header);
          else if ((req->eof_received && preview_status == 200) || preview_status == 206) {
               ci_headers_unpack(req->response_header);
               if ((val = ci_headers_search(req->response_header, "Encapsulated")) == NULL) {
                    ci_debug_printf(1, "No encapsulated entities!\n");
                    return CI_ERROR;
               }
               process_encapsulated(req, val);
               if (!req->entities[1])   /*Then we have only body */
                    req->status = GET_BODY;
               else
                    req->status = GET_HEADERS;
          }
          else
	       ci_headers_reset(req->response_header);
     }

     if (preview_status == 204 || preview_status == 206)
          return preview_status;

     ret =
         client_send_get_data(req, timeout, data_source, source_read, data_dest,
                              dest_write);

     if (preview_status == 206 && ret == CI_OK)
         return 206;

     return ret;
}

/**
 * Return 1 if the HTTP headers (if any) are read completely
 * otherwise return 0;
 */
int ci_client_http_headers_completed(ci_request_t * req)
{
    return (req->status >= GET_PREVIEW);
}

