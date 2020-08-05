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


#ifndef __C_ICAP_NET_IO_H
#define __C_ICAP_NET_IO_H

#include "c-icap.h"
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
#define CI_SOCKET_INVALID -1
#define ci_socket_valid(fd) (fd > 0)
#else
#define ci_socket SOCKET
#define CI_SOCKET_INVALID INVALID_SOCKET
#endif
typedef ci_socket ci_socket_t;

typedef struct ci_sockaddr {
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
#define ci_ipv4_inaddr_hostnetmask(addr)((addr).ipv4_addr.s_addr = htonl(0xFFFFFFFF))
#define ci_in6_addr_u32(addr) ((uint32_t *)&((addr).ipv6_addr))
#define ci_ipv6_inaddr_hostnetmask(addr)(ci_in6_addr_u32(addr)[0] = htonl(0xFFFFFFFF),\
                     ci_in6_addr_u32(addr)[1] = htonl(0xFFFFFFFF), \
                     ci_in6_addr_u32(addr)[2] = htonl(0xFFFFFFFF), \
                     ci_in6_addr_u32(addr)[3] = htonl(0xFFFFFFFF))

#define CI_IPLEN      46

#else /*IPV4 only*/

typedef struct in_addr ci_in_addr_t;
#define ci_inaddr_zero(addr) ((addr).s_addr = 0)
#define ci_inaddr_copy(dest,src) ((dest) = (src))
#define ci_ipv4_inaddr_hostnetmask(addr)((addr).s_addr = htonl(0xFFFFFFFF))


#define CI_IPLEN      16
#endif

#define wait_for_read       0x1
#define wait_for_write      0x2
#define wait_for_readwrite  0x3

#define ci_wait_for_read       0x1
#define ci_wait_for_write      0x2
#define ci_wait_for_readwrite  0x3
#define ci_wait_should_retry   0x4


typedef struct ci_ip {
    ci_in_addr_t address;
    ci_in_addr_t netmask;
    int family;
} ci_ip_t;

#ifdef USE_OPENSSL
typedef void * ci_tls_conn_pcontext_t;
#define ci_connection_is_tls(conn) (conn->tls_conn_pcontext != NULL)
#endif

/*Flags for ci_connection_t object*/
#define CI_CONNECTION_CONNECTED 0x1

typedef struct ci_connection {
    ci_socket fd;
    ci_sockaddr_t claddr;
    ci_sockaddr_t srvaddr;
#ifdef USE_OPENSSL
    ci_tls_conn_pcontext_t tls_conn_pcontext;
#endif
    int32_t flags;
}  ci_connection_t ;

struct ci_port;

CI_DECLARE_FUNC(ci_connection_t *) ci_connection_create();
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
CI_DECLARE_FUNC(void) ci_connection_reset(ci_connection_t *conn);

CI_DECLARE_FUNC(int) icap_socket_opts(ci_socket fd, int secs_to_linger);
CI_DECLARE_FUNC(ci_socket) icap_init_server(struct ci_port *port);
CI_DECLARE_FUNC(int) icap_accept_raw_connection(struct ci_port *port, ci_connection_t *conn);


CI_DECLARE_FUNC(int) ci_wait_for_data(ci_socket fd,int secs,int what_wait);

#define ci_wait_for_incomming_data(fd,timeout) ci_wait_for_data(fd,timeout,wait_for_read)
#define ci_wait_for_outgoing_data(fd,timeout) ci_wait_for_data(fd,timeout,wait_for_write)

CI_DECLARE_FUNC(int) ci_connection_set_nonblock(ci_connection_t *conn);

typedef enum {ci_connection_server_side, ci_connection_client_side} ci_connection_type_t;
CI_DECLARE_FUNC(int) ci_connection_init(ci_connection_t *conn, ci_connection_type_t type);

CI_DECLARE_FUNC(int) ci_read(ci_socket fd,void *buf,size_t count,int timeout);
CI_DECLARE_FUNC(int) ci_write(ci_socket fd, const void *buf,size_t count,int timeout);
CI_DECLARE_FUNC(int) ci_read_nonblock(ci_socket fd, void *buf,size_t count);
CI_DECLARE_FUNC(int) ci_write_nonblock(ci_socket fd, const void *buf,size_t count);

CI_DECLARE_FUNC(int) ci_linger_close(ci_socket fd,int secs_to_linger);
CI_DECLARE_FUNC(int) ci_hard_close(ci_socket fd);
CI_DECLARE_FUNC(ci_socket_t) ci_socket_connect(ci_sockaddr_t *srvaddr, int *errcode);
CI_DECLARE_FUNC(int) ci_socket_connected_ok(ci_socket_t socket);

CI_DECLARE_FUNC(ci_connection_t *) ci_connect_to(const char *servername, int port, int proto, int timeout);
CI_DECLARE_FUNC(int) ci_connect_to_nonblock(ci_connection_t *connection, const char *servername, int port, int proto);

CI_DECLARE_FUNC(int) ci_connection_wait(ci_connection_t *conn, int secs, int what_wait);
CI_DECLARE_FUNC(int) ci_connection_read(ci_connection_t *conn, void *buf, size_t count, int timeout);
CI_DECLARE_FUNC(int) ci_connection_write(ci_connection_t *conn, void *buf, size_t count, int timeout);
CI_DECLARE_FUNC(int) ci_connection_read_nonblock(ci_connection_t *conn, void *buf, size_t count);
CI_DECLARE_FUNC(int) ci_connection_write_nonblock(ci_connection_t *conn, void *buf, size_t count);
CI_DECLARE_FUNC(int) ci_connection_linger_close(ci_connection_t *conn, int timeout);
CI_DECLARE_FUNC(int) ci_connection_hard_close(ci_connection_t *conn);
#ifdef __cplusplus
}
#endif

#endif
