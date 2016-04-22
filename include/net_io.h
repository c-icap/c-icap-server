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


#ifndef __NET_IO_H
#define __NET_IO_H

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#else
#include <WinSock2.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32
#define ci_socket int
#define CI_SOCKET_ERROR -1
#else
#define ci_socket SOCKET
#define CI_SOCKET_ERROR INVALID_SOCKET
#endif

typedef struct ci_sockaddr{
#ifdef USE_IPV6
     struct sockaddr_storage  sockaddr;
#else
     struct sockaddr_in   sockaddr;
#endif
     int ci_sin_family;/* #define ci_sin_family sockaddr.sin_family */
     int ci_sin_port;  /* #define ci_sin_port   sockaddr.sin_port   */
     void *ci_sin_addr;
     int ci_inaddr_len;
}  ci_sockaddr_t;


#define CI_MAXHOSTNAMELEN 256

#ifdef USE_IPV6

typedef union ci_inaddr {
     struct in_addr ipv4_addr;
     struct in6_addr ipv6_addr;
} ci_in_addr_t;

#define ci_inaddr_zero(addr) (memset(&(addr),0,sizeof(ci_in_addr_t)))
#define ci_inaddr_copy(dest,src) (memcpy(&(dest),&(src),sizeof(ci_in_addr_t)))
#define ci_ipv4_inaddr_hostnetmask(addr)((addr).ipv4_addr.s_addr=htonl(0xFFFFFFFF))
#define ci_in6_addr_u32(addr) ((uint32_t *)&((addr).ipv6_addr))
#define ci_ipv6_inaddr_hostnetmask(addr)(ci_in6_addr_u32(addr)[0]=htonl(0xFFFFFFFF),\
					 ci_in6_addr_u32(addr)[1]=htonl(0xFFFFFFFF), \
					 ci_in6_addr_u32(addr)[2]=htonl(0xFFFFFFFF), \
					 ci_in6_addr_u32(addr)[3]=htonl(0xFFFFFFFF))

#define CI_IPLEN      46
#define CI_SOCKADDR_SIZE sizeof(struct sockaddr_storage)

#else /*IPV4 only*/

typedef struct in_addr ci_in_addr_t;
#define ci_inaddr_zero(addr) ((addr).s_addr=0)
#define ci_inaddr_copy(dest,src) ((dest)=(src))
#define ci_ipv4_inaddr_hostnetmask(addr)((addr).s_addr=htonl(0xFFFFFFFF))


#define CI_IPLEN      16
#define CI_SOCKADDR_SIZE sizeof(struct sockaddr_in)
#endif

#define wait_for_read       0x1
#define wait_for_write      0x2
#define wait_for_readwrite  0x3 


typedef struct ci_ip {
    ci_in_addr_t address;
    ci_in_addr_t netmask;
    int family;
} ci_ip_t;

typedef struct ci_connection{
     ci_socket fd;
     ci_sockaddr_t claddr;
     ci_sockaddr_t srvaddr;
}  ci_connection_t ;

CI_DECLARE_FUNC(void) ci_connection_destroy(ci_connection_t *connection);

CI_DECLARE_FUNC(void) ci_fill_sockaddr(ci_sockaddr_t *addr);
CI_DECLARE_FUNC(void) ci_fill_ip_t(ci_ip_t *ip, ci_sockaddr_t *addr);

CI_DECLARE_FUNC(void) ci_copy_sockaddr(ci_sockaddr_t *dest, ci_sockaddr_t *src);
CI_DECLARE_FUNC(int) ci_inet_aton(int af,const char *cp, void *inp);
CI_DECLARE_FUNC(const char *) ci_inet_ntoa(int af,const void *src, char *dst,int cnt);


CI_DECLARE_FUNC(const char *) ci_sockaddr_t_to_ip(ci_sockaddr_t *addr, char *ip,int ip_strlen);
#define ci_conn_remote_ip(conn,ip) ci_sockaddr_t_to_ip(&(conn->claddr),ip,CI_IPLEN)
#define ci_conn_local_ip(conn,ip)  ci_sockaddr_t_to_ip(&(conn->srvaddr),ip,CI_IPLEN)

#ifdef USE_IPV6
CI_DECLARE_FUNC(void) ci_sockaddr_set_port(ci_sockaddr_t *addr, int port);
#define ci_sockaddr_set_family(addr,family) ((addr).sockaddr.ss_family=family)
#else
CI_DECLARE_FUNC(void) ci_sockaddr_set_port(ci_sockaddr_t *addr, int port);
#define ci_sockaddr_set_family(addr,family) ((addr).sockaddr.sin_family=family/*,(addr).ci_sin_family=family*/)
#endif

CI_DECLARE_FUNC(const char *) ci_sockaddr_t_to_host(ci_sockaddr_t *addr, char *hname, int maxhostlen);
CI_DECLARE_FUNC(int) ci_host_to_sockaddr_t(const char *servername, ci_sockaddr_t * addr, int proto);

CI_DECLARE_FUNC(void) ci_copy_connection(ci_connection_t *dest, ci_connection_t *src);

CI_DECLARE_FUNC(int) icap_socket_opts(ci_socket fd, int secs_to_linger);
CI_DECLARE_FUNC(ci_socket) icap_init_server(char *address, int port,int *protocol_family,int secs_to_linger);


CI_DECLARE_FUNC(int) ci_wait_for_data(ci_socket fd,int secs,int what_wait);

#define ci_wait_for_incomming_data(fd,timeout) ci_wait_for_data(fd,timeout,wait_for_read)
#define ci_wait_for_outgoing_data(fd,timeout) ci_wait_for_data(fd,timeout,wait_for_write)

CI_DECLARE_FUNC(int) ci_netio_init(ci_socket fd);
CI_DECLARE_FUNC(int) ci_read(ci_socket fd,void *buf,size_t count,int timeout);
CI_DECLARE_FUNC(int) ci_write(ci_socket fd, const void *buf,size_t count,int timeout);
CI_DECLARE_FUNC(int) ci_read_nonblock(ci_socket fd, void *buf,size_t count);
CI_DECLARE_FUNC(int) ci_write_nonblock(ci_socket fd, const void *buf,size_t count);

CI_DECLARE_FUNC(int) ci_linger_close(ci_socket fd,int secs_to_linger);
CI_DECLARE_FUNC(int) ci_hard_close(ci_socket fd);

#ifdef __cplusplus
}
#endif

#endif
