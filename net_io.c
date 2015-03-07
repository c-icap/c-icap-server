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
#include <errno.h>
#include "net_io.h"
#include "debug.h"
#include "net_io.h"


#ifdef USE_IPV6
void ci_fill_sockaddr(ci_sockaddr_t * addr)
{
     addr->ci_sin_family = addr->sockaddr.ss_family;
     if (addr->ci_sin_family == AF_INET6) {
          addr->ci_sin_port =
              ((struct sockaddr_in6 *) &(addr->sockaddr))->sin6_port;
          addr->ci_sin_addr =
              &(((struct sockaddr_in6 *) &(addr->sockaddr))->sin6_addr);
          addr->ci_inaddr_len = sizeof(struct in6_addr);
     }
     else {
          addr->ci_sin_port =
              ((struct sockaddr_in *) &(addr->sockaddr))->sin_port;
          addr->ci_sin_addr =
              &(((struct sockaddr_in *) &(addr->sockaddr))->sin_addr);
          addr->ci_inaddr_len = sizeof(struct in_addr);
     }
}

void ci_copy_sockaddr(ci_sockaddr_t * dest, ci_sockaddr_t * src)
{
     memcpy(dest, src, sizeof(ci_sockaddr_t));
     if (dest->ci_sin_family == AF_INET6)
          dest->ci_sin_addr =
              &(((struct sockaddr_in6 *) &(dest->sockaddr))->sin6_addr);
     else
          dest->ci_sin_addr =
              &(((struct sockaddr_in *) &(dest->sockaddr))->sin_addr);
}

#else
void ci_fill_sockaddr(ci_sockaddr_t * addr)
{
     addr->ci_sin_family = addr->sockaddr.sin_family;
     addr->ci_sin_port = addr->sockaddr.sin_port;
     addr->ci_sin_addr = &(addr->sockaddr.sin_addr);
     addr->ci_inaddr_len = sizeof(struct in_addr);
}

void ci_copy_sockaddr(ci_sockaddr_t * dest, ci_sockaddr_t * src)
{
     memcpy(dest, src, sizeof(ci_sockaddr_t));
     dest->ci_sin_addr = &(dest->sockaddr.sin_addr);
}
#endif

void ci_fill_ip_t(ci_ip_t *ip_dest, ci_sockaddr_t *src)
{
    ip_dest->family = src->ci_sin_family;
    memcpy(&ip_dest->address,src->ci_sin_addr,sizeof(ci_in_addr_t));
    
#ifdef USE_IPV6
    if(ip_dest->family == AF_INET6)
	ci_ipv6_inaddr_hostnetmask(ip_dest->netmask);
    else
	ci_ipv4_inaddr_hostnetmask(ip_dest->netmask);
#else
    ci_ipv4_inaddr_hostnetmask(ip_dest->netmask);
#endif
}

#ifdef USE_IPV6

void ci_sockaddr_set_port(ci_sockaddr_t * addr, int port)
{
     if (addr->sockaddr.ss_family == AF_INET) {
          ((struct sockaddr_in *) &(addr->sockaddr))->sin_port = htons(port);
          addr->ci_sin_port =
               ((struct sockaddr_in6 *) &(addr->sockaddr))->sin6_port;
     } else {
          ((struct sockaddr_in6 *) &(addr->sockaddr))->sin6_port = htons(port);
          addr->ci_sin_port =
               ((struct sockaddr_in *) &(addr->sockaddr))->sin_port;
     }
}

#else

void ci_sockaddr_set_port(ci_sockaddr_t * addr, int port)
{
     addr->sockaddr.sin_port = htons(port);
     addr->ci_sin_port = addr->sockaddr.sin_port;
}
#endif



const char *ci_sockaddr_t_to_ip(ci_sockaddr_t * addr, char *ip, int maxlen)
{
     return ci_inet_ntoa(addr->ci_sin_family, addr->ci_sin_addr, ip, maxlen);
}


/*
  Needed check in configure.in for inet_pton and inet_ntop ?
  For Linux and Solaris exists. 
  But I did not found these functions in win32 for example .
*/

int ci_inet_aton(int af, const char *cp, void *addr)
{
#ifdef USE_IPV6
     return inet_pton(af, cp, addr);
#else
#ifdef HAVE_INET_ATON
     return inet_aton(cp, (struct in_addr *) addr);
#else
     ((struct in_addr *) addr)->s_addr = inet_addr(cp);
     if (((struct in_addr *) addr)->s_addr == 0xffffffff
         && strcmp(cp, "255.255.255.255") != 0)
          return 0;             /*0xffffffff =255.255.255.255 which is a valid address */
     return 1;
#endif
#endif                          /*USE_IPV6 */
}



const char *ci_inet_ntoa(int af, const void *src, char *dst, int cnt)
{
#ifdef USE_IPV6
     return inet_ntop(af, src, dst, cnt);
#else
     unsigned char *addr_bytes;
     addr_bytes = (unsigned char *) src;
     snprintf(dst, cnt, "%d.%d.%d.%d", addr_bytes[0], addr_bytes[1],
              addr_bytes[2], addr_bytes[3]);
     dst[cnt - 1] = '\0';
     return (const char *) dst;
#endif
}


void ci_copy_connection(ci_connection_t * dest, ci_connection_t * src)
{
     dest->fd = src->fd;
     ci_copy_sockaddr(&dest->claddr, &src->claddr);
     ci_copy_sockaddr(&dest->srvaddr, &src->srvaddr);
}

int ci_host_to_sockaddr_t(const char *servername, ci_sockaddr_t * addr, int proto)
{
     int ret = 0;
     struct addrinfo hints, *res;
     memset(&hints, 0, sizeof(hints));
     hints.ai_family = proto;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_protocol = 0;
     if ((ret = getaddrinfo(servername, NULL, &hints, &res)) != 0) {
	 ci_debug_printf(5, "Error geting addrinfo:%s\n", gai_strerror(ret));
	 return 0;
     }
     //fill the addr..... and
     memcpy(&(addr->sockaddr), res->ai_addr, CI_SOCKADDR_SIZE);
     freeaddrinfo(res);
     ci_fill_sockaddr(addr);
     return 1;
}
