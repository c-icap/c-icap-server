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
#include "service.h"
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"

int echo_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf);
int echo_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *);
int echo_end_of_data_handler(ci_request_t * req);
void *echo_init_request_data(ci_request_t * req);
void echo_close_service();
void echo_release_request_data(void *data);
int echo_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req);


CI_DECLARE_MOD_DATA ci_service_module_t service = {
     "echo",                         /* mod_name, The module name */
     "Echo demo service",            /* mod_short_descr,  Module short description */
     ICAP_RESPMOD | ICAP_REQMOD,     /* mod_type, The service type is responce or request modification */
     echo_init_service,              /* mod_init_service. Service initialization */
     NULL,                           /* post_init_service. Service initialization after c-icap 
					configured. Not used here */
     echo_close_service,           /* mod_close_service. Called when service shutdowns. */
     echo_init_request_data,         /* mod_init_request_data */
     echo_release_request_data,      /* mod_release_request_data */
     echo_check_preview_handler,     /* mod_check_preview_handler */
     echo_end_of_data_handler,       /* mod_end_of_data_handler */
     echo_io,                        /* mod_service_io */
     NULL,
     NULL
};

/*
  The echo_req_data structure will store the data required to serve an ICAP request.
*/
struct echo_req_data {
    /*Currently only the body data needed*/
    ci_cached_file_t *body;
};


/* This function will be called when the service loaded  */
int echo_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf)
{
     ci_debug_printf(5, "Initialization of echo module......\n");
     
     /*Tell to the icap clients that we can support up to 1024 size of preview data*/
     ci_service_set_preview(srv_xdata, 1024);

     /*Tell to the icap clients that we support 204 responses*/
     ci_service_enable_204(srv_xdata);

     /*Tell to the icap clients to send preview data for all files*/
     ci_service_set_transfer_preview(srv_xdata, "*");

     /*Tell to the icap clients that we want the X-Authenticated-User and X-Authenticated-Groups headers
       which contains the username and the groups in which belongs.  */
     ci_service_set_xopts(srv_xdata,  CI_XAUTHENTICATEDUSER|CI_XAUTHENTICATEDGROUPS);
     
     return CI_OK;
}

/* This function will be called when the service shutdown */
void echo_close_service() 
{
    ci_debug_printf(5,"The service shutdown!\n");
    /*Nothing to do*/
}

/*This function will be executed when a new request for echo service arrives. This function will
  initialize the required structures and data to serve the request.
 */
void *echo_init_request_data(ci_request_t * req)
{
    struct echo_req_data *echo_data;

    /*Allocate memory fot the echo_data*/
    echo_data = malloc(sizeof(struct echo_req_data));

    /*If the ICAP request encuspulates a HTTP objects which contains body data 
      and not only headers allocate a ci_cached_file_t object to store the body data.
     */
     if (ci_req_hasbody(req))
          echo_data->body = ci_cached_file_new(0);
     else
	 echo_data->body = NULL;

     /*Return to the c-icap server the allocated data*/
     return echo_data;
}

/*This function will be executed after the request served to release allocated data*/
void echo_release_request_data(void *data)
{
    /*The data points to the echo_req_data struct we allocated in function echo_init_service */
    struct echo_req_data *echo_data = (struct echo_req_data *)data;
    
    /*if we had body data, release the related allocated data*/
    if(echo_data->body)
	ci_cached_file_destroy(echo_data->body);

    free(echo_data);
}


static int whattodo = 0;
int echo_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t * req)
{
     ci_off_t content_len;
     
     /*Get the echo_req_data we allocated using the  echo_init_service  function*/
     struct echo_req_data *echo_data = ci_service_data(req);

     /*If there are is a Content-Length header in encupsulated Http object read it
      and display a debug message (used here only for debuging purposes)*/
     content_len = ci_http_content_lenght(req);
     ci_debug_printf(9, "We expect to read :%" PRINTF_OFF_T " body data\n",
                     content_len);

     /*If there are not body data in HTTP encapsulated object but only headers
       respond with Allow204 (no modification required) and terminate here the
       ICAP transaction */
     if(!ci_req_hasbody(req))
	 return CI_MOD_ALLOW204;

     /*Unlock the request body data so the c-icap server can send data before 
       all body data has received */
     ci_req_unlock_data(req);

     /*If there are not preview data tell to the client to continue sending data 
       (http object modification required). */
     if (!preview_data_len)
          return CI_MOD_CONTINUE;

     /* In most real world services we should decide here if we must modify/process
	or not the encupsulated HTTP object and return CI_MOD_CONTINUE or  
	CI_MOD_ALLOW204 respectively. The decision can be taken examining the http
	object headers or/and the preview_data buffer.

	In this example service we just use the whattodo static variable to decide
	if we want to process or not the HTTP object.
      */
     if (whattodo == 0) {
          whattodo = 1;
          ci_debug_printf(8, "Echo service will process the request\n");

	  /*if we have preview data and we want to proceed with the request processing
	    we should store the preview data. There are cases where all the body
	    data of the encapsulated HTTP object included in preview data. Someone can use
	    the ci_req_hasalldata macro to  identify these cases*/
          if (preview_data_len)
	      ci_cached_file_write(echo_data->body, preview_data, preview_data_len,
				   ci_req_hasalldata(req));
          return CI_MOD_CONTINUE;
     }
     else {
          whattodo = 0;
	  /*Nothing to do just return an allow204 (No modification) to terminate here
	   the ICAP transaction */
          ci_debug_printf(8, "Allow 204...\n");
          return CI_MOD_ALLOW204;
     }
}

/* This function will called if we returned CI_MOD_CONTINUE in  echo_check_preview_handler
 function, after we read all the data from the ICAP client*/
int echo_end_of_data_handler(ci_request_t * req)
{
    /*Nothing to do here just return CI_MOD_DONE */
     return CI_MOD_DONE;
}

/* This function will called if we returned CI_MOD_CONTINUE in  echo_check_preview_handler
   function, when new data arrived from the ICAP client and when the ICAP client is 
   ready to get data.
*/
int echo_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req)
{
     int ret;
     struct echo_req_data *echo_data = ci_service_data(req);
     ret = CI_OK;

     /*write the data read from icap_client to the echo_data->body*/
     if(rlen && rbuf) {
         *rlen = ci_cached_file_write(echo_data->body, rbuf, *rlen, iseof);
         if (*rlen < 0)
	    ret = CI_ERROR;
     }

     /*read some data from the echo_data->body and put them to the write buffer to be send
      to the ICAP client*/
     if (wbuf && wlen) {
          *wlen = ci_cached_file_read(echo_data->body, wbuf, *wlen);
     }

     return ret;
}
