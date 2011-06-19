/*
 *  Copyright (C) 2011 Christos Tsantilas
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

int ex206_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf);
int ex206_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *);
int ex206_end_of_data_handler(ci_request_t * req);
void *ex206_init_request_data(ci_request_t * req);
void ex206_close_service();
void ex206_release_request_data(void *data);
int ex206_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req);


CI_DECLARE_MOD_DATA ci_service_module_t service = {
     "ex206",                         /* mod_name, The module name */
     "Ex206 demo service",            /* mod_short_descr,  Module short description */
     ICAP_RESPMOD | ICAP_REQMOD,     /* mod_type, The service type is responce or request modification */
     ex206_init_service,              /* mod_init_service. Service initialization */
     NULL,                           /* post_init_service. Service initialization after c-icap 
					configured. Not used here */
     ex206_close_service,           /* mod_close_service. Called when service shutdowns. */
     ex206_init_request_data,         /* mod_init_request_data */
     ex206_release_request_data,      /* mod_release_request_data */
     ex206_check_preview_handler,     /* mod_check_preview_handler */
     ex206_end_of_data_handler,       /* mod_end_of_data_handler */
     ex206_io,                        /* mod_service_io */
     NULL,
     NULL
};

/*
  The ex206_req_data structure will store the data required to serve an ICAP request.
*/
struct ex206_req_data {
    ci_membuf_t *body;
    int script_size;
};


/* This function will be called when the service loaded  */
int ex206_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf)
{
     ci_debug_printf(5, "Initialization of ex206 module......\n");
     
     /*Tell to the icap clients that we can support up to 1024 size of preview data*/
     ci_service_set_preview(srv_xdata, 1024);

     /*Tell to the icap clients that we support 204 responses*/
     ci_service_enable_204(srv_xdata);

     /*Tell to the icap clients that we support 206 responses*/
     ci_service_enable_206(srv_xdata);


     /*Tell to the icap clients to send preview data for all files*/
     ci_service_set_transfer_preview(srv_xdata, "*");

     return CI_OK;
}

/* This function will be called when the service shutdown */
void ex206_close_service() 
{
    ci_debug_printf(5,"Service shutdown!\n");
    /*Nothing to do*/
}

/*This function will be executed when a new request for ex206 service arrives. This function will
  initialize the required structures and data to serve the request.
 */
void *ex206_init_request_data(ci_request_t * req)
{
    struct ex206_req_data *ex206_data;

    /*Allocate memory fot the ex206_data*/
    ex206_data = malloc(sizeof(struct ex206_req_data));
    ex206_data->body = NULL;
     ex206_data->script_size = 0;
     /*Return to the c-icap server the allocated data*/
     return ex206_data;
}

/*This function will be executed after the request served to release allocated data*/
void ex206_release_request_data(void *data)
{
    /*The data points to the ex206_req_data struct we allocated in function ex206_init_service */
    struct ex206_req_data *ex206_data = (struct ex206_req_data *)data;
    free(ex206_data);
}

int ex206_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t * req)
{
     ci_off_t content_len;
     const char *script = "\n<!--A simple comment added by the  ex206 C-ICAP service-->\n\n";
     const char *p, *e;
     char buf[256];
     int use_origin = 0;
     int body_altered = 0;

     /*Get the ex206_req_data we allocated using the  ex206_init_service  function*/
     struct ex206_req_data *ex206_data = ci_service_data(req);

     /*If there are is a Content-Length header in encupsulated Http object read it
      and display a debug message (used here only for debuging purposes)*/
     content_len = ci_http_content_length(req);
     ci_debug_printf(9, "We expect to read :%" PRINTF_OFF_T " body data\n",
                     (CAST_OFF_T) content_len);

     if (!ci_req_allow206(req)) /*The client does not support allow 206, return allow204*/
         return CI_MOD_ALLOW204;
     
     ci_debug_printf(8, "Ex206 service will process the request\n");
     if (preview_data_len) {
         if ((p=strncasestr(preview_data, "<html", preview_data_len)) != NULL && 
             (e = strnstr(p, ">", preview_data_len - (p-preview_data))) != NULL) {
             if ((ex206_data->body = ci_membuf_new()) == NULL)
                 return CI_ERROR;
             /* Copy body data untill the <html> tag*/
             ci_membuf_write(ex206_data->body, preview_data, (e - preview_data+1), 0);
             /* Copy the script */
             ci_membuf_write(ex206_data->body, script, strlen(script), 1);
             ex206_data->script_size = strlen(script);
             /*Use only the original body after the <html> tag */
             use_origin = e - preview_data + 1;
             ci_request_206_origin_body(req, use_origin);
             if(content_len) {
                 content_len += ex206_data->script_size - use_origin;
                 ci_http_response_remove_header(req, "Content-Length");
                 char head[512];
                 snprintf(head, 512, "Content-Length: %" PRINTF_OFF_T, (CAST_OFF_T) content_len);
                 ci_http_response_add_header(req, head);
             }
         }
         else //Else no HTML tag use all of the original body data
             ci_request_206_origin_body(req, 0);
     }
     else //Use all of the original body data
         ci_request_206_origin_body(req, 0);

     sprintf(buf , "X-Ex206-Service: %s", (body_altered ? "Modified" : "Unmodified"));
     if (req->type == ICAP_REQMOD) 
         ci_http_request_add_header(req, buf);
     else if (req->type == ICAP_RESPMOD) 
         ci_http_response_add_header(req, buf);
     
     return CI_MOD_ALLOW206;
}

/* This function will called if we returned CI_MOD_CONTINUE in  ex206_check_preview_handler
 function, after we read all the data from the ICAP client*/
int ex206_end_of_data_handler(ci_request_t * req)
{
    /*struct ex206_req_data *ex206_data = ci_service_data(req);*/
    return CI_MOD_DONE;
}

int ex206_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req)
{
     int ret;
     struct ex206_req_data *ex206_data = ci_service_data(req);
     ret = CI_OK;

     /*write the data read from icap_client to the ex206_data->body*/
     if(rlen && rbuf) {
         /*Client should not send more data here. 
           Just ignore for now*/
     }

     if (!ex206_data->body) {
         *wlen = CI_EOF;
     }
     else if (wbuf && wlen) {
         /*read some data from the ex206_data->body and put them to the write buffer to be send
           to the ICAP client*/
         *wlen = ci_membuf_read(ex206_data->body, wbuf, *wlen);
     }
     return ret;
}
