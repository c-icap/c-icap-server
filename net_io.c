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
#include <errno.h>
#include "net_io.h"
#include "debug.h"
#include "net_io.h"



void ci_addrtoip(struct sockaddr_in *addr, char *ip,int maxlen){
     unsigned char *addr_bytes;
     addr_bytes=(unsigned char *)&(addr->sin_addr);
     snprintf(ip,maxlen,"%d.%d.%d.%d",addr_bytes[0],addr_bytes[1],addr_bytes[2],addr_bytes[3]);
     ip[maxlen-1]='\0';
}


char *ci_addrtohost(struct in_addr *addr, char *hname, int maxhostlen)
{
     struct hostent *hent;
     hent = gethostbyaddr(addr, sizeof(*addr), AF_INET);
     if(hent == NULL){
	  /* Use the ip address as the hostname */
	  ci_addrtoip(addr,hname,maxhostlen);
     }
     else{
	  strncpy(hname, hent->h_name, maxhostlen);
     }
     return hname;
}


