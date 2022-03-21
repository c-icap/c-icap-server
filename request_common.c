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
#include "request_util.h"
#include "util.h"
#include "body.h"
#include "mem.h"

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
int ci_buf_mem_alloc(struct ci_buf *buf, size_t size)
{
    if (!(buf->buf = ci_buffer_alloc2(size * sizeof(char), &buf->size)))
        return 0;
    buf->used = 0;
    return buf->size;
}

void ci_buf_mem_free(struct ci_buf *buf)
{
    ci_buffer_free(buf->buf);
    buf->buf = NULL;
    buf->size = 0;
    buf->used = 0;
}


int ci_buf_write(struct ci_buf *buf, char *data, size_t len)
{
    if (len > (buf->size - buf->used))
        return -1;
    memcpy(buf->buf + buf->used, data, len);
    buf->used += len;
    return len;
}

int ci_buf_reset_and_resize(struct ci_buf *buf, size_t req_size)
{
    if (buf->size >= req_size) {
        buf->used = 0;
        return buf->size;
    }

    if (buf->buf)
        ci_buffer_free(buf->buf);
    return ci_buf_mem_alloc(buf, req_size);
}

int ci_buf_reset_size(struct ci_buf *buf, int req_size)
{
    return ci_buf_reset_and_resize(buf, (size_t)req_size);
}

static void ci_request_t_pack(ci_request_t * req, int is_request)
{
    ci_encaps_entity_t **elist, *e;
    char buf[512];
    int i, added;

    req->packed = 1;

    if (is_request && req->preview >= 0) {
        snprintf(buf, sizeof(buf), "Preview: %d", req->preview);
        ci_headers_add(req->request_header, buf);
    }

    elist = req->entities;

    if (elist[0] != NULL) {
        elist[0]->start = 0;

        if (elist[1] != NULL) {
            elist[1]->start = sizeofencaps(elist[0]);

            if (elist[2] != NULL) {
                elist[2]->start = sizeofencaps(elist[1]) + elist[1]->start;
            }
        }
    }


    if (elist[0] == NULL) {
        snprintf(buf, sizeof(buf), "Encapsulated: null-body=0");
    } else {
        added = snprintf(buf, sizeof(buf), "Encapsulated: ");
        for (i = 0; elist[i] != NULL && i < 3 && added < sizeof(buf); ++i)
            added += snprintf(buf + added, sizeof(buf) - added,
                              "%s%s=%d",
                              (i != 0 ? ", " : ""),
                              ci_encaps_entity_string(elist[i]->type),
                              elist[i]->start);
    }
    if (is_request)
        ci_headers_add(req->request_header, buf);
    else
        ci_headers_add(req->response_header, buf);

    while ((e = *elist++) != NULL) {
        if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR)
            ci_headers_pack((ci_headers_list_t *) e->entity);
    }
    /*e_list is not usable now !!!!!!! */
    if (is_request)
        ci_headers_pack(req->request_header);
    else
        ci_headers_pack(req->response_header);
}

void ci_request_pack(ci_request_t *req)
{
    ci_request_t_pack(req, 1);
}

void ci_response_pack(ci_request_t *req)
{
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
    req = (ci_request_t *) malloc(sizeof(ci_request_t));
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
    req->eof_sent = 0;

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

    req->echo_body = NULL;

    req->preview_data_type = -1;
    req->auth_required = 0;
    req->protocol = 0;
    req->proto_version.major = 0;
    req->proto_version.minor = 0;

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

    ci_clock_time_reset(&req->start_r_t);
    ci_clock_time_reset(&req->stop_r_t);
    ci_clock_time_reset(&req->start_w_t);
    ci_clock_time_reset(&req->stop_w_t);
    req->processing_time = 0;

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
    req->eof_sent = 0;

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

    if (req->echo_body) {
        ci_ring_buf_destroy(req->echo_body);
        req->echo_body = NULL;
    }

    req->preview_data_type = -1;
    req->auth_required = 0;
    req->protocol = 0;
    req->proto_version.major = 0;
    req->proto_version.minor = 0;

    if (req->log_str)
        ci_buffer_free(req->log_str);
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

    ci_clock_time_reset(&req->start_r_t);
    ci_clock_time_reset(&req->stop_r_t);
    ci_clock_time_reset(&req->start_w_t);
    ci_clock_time_reset(&req->stop_w_t);
    req->processing_time = 0;

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
    assert(req);
    if (req->connection)
        free(req->connection);

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

    if (req->echo_body) {
        ci_ring_buf_destroy(req->echo_body);
        req->echo_body = NULL;
    }

    if (req->log_str)
        ci_buffer_free(req->log_str);

    if (req->attributes)
        ci_array_destroy(req->attributes);

    free(req);
}

char *ci_request_set_log_str(ci_request_t *req, char *logstr)
{
    int size;
    assert(req);
    if (req->log_str)
        ci_buffer_free(req->log_str);

    size = strlen(logstr) + 1;
    req->log_str = ci_buffer_alloc(size * sizeof(char));
    if (!req->log_str)
        return NULL;
    strncpy(req->log_str, logstr, size);
    req->log_str[size] = '\0';
    return req->log_str;
}

int  ci_request_set_str_attribute(ci_request_t *req, const char *name, const char *value)
{
    assert(req);
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
    for (; isspace((int)*pos) && *pos != '\0'; ++pos);
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
        for (; (isspace((int)*pos) || *pos == ',') && *pos != '\0'; ++pos);
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

            while (*end == ' ' || *end == '\t') ++end; /*ignore spaces*/
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
                    while (*end == ' ' || *end == '\t') ++end; /*ignore spaces*/
                    remains = req->pstrblock_read_len - (end - req->pstrblock_read);
                    if (remains >= 18 && strncmp(end, "use-original-body=", 18) == 0) {
                        req->i206_use_original_body = strtol(end + 18, &end, 10);
                    } else if (remains >= 4 && strncmp(end, "ieof", 4) != 0)
                        return CI_ERROR;
                    // ignore any char after ';'
                    while (*end != '\r') ++end;
                    req->eof_received = 1;
                }

            } else {
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
                } else      /*we are in all or part of the \r\n end of chunk data */
                    req->write_to_module_pending = 0;
                req->chunk_bytes_read += remains;
                req->pstrblock_read += remains;
                req->pstrblock_read_len -= remains;
                req->request_bytes_in += remains; //append parsed data
            } else {
                tmp = remains - req->pstrblock_read_len;
                if (tmp < 2) {
                    req->write_to_module_pending =
                        req->pstrblock_read_len - tmp;
                } else {
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

    if ((bytes = ci_connection_read_nonblock(req->connection, req->rbuf + req->pstrblock_read_len, bytes)) < 0) {    /*... read some data... */
        ci_debug_printf(5, "Error reading data (read return=%d, errno=%d) \n", bytes, errno);
        return CI_ERROR;
    }
    ci_clock_time_get(&req->stop_r_t);
    req->pstrblock_read_len += bytes;  /* ... (size of data is readed plus old)... */
    req->bytes_in += bytes;
    return CI_OK;
}

