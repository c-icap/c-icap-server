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
//#include "cfg_param.h"
#include "port.h"
#include <assert.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#if defined(USE_POLL)
#include <poll.h>
#else
#include <sys/select.h>
#endif
#include "debug.h"
#include "net_io.h"
#include "util.h"

const char *ci_sockaddr_t_to_host(ci_sockaddr_t * addr, char *hname,
                                  int maxhostlen)
{
    getnameinfo((const struct sockaddr *)&(addr->sockaddr),
                addr->ci_sin_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in), hname, maxhostlen - 1,
                NULL, 0, 0);
    return (const char *) hname;
}


#ifdef USE_IPV6
int icap_init_server_ipv6(ci_port_t *port)
{
    struct sockaddr_in6 addr;

    port->accept_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (port->accept_socket == -1) {
        ci_debug_printf(1, "Error opening ipv6 socket ....\n");
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }

    icap_socket_opts(port->accept_socket, port->secs_to_linger);

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port->port);
    if (port->address == NULL) // ListenAddress is not set in configuration file. Bind to all interfaces
        addr.sin6_addr = in6addr_any;
    else {
        if (inet_pton(AF_INET6, port->address, (void *) &addr.sin6_addr) != 1) {
            ci_debug_printf(1, "Error converting ipv6 address to the network byte order \n");
            close(port->accept_socket);
            port->accept_socket = CI_SOCKET_INVALID;
            return port->accept_socket;
        }
    }



    if (bind(port->accept_socket, (struct sockaddr *) &addr, sizeof(addr))) {
        ci_debug_printf(1, "Error bind  at ipv6 address \n");;
        close(port->accept_socket);
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }
    if (listen(port->accept_socket, 512)) {
        ci_debug_printf(1, "Error listening to ipv6 address.....\n");
        close(port->accept_socket);
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }
    port->protocol_family = AF_INET6;
    return port->accept_socket;

}

#endif

int icap_init_server(ci_port_t *port)
{
    struct sockaddr_in addr;

#ifdef USE_IPV6
    if (icap_init_server_ipv6(port) != CI_SOCKET_INVALID)
        return port->accept_socket;
    ci_debug_printf(1,
                    "WARNING! Error binding to an ipv6 address. Trying ipv4...\n");
#endif

    port->accept_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (port->accept_socket == -1) {
        ci_debug_printf(1, "Error opening socket ....\n");
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }

    icap_socket_opts(port->accept_socket, port->secs_to_linger);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port->port);
    if (port->address == NULL) // ListenAddress is not set in configuration file
        addr.sin_addr.s_addr = INADDR_ANY;
    else if (inet_pton(AF_INET, port->address, (void *) &addr.sin_addr.s_addr) != 1) {
        ci_debug_printf(1, "Error converting ipv4 address to the network byte order \n");
        close(port->accept_socket);
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }

    if (bind(port->accept_socket, (struct sockaddr *) &addr, sizeof(addr))) {
        ci_debug_printf(1, "Error binding  \n");;
        close(port->accept_socket);
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }
    if (listen(port->accept_socket, 512)) {
        ci_debug_printf(1, "Error listening .....\n");
        close(port->accept_socket);
        port->accept_socket = CI_SOCKET_INVALID;
        return port->accept_socket;
    }
    port->protocol_family = AF_INET;
    return port->accept_socket;
}



int icap_socket_opts(ci_socket fd, int secs_to_linger)
{
    struct linger li;
    int value;
    /* if (fcntl(fd, F_SETFD, 1) == -1) {
       ci_debug_printf(1,"can't set close-on-exec on server socket!");
       }
     */

    value = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1) {
        ci_debug_printf(1, "setsockopt: unable to set SO_REUSEADDR\n");
    }

    value = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) == -1) {
        ci_debug_printf(1, "setsockopt: unable to set TCP_NODELAY\n");
    }

    li.l_onoff = 1;
    li.l_linger = secs_to_linger;      /*MAX_SECS_TO_LINGER; */

    if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
                   (char *) &li, sizeof(struct linger)) < 0) {
        ci_debug_printf(1, "setsockopt: unable to set SO_LINGER \n");
    }
    return 1;
}

/*1 is success, 0 should retried, -1 error can be ignored, -2 fatal error */
int icap_accept_raw_connection(ci_port_t *port, ci_connection_t *conn)
{
    socklen_t claddrlen;

    errno = 0;
    claddrlen = sizeof(conn->claddr.sockaddr);
    if (((conn->fd =
                accept(port->accept_socket,
                       (struct sockaddr *) &(conn->claddr.sockaddr),
                       &claddrlen)) < 0)) {
        conn->fd = CI_SOCKET_INVALID;
        switch (errno) {
        case EINTR:
            return 0;
        case ECONNABORTED:
            ci_debug_printf(2, "Accepting connection aborted\n");
            return -1;
        default:
            ci_debug_printf(1, "Accept failed: errno=%d\n", errno);
            return -2;
        }
    }

    if (!ci_connection_init(conn, ci_connection_server_side)) {
        ci_debug_printf(1, "Initializing connection failed, errno:%d\n", errno);
        close(conn->fd);
        conn->fd = CI_SOCKET_INVALID;
        return -2;
    }

    return 1;
}

int ci_connection_set_nonblock(ci_connection_t *conn)
{
    fcntl(conn->fd, F_SETFL, O_NONBLOCK);    //Setting newfd descriptor to nonblocking state....
    return 1;
}

#if defined(USE_POLL)
int ci_wait_ms_for_data(int fd, int msecs, int what_wait)
{
    int ret = 0;
    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = (what_wait & ci_wait_for_read ? POLLIN : 0) | (what_wait & ci_wait_for_write ? POLLOUT : 0);

    errno = 0;
    if ((ret = poll(fds, 1, msecs)) > 0) {
        if (fds[0].revents & (POLLERR | POLLHUP)) {
            ci_debug_printf(3, "ci_wait_for_data error: the connection is terminated\n");
            return -1;
        }

        if (fds[0].revents & (POLLNVAL)) {
            ci_debug_printf(1, "ci_wait_for_data error: poll on closed socket?\n");
            return -1;
        }
        ret = 0;
        if (fds[0].revents & POLLIN)
            ret = ci_wait_for_read;
        if (fds[0].revents & POLLOUT)
            ret = ret | ci_wait_for_write;
        return ret;
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return ci_wait_should_retry;
        } else {
            ci_debug_printf(5, "Fatal error while waiting for new data (errno=%d....\n", errno);
            return -1;
        }
    }
    return 0;
}

#else
int ci_wait_ms_for_data(int fd, int msecs, int what_wait)
{
    fd_set rfds, wfds, *preadfds, *pwritefds;
    struct timeval tv;
    int ret = 0;

    if (secs >= 0) {
        tv.tv_sec = msecs / 1000;
        tv.tv_usec = (msecs % 1000) * 1000;
    }

    preadfds = NULL;
    pwritefds = NULL;

    if (what_wait & wait_for_read) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        preadfds = &rfds;
    }

    if (what_wait & wait_for_write) {
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        pwritefds = &wfds;
    }

    errno = 0;
    if ((ret =
                select(fd + 1, preadfds, pwritefds, NULL,
                       (secs >= 0 ? &tv : NULL))) > 0) {
        ret = 0;
        if (preadfds && FD_ISSET(fd, preadfds))
            ret = wait_for_read;
        if (pwritefds && FD_ISSET(fd, pwritefds))
            ret = ret | wait_for_write;
        return ret;
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return ci_wait_should_retry;
        } else {
            ci_debug_printf(5, "Fatal error while waiting for new data (errno=%d....\n", errno);
            return -1;
        }
    }
    return 0;
}
#endif


int ci_read(int fd, void *buf, size_t count, int timeout)
{
    int bytes = 0;
    do {
        bytes = read(fd, buf, count);
    } while (bytes == -1 && errno == EINTR);

    if (bytes == -1 && errno == EAGAIN) {
        int ret;
        do {
            ret = ci_wait_for_data(fd, timeout, wait_for_read);
        } while (ret & ci_wait_should_retry);

        if (ret <= 0)  /*timeout or connection closed*/
            return -1;

        do {
            bytes = read(fd, buf, count);
        } while (bytes == -1 && errno == EINTR);
    }
    if (bytes == 0) {
        return -1;
    }
    return bytes;
}


int ci_write(int fd, const void *buf, size_t count, int timeout)
{
    int bytes = 0;
    int remains = count;
    char *b = (char *) buf;

    while (remains > 0) {      //write until count bytes written
        do {
            bytes = write(fd, b, remains);
        } while (bytes == -1 && errno == EINTR);

        if (bytes == -1 && errno == EAGAIN) {
            int ret;
            do {
                ret = ci_wait_for_data(fd, timeout, wait_for_write);
            } while (ret & ci_wait_should_retry);

            if (ret <= 0) /*timeout or connection closed*/
                return -1;

            do {
                bytes = write(fd, b, remains);
            } while (bytes == -1 && errno == EINTR);

        }
        if (bytes < 0)
            return bytes;
        b = b + bytes;        //points to remaining bytes......
        remains = remains - bytes;
    }                          //Ok......
    return count;
}


int ci_read_nonblock(int fd, void *buf, size_t count)
{
    int bytes = 0;
    do {
        bytes = read(fd, buf, count);
    } while (bytes == -1 && errno == EINTR);

    if (bytes < 0 && errno == EAGAIN)
        return 0;

    if (bytes == 0) /*EOF received?*/
        return -1;

    return bytes;
}



int ci_write_nonblock(int fd, const void *buf, size_t count)
{
    int bytes = 0;
    do {
        bytes = write(fd, buf, count);
    } while (bytes == -1 && errno == EINTR);

    if (bytes < 0 && errno == EAGAIN)
        return 0;

    if (bytes == 0) /*connection is closed?*/
        return -1;

    return bytes;
}



int ci_linger_close(int fd, int timeout)
{
    char buf[10];
    int ret;
    ci_debug_printf(8, "Waiting to close connection\n");

    if (shutdown(fd, SHUT_WR) != 0) {
        close(fd);
        return 1;
    }

    while (ci_wait_for_data(fd, timeout, wait_for_read)
            && (ret = ci_read_nonblock(fd, buf, 10)) > 0)
        ci_debug_printf(10, "OK I linger %d bytes.....\n", ret);

    close(fd);
    ci_debug_printf(8, "Connection closed ...\n");
    return 1;
}


int ci_hard_close(int fd)
{
    close(fd);
    return 1;
}

ci_socket_t ci_socket_connect(ci_sockaddr_t *srvaddr, int *errcode)
{
    unsigned int addrlen = 0;
    int fd;
    fd = socket(srvaddr->ci_sin_family, SOCK_STREAM, 0);
    if (fd == -1)
        return CI_SOCKET_INVALID;

#ifdef USE_IPV6
    if (srvaddr->ci_sin_family == AF_INET6)
        addrlen = sizeof(struct sockaddr_in6);
    else
#endif
        addrlen = sizeof(struct sockaddr_in);

    // Sets the fd to non-block mode
    fcntl(fd, F_SETFL, O_NONBLOCK);

    int ret;
    ret = connect(fd, (struct sockaddr *) &(srvaddr->sockaddr), addrlen);
    if (ret < 0 && errno != EINPROGRESS) {
        close(fd);
        fd = -1;
    }
    return (ci_socket_t)fd;
}

int ci_socket_connected_ok(ci_socket_t fd)
{
    int errcode = 0;
    socklen_t len = sizeof(errcode);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &len) != 0)
        errcode = errno;

    return errcode;
}

/*
int readline(int fd,char *buf){
     int i = 0, readed = 0;
     char c,oc=0;
     while((readed = icap_read(fd, &c, 1)) > 0 && c != '\n'  && i < BUFSIZE ){
      if(c=='\r'){
           icap_read(fd, &c, 1);
           if(c == '\n')
            break;
           buf[i++] = '\r';
           buf[i++] = c;
      }
      else
           buf[i++] = c;
     }
     buf[i] = '\0';
     if(i == BUFSIZE){
      ci_debug_printf("Readline error. Skip until eol ......\n");
      while(icap_read(fd, &c, 1) > 0 && c!='\n');
     }
     return i;
     }
*/
