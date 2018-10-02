/*
 *  Copyright (C) 2004-2018 Christos Tsantilas
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

#ifndef __C_ICAP_OPENSSL_SUPPORT_H
#define __C_ICAP_OPENSSL_SUPPORT_H

#include "net_io_ssl.h"
#include "port.h"
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C"
{
#endif
/**
 \ingroup TLS
 \brief Returns the openSSL BIO attached to the given connection object
 * For the TLS client connections this is the connect BIO built from a
 * BIO_s_connect call.
 * For the TLS server connections this is the socket BIO built from a
 * BIO_do_accept call.
 */
CI_DECLARE_FUNC(BIO *) ci_openssl_connection_BIO(ci_connection_t *conn);

/**
 \ingroup TLS
 \brief Return the openSSL SSL object of the given TLS connection
 */
CI_DECLARE_FUNC(SSL *) ci_openssl_connection_SSL(ci_connection_t *conn);

/**
 \ingroup TLS
 \brief Return the SSL_CTX object of the given TLS connection
 */
CI_DECLARE_FUNC(SSL_CTX *) ci_openssl_connection_SSL_CTX(ci_connection_t *conn);

/**
 \ingroup TLS
 \brief Converts to the equivalent SSL_CTX object the given c-icap tls context
 */
CI_DECLARE_FUNC(SSL_CTX *) ci_openssl_context_SSL_CTX(ci_tls_pcontext_t pcontext);

/**
 \ingroup TLS
 \brief Returns the openSSL TLS_CTX object of the given listening port
 */
CI_DECLARE_FUNC(SSL_CTX *) ci_openssl_port_SSL_CTX(ci_port_t *port);

#ifdef __cplusplus
}
#endif

#endif
