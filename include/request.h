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

enum GETDATA_STATUS {GET_NOTHING=0,GET_HEADERS,GET_PREVIEW,GET_BODY};
enum RESPONCE_STATUS {SEND_NOTHING=0, SEND_RESPHEAD, SEND_HEAD1, SEND_HEAD2, SEND_HEAD3, SEND_BODY, SEND_EOF };

/*enum BODY_RESPONCE_STATUS{ CHUNK_DEF=1,CHUNK_BODY,CHUNK_END};*/


#define CI_OK       1
#define CI_ERROR   -1
#define CI_EOF     -2
#define  MAX_CHUNK_SIZE 512
#define EXTRA_CHUNK_SIZE 30
#define MAX_USERNAME_LEN 255

struct buf{
     char *buf;
     int size;
     int used;
} buf_t;


struct service_module;

typedef struct request{
     ci_connection_t *connection;
     int type;
     char req_server[CI_MAXHOSTNAMELEN+1];
     int access_type;
     char user[MAX_USERNAME_LEN+1];
     char *service;
     char *args;
     int preview;
     int keepalive;
     int allow204;
     int hasbody;
     struct buf preview_data;
     struct service_module *current_service_mod;
     ci_header_list_t *head;
     ci_header_list_t *responce_head;
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

//     int next_block_len;
//     int getdata_status;     
//     char chunk_defs_buf[10];
//     char *pstr_chunk_defs;
//     char remain_chunk_defs_bytes;
//     int body_responce_status;
     int responce_status;
     char *pstrblock_responce;
     int remain_send_block_bytes;
} request_t;

#define lock_data(req) (req->data_locked=1)
#define unlock_data(req) (req->data_locked=0)


request_t *newrequest(ci_connection_t *connection);
void destroy_request(request_t *req);
int recycle_request(request_t *req,ci_connection_t *connection);
int reset_request(request_t *req);
int process_request(request_t *);

/*Tool functions .........*/
int move_entity_to_trash(request_t *req,int pos);

/***************/

#endif
