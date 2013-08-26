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


#ifndef _REQUEST_H
#define _REQUEST_H

#include "header.h"
#include "service.h"
#include "net_io.h"
#include "array.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup REQUEST ICAP request API
 \ingroup API
 * ICAP request related API.
 */


//enum REQUEST_STATUS { WAIT,SERVED };

enum GETDATA_STATUS {GET_NOTHING=0,GET_HEADERS,GET_PREVIEW,GET_BODY,GET_EOF};
enum SENDDATA_STATUS {SEND_NOTHING=0, SEND_RESPHEAD, SEND_HEAD1, SEND_HEAD2, SEND_HEAD3, SEND_BODY, SEND_EOF };

/*enum BODY_RESPONCE_STATUS{ CHUNK_DEF=1,CHUNK_BODY,CHUNK_END};*/


#define CI_NO_STATUS   0
#define CI_OK                1
#define CI_NEEDS_MORE 2
#define CI_ERROR          -1
#define CI_EOF              -2


#define EXTRA_CHUNK_SIZE  30
#define MAX_CHUNK_SIZE    4064   /*4096 -EXTRA_CHUNK_SIZE-2*/
#define MAX_USERNAME_LEN 255

typedef struct ci_buf{
     char *buf;
     int size;
     int used;
} ci_buf_t;


struct ci_service_module;


/**
   \typedef ci_request_t
   \ingroup REQUEST
   * This is the struct which holds all the data which represent an ICAP request. The
   * developers should not access directly the fields of this struct but better use
   * the documented macros and functions
*/
typedef struct ci_request{
     ci_connection_t *connection;
     int packed;
     int type;
     char req_server[CI_MAXHOSTNAMELEN+1];
     int access_type;
     char user[MAX_USERNAME_LEN+1];
     char service[MAX_SERVICE_NAME+1];
     char args[MAX_SERVICE_ARGS];
     int preview;
     int keepalive;
     int allow204;
     int hasbody;
     int responce_hasbody;
     struct ci_buf preview_data;
     struct ci_service_module *current_service_mod;
     ci_headers_list_t *request_header;
     ci_headers_list_t *response_header;
     ci_encaps_entity_t *entities[5];//At most 3 and 1 for termination.....
     ci_encaps_entity_t *trash_entities[7];
     ci_headers_list_t *xheaders;

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
     int return_code;
     char *pstrblock_responce;
     int remain_send_block_bytes;
    
     /*Caching values for various subsystems*/
     int preview_data_type;
     int auth_required;

     /*log string*/
     char *log_str;
    ci_str_array_t *attributes;

     /* statistics */
     uint64_t bytes_in;
     uint64_t bytes_out;
     uint64_t http_bytes_in;
     uint64_t http_bytes_out;
     uint64_t body_bytes_in;
     uint64_t body_bytes_out;

     /* added flags/variables*/
     int allow206;
     int64_t i206_use_original_body;
} ci_request_t;

#define lock_data(req) (req->data_locked=1)
#define unlock_data(req) (req->data_locked=0)

/*This functions needed in server (mpmt_server.c ) */
ci_request_t *newrequest(ci_connection_t *connection);
int recycle_request(ci_request_t *req,ci_connection_t *connection);
int process_request(ci_request_t *);

/*Functions used in both server and icap-client library*/
CI_DECLARE_FUNC(int) parse_chunk_data(ci_request_t *req, char **wdata);
CI_DECLARE_FUNC(int) net_data_read(ci_request_t *req);
CI_DECLARE_FUNC(int) process_encapsulated(ci_request_t *req, const char *buf);

/*********************************************/
/*Buffer functions (I do not know if they must included in ci library....) */
CI_DECLARE_FUNC(void)  ci_buf_init(struct ci_buf *buf);
CI_DECLARE_FUNC(void)  ci_buf_reset(struct ci_buf *buf);
CI_DECLARE_FUNC(int)   ci_buf_mem_alloc(struct ci_buf *buf,int size);
CI_DECLARE_FUNC(void)  ci_buf_mem_free(struct ci_buf *buf);
CI_DECLARE_FUNC(int)   ci_buf_write(struct ci_buf *buf,char *data,int len);
CI_DECLARE_FUNC(int)   ci_buf_reset_size(struct ci_buf *buf,int req_size);

/***************/
/*API defines */
#define ci_service_data(req) ((req)->service_data)
#define ci_allow204(req)     ((req)->allow204)
#define ci_allow206(req)     ((req)->allow206)
/*API functions ......*/
CI_DECLARE_FUNC(ci_request_t *)  ci_request_alloc(ci_connection_t *connection);
CI_DECLARE_FUNC(void)         ci_request_reset(ci_request_t *req);
CI_DECLARE_FUNC(void)         ci_request_destroy(ci_request_t *req);
CI_DECLARE_FUNC(void)         ci_request_pack(ci_request_t *req);
CI_DECLARE_FUNC(void)         ci_response_pack(ci_request_t *req);
CI_DECLARE_FUNC(ci_encaps_entity_t *) ci_request_alloc_entity(ci_request_t *req,int type,int val);
CI_DECLARE_FUNC(int)          ci_request_release_entity(ci_request_t *req,int pos);
CI_DECLARE_FUNC(char *)       ci_request_set_log_str(ci_request_t *req, char *logstr);
CI_DECLARE_FUNC(int)       ci_request_set_str_attribute(ci_request_t *req, const char *name, const char *value);

CI_DECLARE_FUNC(int)          ci_request_206_origin_body(ci_request_t *req, uint64_t offset);

/*ICAP client api*/
CI_DECLARE_FUNC(ci_request_t *)  ci_client_request(ci_connection_t *conn,char *server,char *service);
CI_DECLARE_FUNC(void)         ci_client_request_reuse(ci_request_t *req);
CI_DECLARE_FUNC(int)          ci_client_get_server_options(ci_request_t *req,int timeout);
CI_DECLARE_FUNC(ci_connection_t *)  ci_client_connect_to(char *servername,int port,int proto);
CI_DECLARE_FUNC(int)          ci_client_icapfilter(ci_request_t *req,
						   int timeout,
						   ci_headers_list_t *req_headers,
                                                   ci_headers_list_t *resp_headers,
						   void *data_source,
						   int (*source_read)(void *,char *,int),
						   void *data_dest,  
						   int (*dest_write) (void *,char *,int));


#ifdef __CI_COMPAT
#define request_t   ci_request_t
#endif

#ifdef __cplusplus
}
#endif

#endif
