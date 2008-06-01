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


#include "simple_api.h"



/*
int ci_resp_check_body(ci_request_t *req){
     int i;
     ci_encaps_entity_t **e=req->entities;
     for(i=0;e[i]!=NULL;i++)
	  if(e[i]->type==ICAP_NULL_BODY)
	       return 0;
     return 1;
}
*/


ci_headers_list_t * ci_http_response_headers(ci_request_t * req)
{
     int i;
     ci_encaps_entity_t **e_list;
     e_list = req->entities;
     for (i = 0; e_list[i] != NULL && i < 3; i++) {     /*It is the first or second ellement */
          if (e_list[i]->type == ICAP_RES_HDR)
               return (ci_headers_list_t *) e_list[i]->entity;

     }
     return NULL;
}

ci_headers_list_t *ci_http_request_headers(ci_request_t * req)
{
     ci_encaps_entity_t **e_list;
     e_list = req->entities;
     if (e_list[0] != NULL && e_list[0]->type == ICAP_REQ_HDR)  /*It is always the first ellement */
          return (ci_headers_list_t *) e_list[0]->entity;

     return NULL;
}

int ci_http_response_reset_headers(ci_request_t * req)
{
     ci_headers_list_t *heads;
     if (!(heads =  ci_http_response_headers(req)))
          return 0;
     ci_headers_reset(heads);
     return 1;
}

int ci_http_request_reset_headers(ci_request_t * req)
{
     ci_headers_list_t *heads;
     if (!(heads = ci_http_request_headers(req)))
          return 0;
     ci_headers_reset(heads);
     return 1;
}

/*
 This function will be used when we want to responce with an error message
 to an reqmod request or respmod request.
 ICAP  rfc says that we must responce as:
 REQMOD  response encapsulated_list: {[reshdr] resbody}
 RESPMOD response encapsulated_list: [reshdr] resbody
 
 */
int ci_http_response_create(ci_request_t * req, int has_reshdr, int has_body)
{
     int i = 0;
     ci_encaps_entity_t **e_list;
     e_list = req->entities;

     for (i = 0; i < 4; i++) {
          if (req->entities[i]) {
               ci_request_release_entity(req, i);
          }
     }
     i = 0;
     if (has_reshdr)
          req->entities[i++] = ci_request_alloc_entity(req, ICAP_RES_HDR, 0);
     if (has_body)
          req->entities[i] = ci_request_alloc_entity(req, ICAP_RES_BODY, 0);
     else
          req->entities[i] = ci_request_alloc_entity(req, ICAP_NULL_BODY, 0);

     return 1;
}


char *ci_http_response_add_header(ci_request_t * req, char *header)
{
     ci_headers_list_t *heads;
     if (!(heads =  ci_http_response_headers(req)))
          return NULL;
     return ci_headers_add(heads, header);
}


char *ci_http_request_add_header(ci_request_t * req, char *header)
{
     ci_headers_list_t *heads;
     if (!(heads = ci_http_request_headers(req)))
          return NULL;
     return ci_headers_add(heads, header);
}

int ci_http_response_remove_header(ci_request_t * req, char *header)
{
     ci_headers_list_t *heads;
     if (!(heads =  ci_http_response_headers(req)))
          return 0;
     return ci_headers_remove(heads, header);
}


int ci_http_request_remove_header(ci_request_t * req, char *header)
{
     ci_headers_list_t *heads;
     if (!(heads = ci_http_request_headers(req)))
          return 0;
     return ci_headers_remove(heads, header);
}


char *ci_http_response_get_header(ci_request_t * req, char *head_name)
{
     ci_headers_list_t *heads;
     char *val;
     if (!(heads =  ci_http_response_headers(req)))
          return NULL;
     if (!(val = ci_headers_value(heads, head_name)))
          return NULL;
     return val;
}

char *ci_http_request_get_header(ci_request_t * req, char *head_name)
{
     ci_headers_list_t *heads;
     char *val;
     if (!(heads = ci_http_request_headers(req)))
          return NULL;
     if (!(val = ci_headers_value(heads, head_name)))
          return NULL;
     return val;
}


ci_off_t ci_http_content_lenght(ci_request_t * req)
{
     ci_headers_list_t *heads;
     char *val;
     if (!(heads =  ci_http_response_headers(req))) {
          /*Then maybe is a reqmod reauest, try to get request headers */
          if (!(heads = ci_http_request_headers(req)))
               return 0;
     }
     if (!(val = ci_headers_value(heads, "Content-Length")))
          return 0;
     return ci_strto_off_t(val, NULL, 10);
}

char *ci_http_request(ci_request_t * req)
{
     ci_headers_list_t *heads;
     if (!(heads = ci_http_request_headers(req)))
          return NULL;
     return heads->headers[0];
}

char *ci_icap_add_xheader(ci_request_t * req, char *header)
{
     return ci_headers_add(req->xheaders, header);
}
