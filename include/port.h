/*
 *  Copyright (C) 2016 Christos Tsantilas
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


#ifndef __C_ICAP_PORT_H
#define __C_ICAP_PORT_H

#include "c-icap.h"
#include "net_io.h"


#ifdef USE_OPENSSL
struct ci_tls_server_accept_details;
#endif

enum CI_PROTO {
    CI_PROTO_NONE = 0,
    CI_PROTO_ICAP,
    CI_PROTO_HTTP
};

/**
 * Basic configurations for a listening port
 \ingroup CONFIG
 */
typedef struct ci_port {
    int port;
    int protocol_family;
    char *address;
    int secs_to_linger;
    int tls_enabled;
    int proto;
#ifdef USE_OPENSSL
    char *tls_server_cert;
    char *tls_server_key;
    char *tls_client_ca_certs;
    char *tls_cafile;
    char *tls_capath;
    char *tls_ciphers;
    long tls_options;
#endif
    int configured;
    union{
        ci_socket_t fd;
        ci_socket_t accept_socket;
    };

#ifdef USE_OPENSSL
    struct ci_tls_server_accept_details *tls_accept_details;
#endif
    int stat_connections;
} ci_port_t;

/*For internal c-icap use*/
struct ci_vector;
void ci_port_handle_reconfigure(struct ci_vector *new_ports, struct ci_vector *old_ports);
void ci_port_close(ci_port_t *port);
void ci_port_list_release(struct ci_vector *ports);
#endif
