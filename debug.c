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
#include <stdio.h>
#include "debug.h"


int CI_DEBUG_LEVEL = 1;
int CI_DEBUG_STDOUT = 0;

void (*__log_error) (void *req, const char *format, ...) = NULL;
void (*__vlog_error) (void *req, const char *format, va_list ap) = NULL;
void __ldebug_printf(int i, const char *format, ...)
{
    va_list ap;
    if (i <= CI_DEBUG_LEVEL) {
        va_start(ap, format);
        if (__vlog_error) {
            (*__vlog_error) (NULL, format, ap);
        }
        if (CI_DEBUG_STDOUT)
            vprintf(format, ap);
        va_end(ap);
    }
}

void ci_debug_abort(const char *file, int line, const char *function, const char *mesg)
{
    ci_debug_printf(1, "Abort at %s, %d, %s: \'%s\'\n", file, line, function, mesg);
    fflush(stderr);
    fflush(stdout);
    abort();
}

void (*__ci_debug_abort)(const char *file, int line, const char *function, const char *mesg) = ci_debug_abort;

/*
void debug_print_request(ci_request_t *req){
     int i,j;

     ci_debug_printf(1,"Request Type :\n");
     if(req->type>=0){
      ci_debug_printf(1,"     Requested: %s\n     Server: %s\n     Service: %s\n",
         ci_method_string(req->type),
         req->req_server,
         req->service);
      if(req->args){
           ci_debug_printf(1,"     Args: %s\n",req->args);
      }
      else{
           ci_debug_printf(1,"\n");
      }
     }
     else{
      ci_debug_printf(1,"     No Method\n");
     }

     ci_debug_printf(1,"\n\nHEADERS : \n");
     for(i = 0; i < req->head->used; i++){
      ci_debug_printf(1,"        %s\n",req->head->headers[i]);
     }
     ci_debug_printf(1,"\n\nEncapsulated Entities: \n");
     i = 0;
     while(req->entities[i] != NULL){
      ci_debug_printf(1,"\t %s header at %d\n",
               ci_encaps_entity_string_inline(req->entities[i]->type),req->entities[i]->start);
      if(req->entities[i]->type<ICAP_REQ_BODY){
           ci_headers_list_t *h=(ci_headeris_list_t *)req->entities[i]->entity;
           ci_debug_printf(1,"\t\t HEADERS : \n");
           for(j=0;j<h->used;j++){
            ci_debug_printf(1,"\t\t\t%d. %s\n",j,h->headers[j]);
           }
      }
      i++;
     }
}
*/
