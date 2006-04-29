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


#ifndef _REQUEST_H
#define _REQUEST_H

#include "header.h"
#include "service.h"
#include "net_io.h"


//enum REQUEST_STATUS { WAIT,SERVED };

enum GETDATA_STATUS {GET_NOTHING=0,GET_HEADERS,GET_PREVIEW,GET_BODY,GET_EOF};
enum SENDDATA_STATUS {SEND_NOTHING=0, SEND_RESPHEAD, SEND_HEAD1, SEND_HEAD2, SEND_HEAD3, SEND_BODY, SEND_EOF };

/*enum BODY_RESPONCE_STATUS{ CHUNK_DEF=1,CHUNK_BODY,CHUNK_END};*/


#define CI_OK          1
#define CI_NEEDS_MORE  2

#define CI_ERROR      -1
#define CI_EOF        -2


#define EXTRA_CHUNK_SIZE  30
#define MAX_CHUNK_SIZE    4064   /*4096 -EXTRA_CHUNK_SIZE-2*/
#define MAX_USERNAME_LEN 255

struct ci_buf{
     char *buf;
     int size;
     int used;
} ci_buf_t;


struct service_module;

typedef struct request{
     ci_connection_t *connection;
     int type;
     int is_client_request;
     char req_server[CI_MAXHOSTNAMELEN+1];
     int access_type;
     char user[MAX_USERNAME_LEN+1];
     char *service;
     char *args;
     int preview;
     int keepalive;
     int allow204;
     int hasbody;
     int responce_hasbody;
     struct ci_buf preview_data;
     struct service_module *current_service_mod;
     ci_header_list_t *head;
     ci_encaps_entity_t *entities[5];//At most 3 and 1 for termination.....
     ci_encaps_entity_t *trash_entities[7];

     void *service_data;

     char rbuf[BUFSIZE];
     char wbuf[MAX_CHUNK_SIZE+EXTRA_CHUNK_SIZE+2];
     int eof_received;
     int data_locked;

     char *pstrblock_read; 
     int  pstrblock_read_len;
     unsigned int current_chunk_len;
     unsigned int chunk_bytes_read;
     unsigned int write_to_module_pending;

     int status;
     char *pstrblock_responce;
     int remain_send_block_bytes;
} request_t;

#define lock_data(req) (req->data_locked=1)
#define unlock_data(req) (req->data_locked=0)

/*This functions needed in server (mpmt_server.c ) */
request_t *newrequest(ci_connection_t *connection);
void destroy_request(request_t *req);
int recycle_request(request_t *req,ci_connection_t *connection);
void reset_request(request_t *req);
int process_request(request_t *);
int parse_chunk_data(request_t *req, char **wdata);
int net_data_read(request_t *req);


/*********************************************/
/*Buffer functions (I do not know if they must included in ci library....) */
CI_DECLARE_FUNC(void)  ci_buf_init(struct ci_buf *buf);
CI_DECLARE_FUNC(void)  ci_buf_reset(struct ci_buf *buf);
CI_DECLARE_FUNC(int)   ci_buf_mem_alloc(struct ci_buf *buf,int size);
CI_DECLARE_FUNC(void)  ci_buf_mem_free(struct ci_buf *buf);
CI_DECLARE_FUNC(int)   ci_buf_write(struct ci_buf *buf,char *data,int len);
CI_DECLARE_FUNC(int)   ci_buf_reset_size(struct ci_buf *buf,int req_size);


/***************/
/*API functions ......*/
CI_DECLARE_FUNC(request_t *)  ci_request_alloc(ci_connection_t *connection);
CI_DECLARE_FUNC(void)         ci_request_reset(request_t *req);
CI_DECLARE_FUNC(void)         ci_request_destroy(request_t *req);
CI_DECLARE_FUNC(void)         ci_request_pack(request_t *req);
CI_DECLARE_FUNC(ci_encaps_entity_t *) ci_request_alloc_entity(request_t *req,int type,int val);
CI_DECLARE_FUNC(int)          ci_request_release_entity(request_t *req,int pos);
CI_DECLARE_FUNC(int)          ci_read_icap_header(request_t *req,ci_header_list_t *h,int timeout);

#endif
