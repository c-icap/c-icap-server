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
#include "net_io.h"
#include "request.h"
#include "module.h"
#include "cfg_param.h"
#include "access.h"

auth_module_t **used_authenticators=NULL;

int access_check_client(ci_connection_t *connection){
     int i=0,res;
     struct sockaddr_in *client_address, *server_address;
     if(!used_authenticators)
	  return 1;
     client_address=&(connection->claddr);
     server_address=&(connection->srvaddr);
     i=0;
     while(used_authenticators[i]!=NULL){
	  if(used_authenticators[i]->auth_client){
	       res=used_authenticators[i]->auth_client(client_address,server_address);
	       if(res==0)
		    return res;
	  }
	  i++;
     }
     return 1;
}


int access_check_request(request_t *req){
     return 1;
}

