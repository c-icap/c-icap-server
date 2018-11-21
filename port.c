/*
 *  Copyright (C) 20016 Christos Tsantilas
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
#include "array.h"
#include "port.h"
#ifdef USE_OPENSSL
#include "net_io_ssl.h"
#endif
#include "debug.h"

static int mystrcmp(const char *s1, const char *s2)
{
    if (s1 && !s2)
        return 1;
    if (!s1 && s2)
        return -1;
    if (!s1 && !s2)
        return 0;
    return strcmp(s1, s2);
}

static int ci_port_compare_config(ci_port_t *src, ci_port_t *dst)
{
    if (dst->port != src->port)
        return 0;

    if (mystrcmp(dst->address, src->address))
        return 0;

    /*fd, configured, protocol_family and secs_to_linger are filled by c-icap while port configured*/

#ifdef USE_OPENSSL
    if (dst->tls_enabled != src->tls_enabled)
        return 0;
#endif
    return 1;
}

static void ci_port_move_configured(ci_port_t *dst, ci_port_t *src)
{
    dst->configured = src->configured;
    dst->accept_socket = src->accept_socket;
    src->configured = 0;
    src->accept_socket = CI_SOCKET_INVALID;

#ifdef USE_OPENSSL
    dst->tls_enabled = src->tls_enabled;
    dst->tls_accept_details = src->tls_accept_details;
    src->tls_accept_details = NULL;
    if (src->tls_enabled)
        ci_port_reconfigure_tls(dst);
#endif
}

void ci_port_handle_reconfigure(ci_vector_t *new_ports, ci_vector_t *old_ports)
{
    int i, k;
    ci_port_t *find_port, *check_port, *check_old_port;
    for (i = 0; (check_port = (ci_port_t *)ci_vector_get(new_ports, i)) != NULL; ++i) {
        for (k = 0, find_port = NULL; (find_port == NULL) && ((check_old_port = (ci_port_t *)ci_vector_get(old_ports, k)) != NULL); ++k ) {
            if (ci_port_compare_config(check_port, check_old_port)) {
                find_port = check_port;
                ci_port_move_configured(check_port, check_old_port);
            }
        }
        if (find_port)
            ci_debug_printf(1, "Port %d is already configured\n", find_port->port);
    }
}

void ci_port_close(ci_port_t *port)
{
    if (!ci_socket_valid(port->accept_socket))
        return;

#ifdef USE_OPENSSL
    if (port->tls_accept_details)
        icap_close_server_tls(port);
    else
#endif
        close(port->accept_socket);
    port->accept_socket = CI_SOCKET_INVALID;
    port->configured = 0;
}

void ci_port_list_release(ci_vector_t *ports)
{
    int i;
    ci_port_t *p;

    for (i = 0; (p = (ci_port_t *)ci_vector_get(ports, i)) != NULL; ++i) {
        ci_port_close(p);
        if (p->address)
            free(p->address);
#ifdef USE_OPENSSL
        if (p->tls_server_cert)
            free(p->tls_server_cert);
        if (p->tls_server_key)
            free(p->tls_server_key);
        if (p->tls_client_ca_certs)
            free(p->tls_client_ca_certs);
        if (p->tls_cafile)
            free(p->tls_cafile);
        if (p->tls_capath)
            free(p->tls_capath);
        if (p->tls_ciphers)
            free(p->tls_ciphers);
#endif
    }

    ci_vector_destroy(ports);
}
