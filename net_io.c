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
#include "util.h"
#ifdef USE_OPENSSL
#include "net_io_ssl.h"
#endif

#include <assert.h>


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
    } else {
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
    if (ip_dest->family == AF_INET6)
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
#if defined(USE_OPENSSL)
    dest->tls_conn_pcontext = src->tls_conn_pcontext;
#endif
    dest->flags = src->flags;
}

void ci_connection_reset(ci_connection_t *conn)
{
    conn->fd = CI_SOCKET_INVALID;
#if defined(USE_OPENSSL)
    conn->tls_conn_pcontext = NULL;
#endif
    conn->flags = 0;
}

int ci_connection_init(ci_connection_t *conn, ci_connection_type_t type)
{
    socklen_t claddrlen;
    struct sockaddr *addr;
    assert(type == ci_connection_server_side || type == ci_connection_client_side);
    if (type == ci_connection_server_side) {
        claddrlen = sizeof(conn->srvaddr.sockaddr);
        addr = (struct sockaddr *) &(conn->srvaddr.sockaddr);
    } else {
        claddrlen = sizeof(conn->claddr.sockaddr);
        addr = (struct sockaddr *) &(conn->claddr.sockaddr);
    }

    if (getsockname(conn->fd, addr, &claddrlen)) {
        /* caller should handle the error */
        return 0;
    }
    ci_fill_sockaddr(&(conn->claddr));
    ci_fill_sockaddr(&(conn->srvaddr));

    ci_connection_set_nonblock(conn);

    return 1;
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
        ci_debug_printf(5, "Error getting addrinfo for '%s':%s\n", servername, gai_strerror(ret));
        return 0;
    }
    //fill the addr..... and
    memcpy(&(addr->sockaddr), res->ai_addr, CI_SOCKADDR_SIZE);
    freeaddrinfo(res);
    ci_fill_sockaddr(addr);
    return 1;
}

ci_connection_t *ci_connection_create()
{
    ci_connection_t *connection = malloc(sizeof(ci_connection_t));
    if (connection)
        ci_connection_reset(connection);
    return connection;
}

/**
 * Close and free the given connection
 */
void ci_connection_destroy(ci_connection_t *connection)
{
    if ( connection ) {
        if (ci_socket_valid(connection->fd))
            ci_connection_hard_close(connection);
        free(connection);
    }
}

int ci_connect_to_nonblock(ci_connection_t *connection, const char *servername, int port, int proto)
{
    char errBuf[512];
    int errcode;

    if (!connection)
        return -1;

    if (!ci_socket_valid(connection->fd)) {
        if (!ci_host_to_sockaddr_t(servername, &(connection->srvaddr), proto)) {
            ci_debug_printf(1, "Error getting address info for host '%s'\n", servername);
            return -1;
        }
        ci_sockaddr_set_port(&(connection->srvaddr), port);

        connection->fd = ci_socket_connect(&(connection->srvaddr), &errcode);
        if (!ci_socket_valid(connection->fd)) {
            ci_debug_printf(1, "Error connecting to host  '%s': %s \n",
                            servername,
                            ci_strerror(errcode,  errBuf, sizeof(errBuf)));
            return -1;
        }

        return 0;
    }

    if (!(connection->flags & CI_CONNECTION_CONNECTED)) {
        if ((errcode = ci_socket_connected_ok(connection->fd)) != 0) {
            ci_debug_printf(1, "Error while connecting to host '%s': %s\n",
                            servername,
                            ci_strerror(errcode,  errBuf, sizeof(errBuf)));
            return -1;
        }

        if (!ci_connection_init(connection, ci_connection_client_side)) {
            ci_debug_printf(1, "Error initializing connection to '%s': %s\n",
                            servername,
                            ci_strerror(errno,  errBuf, sizeof(errBuf)));
            return -1;
        }
        connection->flags &= CI_CONNECTION_CONNECTED;
    }

    return 1;
}

ci_connection_t *ci_connect_to(const char *servername, int port, int proto, int timeout)
{
    int ret;
    ci_connection_t *connection = ci_connection_create();
    if (!connection) {
        ci_debug_printf(1, "Failed to allocate memory for ci_connection_t object\n");
        return NULL;
    }

    ret = ci_connect_to_nonblock(connection, servername, port, proto);
    if (ret < 0)
        return NULL;

    do {
        ret = ci_wait_for_data(connection->fd, timeout, ci_wait_for_write);
    } while (ret > 0 && (ret & ci_wait_should_retry)); //while iterrupted by signal

    if (ret > 0)
        ret = ci_connect_to_nonblock(connection, servername, port, proto);

    if (ret <= 0) {
        ci_debug_printf(1, "Connection to '%s:%d' failed/timedout\n",
                        servername, port);
        ci_connection_destroy(connection);
        return NULL;
    }

    return connection;
}

int ci_connection_wait(ci_connection_t *conn, int secs, int what_wait)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_wait_tls(conn, secs, what_wait);
#endif
    return ci_wait_for_data(conn->fd, secs, what_wait);
}

int ci_connection_read(ci_connection_t *conn, void *buf, size_t count, int timeout)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_read_tls(conn, buf, count, timeout);
#endif
    return ci_read(conn->fd, buf, count, timeout);
}

int ci_connection_write(ci_connection_t *conn, void *buf, size_t count, int timeout)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_write_tls(conn, buf, count, timeout);
#endif
    return ci_write(conn->fd, buf, count, timeout);
}

int ci_connection_read_nonblock(ci_connection_t *conn, void *buf, size_t count)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_read_nonblock_tls(conn, buf, count);
#endif
    return ci_read_nonblock(conn->fd, buf, count);
}

int ci_connection_write_nonblock(ci_connection_t *conn, void *buf, size_t count)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_write_nonblock_tls(conn, buf, count);
#endif
    return ci_write_nonblock(conn->fd, buf, count);
}

int ci_connection_linger_close(ci_connection_t *conn, int timeout)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_linger_close_tls(conn, timeout);
#endif
    return ci_linger_close(conn->fd, timeout);
}

int ci_connection_hard_close(ci_connection_t *conn)
{
    assert(conn);
#ifdef USE_OPENSSL
    if (ci_connection_is_tls(conn))
        return ci_connection_hard_close_tls(conn);
#endif
    ci_hard_close(conn->fd);
    conn->fd = CI_SOCKET_INVALID;
    return 1;
}
