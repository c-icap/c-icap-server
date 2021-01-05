#ifndef __C_ICAP_NET_IO_SSL_H
#define __C_ICAP_NET_IO_SSL_H

#include "c-icap.h"
#include "net_io.h"
#ifdef USE_OPENSSL

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup TLS TLS/SSL related API
 \ingroup API
 * TLS/SSL related API.
 */

/**
 \ingroup TLS
 \brief Stores basic parameters for connecting to the remote TLS server.
 */
typedef struct ci_tls_client_options {
    /**
     \brief The TLS method to use.
     * Can be set to one of TLSv1_2, TLSv1_1, TLSv1, SSLv23, SSLv3
     */
    const char *method;

    /**
     \brief The path to a file stores the certificate
     */
    const char *cert;

    /**
     \brief The path to a file stores the certificate key
    */
    const char *key;

    /**
     \brief The ciphers list separated by ':'.
     * Read OpenSSL manuals for complete list of ciphers.
     */
    const char *ciphers;

    /**
     \brief Path to a file stores the CA certificates
     */
    const char *cafile;
    /**
     \brief Path to a directory where the CA certificates are stored
     */
    const char *capath;

    /**
     \brief Set to 1 if server certificate verification is required 0 otherwise.
     */
    int verify;

    /**
     \brief Please read SSL_CTX_set_options man page for available options
     */
    unsigned long options;
} ci_tls_client_options_t;

struct ci_tls_server_accept_details;
typedef struct ci_tls_server_accept_details ci_tls_server_accept_details_t;

typedef void * ci_tls_pcontext_t;

/*API functions for implementing TLS icap client*/

/**
 \ingroup TLS
 \brief Initializes c-icap tls subsystem. Normally called on programs startup.
 */
CI_DECLARE_FUNC(void) ci_tls_init();

/**
 \ingroup TLS
 \brief Deinitializes c-icap tls subsystem. Normally called on shutdown to
 *      clean-up.
 */
CI_DECLARE_FUNC(void) ci_tls_cleanup();

/**
 \ingroup TLS
 \brief Create a context based on given opts
 *
 * A context can be used to open more than one connections to a TLS server.
 */
CI_DECLARE_FUNC(ci_tls_pcontext_t) ci_tls_create_context(ci_tls_client_options_t *opts);

/**
 \ingroup TLS
 \brief Initializes and establishes a connection to a server.
 * \b Deprecated. Use ci_tls_connect_to_address instead
 * \n\b Warning: It tries only the first available ip address for the given name.
 \param servername The ip or dns name of the server
 \param p The port number to use
 \param proto One of AF_INET, AF_INET6
 \param ctx The context object to use
 \return NULL on failures the ci_connection_t object which can be used
 *       with various ci_connection_*  api functions on success.
 */
CI_DECLARE_FUNC(ci_connection_t*) ci_tls_connect(const char *servername, int port, int proto, ci_tls_pcontext_t ctx, int timeout);


/**
 \ingroup TLS
 \brief The non-blocking version of ci_tls_connect function
 *
 * \b Deprecated, use the ci_tls_connect_to_address_nonblock instead.
 * \n\b Warning: This function promises non-blocking but it uses the
 * getaddrinfo system call, which may block waiting a DNS answer.
 \return -1 on error, 1 when connection is established or 0 if should be
 *       called again.
 */
CI_DECLARE_FUNC(int) ci_tls_connect_nonblock(ci_connection_t *connection, const char *servername, int port, int proto, ci_tls_pcontext_t ctx);

/**
 \ingroup TLS
 \brief The non-blocking version of ci_tls_connect_to_address function
 *
 * To establish a connection required more than one calls to
 * ci_tls_connect_to_address_nonblock. The user should monitor the
 * connection->fd file descriptor for events in order to call again
 * ci_tls_connect_nonblock. If it is used with a custom monitor of file
 * descriptors event, it should be used with ci_connection_should_read_tls/
 * ci_connection_should_write_tls functions. In this case it should be used
 *  as follows:
 \code
           ci_connection_t *connection = ci_connection_create();
           int ret = ci_tls_connect_to_address_nonblock(connection, address, port, sni_name, use_ctx);
           while (ret == 0) {
               int wants_read = ci_connection_should_read_tls(connection);
               int wants_write = ci_connection_should_write_tls(connection);
               if (wants_read == 0 && wants_write == 0)
                   wants_write = 1;
               int mresult = monitor_fd(connection->fd,
                                        (wants_read > 0 ? MONITOR_FD_FOR_READ : 0),
                                        (wants_write > 0 ? MONITOR_FD_FOR_WRITE : 0)
                   );
               if (mresult == error) {
                   return error;
               }
               ret = ci_tls_connect_to_address_nonblock(connection, address, port, sni_name, use_ctx);
           }
           if (ret < 0)
               return error;
 \endcode
 */
CI_DECLARE_FUNC(int) ci_tls_connect_to_address_nonblock(ci_connection_t *connection, const ci_sockaddr_t *address, int port, const char *sni, ci_tls_pcontext_t use_ctx);

/**
 \ingroup TLS
 \brief Initializes and establishes a connection to a server.
 * This variant uses connecting timeout in seconds
 \param address The ip address of the server
 \param port The port number to use
 \param sni The server name to use
 \param ctx The context object to use
 \param secs Connect timeout in seconds
 \return NULL on failures the ci_connection_t object which can be used
 *       with various ci_connection_*  api functions on success.
 */
CI_DECLARE_FUNC(ci_connection_t *) ci_tls_connect_to_address(const ci_sockaddr_t *address, int port, const char *sni, ci_tls_pcontext_t use_ctx, int secs);

/**
 \ingroup TLS
 \copybrief ci_tls_connect_to_address
 * This variant uses connecting timeout in milliseconds
 \param address The ip address of the server
 \param port The port number to use
 \param sni The server name to use
 \param ctx The context object to use
 \param msecs Connect timeout in miliseconds
*/
CI_DECLARE_FUNC(ci_connection_t *) ci_tls_connect_ms_to_address(const ci_sockaddr_t *address, int port, const char *sni, ci_tls_pcontext_t use_ctx, int msecs);

/**
 \ingroup TLS
 \brief The TLS subsystem wants to read data from the connection
 \return -1 on non TLS connection or error, 1 if wants to read data, 0 otherwise
 */
CI_DECLARE_FUNC(int) ci_connection_should_read_tls(ci_connection_t *connection);

/**
 \ingroup TLS
 \brief The TLS subsystem wants to write data to the connection
 \return -1 on non TLS connection or error, 1 if wants to write data,
 *       0 otherwise
 */
CI_DECLARE_FUNC(int) ci_connection_should_write_tls(ci_connection_t *connection);

/**
 \ingroup TLS
 \brief There are pending bytes to read from TLS connection
 \return The number of pending bytes or 0
 */
CI_DECLARE_FUNC(int) ci_connection_read_pending_tls(ci_connection_t *conn);

/**
 \ingroup TLS
 \brief There are pending bytes to write to TLS connection
 \return The number of pending bytes or 0
 */
CI_DECLARE_FUNC(int) ci_connection_write_pending_tls(ci_connection_t *conn);

/*
  Functions needed to create an SSL server. Used by c-icap server.
*/
struct ci_port;
CI_DECLARE_FUNC(int) icap_init_server_tls(struct ci_port *port);
CI_DECLARE_FUNC(void) icap_close_server_tls(struct ci_port *port);
CI_DECLARE_FUNC(int) icap_port_tls_option(const char *opt, struct ci_port *conf, const char *config_dir);
CI_DECLARE_FUNC(int) icap_accept_tls_connection(struct ci_port *port, ci_connection_t *client_conn);
CI_DECLARE_FUNC(int) ci_port_reconfigure_tls(struct ci_port *port);
CI_DECLARE_FUNC(void) ci_tls_set_passphrase_script(const char *script);

/*
  Low level functions which not exported, but used internally by libicapapi.so library.
*/
int ci_connection_wait_ms_tls(ci_connection_t *conn, int msecs, int what_wait);
int ci_connection_wait_tls(ci_connection_t *conn, int secs, int what_wait);
int ci_connection_read_tls(ci_connection_t *conn, void *buf, size_t count, int timeout);
int ci_connection_write_tls(ci_connection_t *conn, const void *buf, size_t count, int timeout);
int ci_connection_read_nonblock_tls(ci_connection_t *conn, void *buf, size_t count);
int ci_connection_write_nonblock_tls(ci_connection_t *conn, const void *buf, size_t count);
int ci_connection_linger_close_tls(ci_connection_t *conn, int timeout);
int ci_connection_hard_close_tls(ci_connection_t *conn);


#ifdef __cplusplus
}
#endif

#endif
#endif /* NET_IO_SSL_H */
