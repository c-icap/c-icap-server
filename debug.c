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
#include <stdio.h>
#include "debug.h"


void ci_debug_printf(int i,const char *format, ...){
     va_list ap;
     
     if(i>DEBUG_LEVEL)
	  return;
     
     va_start(ap,format);

     vlog_server(NULL,format,ap);
     if(DEBUG_STDOUT)
	  vprintf(format,ap);
     va_end(ap);
}


void debug_print_request(request_t *req){
     int i,j;

     debug_printf(1,"Request Type :\n");
     if(req->type>=0){
	  debug_printf(1,"     Requested: %s\n     Server: %s\n     Service: %s\n",
		 ci_method_string(req->type),
		 req->req_server,
		 req->service);
	  if(req->args){
	       debug_printf(1,"     Args: %s\n",req->args);
	  }
	  else{
	       debug_printf(1,"\n");
	  }
     }
     else{
	  debug_printf(1,"     No Method\n");
     }

     debug_printf(1,"\n\nHEADERS : \n");
     for(i=0;i<req->head->used;i++){
	  debug_printf(1,"        %s\n",req->head->headers[i]);
     }
     debug_printf(1,"\n\nEncapsulated Entities: \n");
     i=0;
     while(req->entities[i]!=NULL){
	  debug_printf(1,"\t %s header at %d\n",
		       ci_encaps_entity_string(req->entities[i]->type),req->entities[i]->start);
	  if(req->entities[i]->type<ICAP_REQ_BODY){
	       ci_header_list_t *h=(ci_header_list_t *)req->entities[i]->entity;
	       debug_printf(1,"\t\t HEADERS : \n");
	       for(j=0;j<h->used;j++){
		    debug_printf(1,"\t\t\t%d. %s\n",j,h->headers[j]);
	       }
	  }
	  i++;
     }
}
