/*
 *  Copyright (C) 2015 NetClean Technologies AB
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
#include "cfg_param.h"
#include "util.h"
#include "port.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <openssl/x509v3.h>

#include <errno.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <assert.h>

#include "debug.h"
#include "net_io.h"
#include "net_io_ssl.h"
#include "cfg_param.h"
#include "request.h"

/*The following include SSL_OP_ defines in an array*/
#include "openssl_options.c"

struct ci_tls_server_accept_details {
    SSL_CTX *tls_context;
    BIO* bio;
};

SSL_CTX *global_client_context = NULL;

ci_thread_mutex_t* g_openssl_mutexes = NULL;

#define _CI_CTX(ctx) ((SSL_CTX *)ctx)
#define _CI_CONN_BIO(conn) ((BIO *) (conn != NULL ? conn->tls_conn_pcontext : NULL))
#define _CI_CONN_SET_BIO(conn, bio) (conn->tls_conn_pcontext = (ci_tls_conn_pcontext_t)bio)

char *TLS_PASSPHRASE_SCRIPT = NULL;

void ci_tls_set_passphrase_script(const char *script)
{
    if (TLS_PASSPHRASE_SCRIPT) {
        free(TLS_PASSPHRASE_SCRIPT);
        TLS_PASSPHRASE_SCRIPT = NULL;
    }

    if (script)
        TLS_PASSPHRASE_SCRIPT = strdup(script);
}

static char *path_dup(const char *path, const char *config_dir)
{
    char fpath[CI_MAX_PATH];
    if (*path == '/')
        return strdup(path);

    snprintf(fpath, CI_MAX_PATH, "%s/%s", config_dir, path);
    fpath[CI_MAX_PATH -1] = '\0';
    return strdup(fpath);
}

static int openssl_option(const char *opt)
{
    int i;
    for (i = 0; OPENSSL_OPTS[i].name != NULL; ++i) {
        if (0 == strcmp(opt, OPENSSL_OPTS[i].name)) {
            ci_debug_printf(7, "OpenSSL option %s:0x%lx\n",OPENSSL_OPTS[i].name, OPENSSL_OPTS[i].value);
            return OPENSSL_OPTS[i].value;
        }
    }

    return 0;
}

static int parse_openssl_options(const char *str, long *options)
{
    char *stroptions = strdup(str);
    char *sopt, *next = NULL;
    long lopt;
    int negate;
    *options = SSL_OP_ALL;
    sopt = strtok_r(stroptions, "|", &next);
    while (sopt) {
        if (*sopt == '!') {
            negate = 1;
            sopt++;
        } else
            negate = 0;
        if (!(lopt = openssl_option(sopt))) {
            ci_debug_printf(1, "unknown tls option :%s\n", sopt);
            free(stroptions);
            return 0;
        }
        if (negate)
            *options ^= lopt;
        else
            *options |= lopt;
        sopt = strtok_r(NULL, "|", &next);
    }
    free(stroptions);
    return 1;
}

int icap_port_tls_option(const char *opt, ci_port_t *conf, const char *config_dir)
{
    /*
      TODO: Check for valid options!
     */
    if (strncmp(opt, "tls-method=", 11) == 0) {
        ci_debug_printf(1, "WARNING: tls-method= option is deprecated/removed, use SSL_OP_NO_TLS* options to disable one or more TLS protocol versions\n");
    } else if (strncmp(opt, "cert=", 5) == 0) {
        conf->tls_server_cert = path_dup(opt + 5, config_dir);
    } else if (strncmp(opt, "key=", 4) == 0) {
        conf->tls_server_key = path_dup(opt + 4, config_dir);
    } else if (strncmp(opt, "client_ca=", 10) == 0) {
        conf->tls_client_ca_certs = path_dup(opt + 10, config_dir);
    } else if (strncmp(opt, "cafile=", 7) == 0) {
        conf->tls_cafile = path_dup(opt + 7, config_dir);
    } else if (strncmp(opt, "capath=", 7) == 0) {
        conf->tls_capath = path_dup(opt + 7, config_dir);
    } else if (strncmp(opt, "ciphers=", 8) == 0) {
        conf->tls_ciphers = strdup(opt + 8);
    } else if (strncmp(opt, "tls-options=", 12) == 0) {
        if (!parse_openssl_options(opt+12, &conf->tls_options))
            return 0;
    } else
        return 0;

    return 1;
}

static int openssl_print_cb(const char *str, size_t len, void *u)
{
    ci_debug_printf(1, "%s\n", str);
    return 0;
}

static int set_linger(int sock, int secs_to_linger)
{
    int result = 0;

    struct linger linger = { 0 };
    linger.l_onoff = 1;
    linger.l_linger = secs_to_linger;
    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
        ci_debug_printf(1, "Unable to set sockopt linger.\n");
        result = 0;
    }

    return result;
}

static int openssl_verify_cert_cb (int ok, X509_STORE_CTX *ctx)
{
    if (ok == 0) {
        ci_debug_printf(1, "Peer cert verification failed: %s\n", X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)));
        return 0;
    }
    return 1;
}

static int openssl_cert_passphrase_cb(char *buf, int size, int rwflag, void *u)
{
    char script[65535];
    ci_port_t *port = (ci_port_t *)u;

    if (!TLS_PASSPHRASE_SCRIPT) {
        ci_debug_printf(1, "Certificate key has password but no TlsPassphrase script is defined!\n");
        return 0;
    }

    snprintf(script, sizeof(script), "%s %s %d", TLS_PASSPHRASE_SCRIPT, port->address ? port->address : "*", port->port);
    script[sizeof(script) - 1] = '\0';

    FILE *f = popen(script, "r");
    int bytes = fread(buf, 1, size, f);
    fclose(f);

    if (bytes <= 0) {
        ci_debug_printf(1, "Error reading key passphrase from '%s'\n", script);
        return 0;
    }

    buf[bytes] = '\0';
    return strlen(buf);
}

/*
 * Get the right TLS method for the given configuration string
 */
static const SSL_METHOD* get_tls_method(int b_for_server)
{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    return (b_for_server) ? TLS_server_method() : TLS_client_method();
#else
    return (b_for_server) ? SSLv23_server_method() : SSLv23_client_method();
#endif
}


/*
 * SSL callback function for locking
 */
#ifdef __GNUC__
#define __LOCAL_UNUSED __attribute__ ((__unused__))
#else
#define __LOCAL_UNUSED
#endif

__LOCAL_UNUSED static void openssl_locking_function(int mode, int n, const char* file, int line)
{
    if ( mode & CRYPTO_LOCK ) {
        ci_thread_mutex_lock(&g_openssl_mutexes[n]);
    } else {
        ci_thread_mutex_unlock(&g_openssl_mutexes[n]);
    }
}
/*
 * SSL callback function to identify the current thread
 */
__LOCAL_UNUSED static unsigned long openssl_id_function()
{
    return (unsigned long)ci_thread_self;
}
/*
 * Cleanup the OpenSSL mutexes
 */
static void cleanup_openssl_mutexes()
{
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    int i;
    if ( g_openssl_mutexes ) {
        for ( i = 0; i < CRYPTO_num_locks(); ++i ) {
            ci_thread_mutex_destroy(&g_openssl_mutexes[i]);
        }
        free(g_openssl_mutexes);
        g_openssl_mutexes = NULL;
    }
}

/*
 * Initialize the OpenSSL mutexes
 */
static int init_openssl_mutexes()
{
    int i;
    cleanup_openssl_mutexes();
    g_openssl_mutexes = malloc(sizeof(ci_thread_mutex_t) * CRYPTO_num_locks());
    if (!g_openssl_mutexes) {
        return 0;
    }
    memset(g_openssl_mutexes, 0, sizeof(ci_thread_mutex_t) * CRYPTO_num_locks());
    for ( i = 0; i < CRYPTO_num_locks(); ++i ) {
        if ( ci_thread_mutex_init(&g_openssl_mutexes[i]) != 0 ) {
            ci_debug_printf(1, "Failed to initialize mutex #%d for SSL\n", i);
            return 0;
        }
    }
    CRYPTO_set_id_callback(openssl_id_function);
    CRYPTO_set_locking_callback(openssl_locking_function);
    return 1;
}

static int OPENSSL_LOADED = 0;
void ci_tls_init()
{
    if (OPENSSL_LOADED)
        return;

    SSL_library_init();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();
    OpenSSL_add_all_algorithms();

    if (!init_openssl_mutexes()) {
        ci_debug_printf(1, "Failed to initialize locks for OpenSSL\n");
        return;
    }

    OPENSSL_LOADED = 1;
}

/*
 * Cleanup
 */
void ci_tls_cleanup()
{
    if (!OPENSSL_LOADED)
        return;

    if ( global_client_context ) {
        SSL_CTX_free(global_client_context);
        global_client_context = NULL;
    }
    cleanup_openssl_mutexes();
}

static SSL_CTX *create_server_context(ci_port_t *port)
{
    SSL_CTX *ctx;
    const SSL_METHOD* method = get_tls_method(1);
    if (method == NULL) {
        return 0;
    }

    if (!(ctx = SSL_CTX_new(method))) {
        ci_debug_printf(1, "Unable to create SSL_CTX object for SSL/TLS listening to: %s:%d\n", port->address, port->port);
        return NULL;
    }

    SSL_CTX_set_default_passwd_cb(ctx, openssl_cert_passphrase_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, port);

    if (port->tls_ciphers)
        SSL_CTX_set_cipher_list(ctx, port->tls_ciphers);

    SSL_CTX_load_verify_locations(ctx, port->tls_cafile ? port->tls_cafile : "root.pem", port->tls_capath);
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_use_certificate_chain_file(ctx, port->tls_server_cert);
    SSL_CTX_use_PrivateKey_file(ctx, port->tls_server_key ? port->tls_server_key : port->tls_server_cert, SSL_FILETYPE_PEM);

    if (port->tls_options)
        SSL_CTX_set_options(ctx, port->tls_options);

    if (port->tls_client_ca_certs) {
        // Set acceptable client certificate authorities
        SSL_CTX_set_client_CA_list(ctx,
                                   SSL_load_client_CA_file(port->tls_client_ca_certs));

        SSL_CTX_set_verify(ctx,
                           SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                           openssl_verify_cert_cb);
        SSL_CTX_set_verify_depth(ctx, 100);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, openssl_verify_cert_cb);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        unsigned long error = ERR_get_error();
        ci_debug_printf(1, "Unable to check private key: %lu:%s\n", error, ERR_error_string(error, NULL));
        SSL_CTX_free(ctx);
        return NULL;
    }
    ci_debug_printf(1, "SSL Keys Verified.\n");

    return ctx;
}

static int configure_openssl_bios(BIO *bio, SSL_CTX *ctx)
{
    BIO *secureBio = BIO_new_ssl(ctx, 0);
    SSL *ssl;
    BIO_get_ssl(secureBio, &ssl);
    if (!ssl) {
        ci_debug_printf(1, "Can't locate SSL pointer\n");
        return 0;
    }

    // We dont want to handle retries ourself
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_accept_bios(bio, secureBio);
    return 1;
}

static struct ci_tls_server_accept_details *ci_tls_server_accept_details_create()
{
    struct ci_tls_server_accept_details *acpt = malloc(sizeof(struct ci_tls_server_accept_details));
    if (!acpt) {
        ci_debug_printf(1, "Error allocating memory for accepting tls connections\n");
        return NULL;
    }

    acpt->tls_context = NULL;
    acpt->bio = NULL;
    return acpt;
}

static void ci_tls_server_accept_details_free(struct ci_tls_server_accept_details *acpt)
{
    if (!acpt)
        return;

    if (acpt->bio) {
        BIO_free_all(acpt->bio);
        acpt->bio = NULL;
    }

    if (acpt->tls_context) {
        SSL_CTX_free(acpt->tls_context);
        acpt->tls_context = NULL;
    }

    free(acpt);
}

int icap_init_server_tls(ci_port_t *port)
{
    ci_debug_printf(5, "icap_init_server_ssl\n");

    assert(port->tls_enabled);

    port->accept_socket = CI_SOCKET_INVALID;

    if (!port->tls_server_cert) {
        ci_debug_printf(1, "To use ssl please provide a server certificate in PEM format.\n");
        return 0;
    }

    struct ci_tls_server_accept_details *acpt = ci_tls_server_accept_details_create();
    if (!acpt)
        return 0;

    // Convert port
    char portString[128];
    snprintf(portString, sizeof(portString), "%s%s%d",
            (port->address ? port->address : ""),
            (port->address ? ":" : ""),
            port->port);

    // Setup socket
    acpt->bio = BIO_new_accept(portString);
    BIO_set_bind_mode(acpt->bio, BIO_BIND_REUSEADDR);
    BIO_set_nbio_accept(acpt->bio, 1);


    if (!(acpt->tls_context = create_server_context(port))) {
        ci_tls_server_accept_details_free(acpt);
        return 0;
    }

    if (!configure_openssl_bios(acpt->bio, acpt->tls_context)) {
        ci_tls_server_accept_details_free(acpt);
        return 0;
    }

    port->tls_accept_details = acpt;

    port->protocol_family = AF_INET; /* What about SSL over IPv6, is it supported by BIOs? */

    // Start to listen
    BIO_do_accept(acpt->bio);

    BIO_get_fd(acpt->bio, &port->accept_socket);
    set_linger(port->accept_socket, port->secs_to_linger);
    return 1;
}

void icap_close_server_tls(ci_port_t *port)
{
    if (port->tls_accept_details) {
        ci_tls_server_accept_details_free(port->tls_accept_details);
        port->tls_accept_details = NULL;
    }
}

int ci_port_reconfigure_tls(ci_port_t *port)
{
    struct ci_tls_server_accept_details *acpt = port->tls_accept_details;
    assert(port->tls_enabled);
    assert(acpt);
    assert(acpt->bio);
    SSL_CTX *old_ctx = acpt->tls_context;
    if (!(acpt->tls_context = create_server_context(port)))
        return 0;

    if (!configure_openssl_bios(acpt->bio, acpt->tls_context))
        return 0;

    SSL_CTX_free(old_ctx);
    return 1;
}

int ci_connection_wait_tls(ci_connection_t *conn, int secs, int what_wait)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    if (!ci_socket_valid(conn->fd) || !conn_bio)
        return -1;

    //TODO: remove the following block
    {
        int fd = -1;
        BIO_get_fd(conn_bio, &fd);
        assert(fd == conn->fd);
    }

    int ret = 0;
    if ((what_wait & ci_wait_for_read) && BIO_pending(conn_bio)) {
        ret |= ci_wait_for_read;
        /* Maybe if (what_wait & ci_wait_for_write) is true run a nonblocking
           ci_wait_for_data to check if we are able to write data to */
        return ret;
    }

    int bio_wait = (BIO_should_read(conn_bio) ? ci_wait_for_read : 0) |
                   (BIO_should_write(conn_bio) ? ci_wait_for_write : 0);
    if (bio_wait == 0)
        bio_wait = what_wait;
    return ci_wait_for_data(conn->fd, secs, bio_wait);
}

int ci_connection_read_tls(ci_connection_t *conn, void *buf, size_t count, int timeout)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    assert(conn_bio);

    int bytes = BIO_read(conn_bio, buf, count);

    if (bytes <= 0 && BIO_should_retry(conn_bio)) {
        if (ci_connection_wait_tls(conn, timeout, ci_wait_for_read) == 0) {
            bytes = BIO_read(conn_bio, buf, count);
        }
    }
    return bytes;
}

int ci_connection_write_tls(ci_connection_t *conn, const void *buf, size_t count, int timeout)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    assert(conn_bio);

    int bytes = 0;
    int remains = count;
    char *b = (char *) buf;
    while (remains) {
        bytes = BIO_write(conn_bio, b, remains);
        if (bytes <= 0 && BIO_should_retry(conn_bio)) {
            if (ci_connection_wait_tls(conn, timeout, ci_wait_for_write) <= 0) {
                /*timeout without action or error*/
                return bytes;
            }
            continue;
        }

        if (bytes > 0) {
            b += bytes;
            remains -= bytes;
        } else {
            break;
        }
    }

    return bytes;
}

int ci_connection_read_nonblock_tls(ci_connection_t *conn, void *buf, size_t count)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    assert(conn_bio);
    int bytes = BIO_read(conn_bio, buf, count);
    if (bytes <= 0)
        return BIO_should_retry(conn_bio) ? 0 : -1;
    return bytes;
}

int ci_connection_write_nonblock_tls(ci_connection_t *conn, const void *buf, size_t count)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    assert(conn_bio);
    int bytes = BIO_write(conn_bio, buf, count);
    if (bytes <= 0)
        return BIO_should_retry(conn_bio) ? 0 : -1;
    return bytes;
}

int ci_connection_hard_close_tls(ci_connection_t *conn)
{
    BIO *conn_bio = _CI_CONN_BIO(conn);
    assert(conn_bio);
    BIO_free_all(conn_bio);
    conn->tls_conn_pcontext = NULL;
    conn->fd = CI_SOCKET_INVALID;
    return 1;
}

int ci_connection_linger_close_tls(ci_connection_t *conn, int timeout)
{
    set_linger(conn->fd, timeout);
    return ci_connection_hard_close_tls(conn);
}

int icap_accept_tls_connection(ci_port_t *port, ci_connection_t *client_conn)
{
    int ret = BIO_do_accept(port->tls_accept_details->bio);
    if (ret <= 0) {
        ERR_print_errors_cb(openssl_print_cb, NULL);
        ci_debug_printf(1, "Error accepting connection: %d.\n", ret);
        client_conn->fd = CI_SOCKET_INVALID;
        return -2;
    }

    assert(client_conn && client_conn->tls_conn_pcontext == NULL);
    BIO *client_conn_bio = BIO_pop(port->tls_accept_details->bio);
    _CI_CONN_SET_BIO(client_conn, client_conn_bio);

    // Check if this is a ssl connection
    SSL *ssl = NULL;
    BIO_get_ssl(client_conn_bio, &ssl);
    if (ssl) {
        int handshakeResult = BIO_do_handshake(client_conn_bio);
        int sslErrorCode = SSL_get_error(ssl, handshakeResult);

        if (sslErrorCode != SSL_ERROR_NONE) {
            /*Unknown protocol???*/
            ERR_print_errors_cb(openssl_print_cb, NULL);
            BIO_free_all(client_conn_bio);
            client_conn_bio = NULL;
            client_conn->fd = CI_SOCKET_INVALID;
            return -1;
        }
    }

    BIO_set_nbio(client_conn_bio, 1);
    BIO_get_fd(client_conn_bio, &client_conn->fd);
    ci_debug_printf(8, "SSL connection FD: %d\n", client_conn->fd);
    /*We need to compute remote client address*/
    ci_connection_init(client_conn, ci_connection_server_side);
    return 1;
}

#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER <= 0x1000201fL
static int check_hostname(const char *servername, ASN1_STRING *check)
{
    const char *cname;
    const char *c, *s;
    int clength;

    if (!servername || !check)
        return -1;

    cname = (char *) ASN1_STRING_data(check);
    clength =  ASN1_STRING_length(check);
    c = cname;
    if (*c == '*') {
        /*For now match only wildcard certificats in the form *.domain.com */
        c++;
        if (*c != '.')
            return -1;
        if ((s = strchr(servername, '.')) == NULL)
            s = servername;
    } else
        s = servername;

    while ( *s && (c - cname)  < clength) {
        if (*s != *c)
            return 1;
        s++, c++;
    }

    /* Full match*/
    return 0;
}

static int match_X509_names(X509 *cert, const char *servername)
{
    X509_NAME *name = X509_get_subject_name(cert);

    int indx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
    ASN1_STRING *cn_data = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name, indx));

    if (check_hostname(servername, cn_data) == 0)
        return 1;

    STACK_OF(GENERAL_NAME) * altnames;
    altnames = (STACK_OF(GENERAL_NAME)*)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    if (altnames) {
        int numalts = sk_GENERAL_NAME_num(altnames);
        for (indx = 0; indx < numalts; ++indx) {
            const GENERAL_NAME *check = sk_GENERAL_NAME_value(altnames, indx);
            if (check->type == GEN_DNS) {
                ASN1_STRING *cn_data = check->d.dNSName;

                if (check_hostname(servername, cn_data) == 0) {
                    sk_GENERAL_NAME_pop_free(altnames, GENERAL_NAME_free);
                    return 1;
                }
            }
        }
        sk_GENERAL_NAME_pop_free(altnames, GENERAL_NAME_free);
    }
    return 0;

}
#endif

ci_tls_pcontext_t ci_tls_create_context(ci_tls_client_options_t *opts)
{
    SSL_CTX *ctx;
    const SSL_METHOD *method = get_tls_method(0);

    if (!method) {
        ci_debug_printf(1, "Enable to get a valid supported SSL method (%s does not exist?)\n", opts ? opts->method : "-");
        return (ci_tls_pcontext_t)NULL;
    }

    ctx = SSL_CTX_new(method);
    if (!opts || opts->verify) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, openssl_verify_cert_cb);
        SSL_CTX_set_default_verify_paths(ctx);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, openssl_verify_cert_cb);
    }

    if (opts) {
        if (opts->ciphers)
            SSL_CTX_set_cipher_list(ctx, opts->ciphers);

        if (opts->cafile || opts->capath)
            SSL_CTX_load_verify_locations(ctx, opts->cafile, opts->capath);

        if (opts->cert) {
            SSL_CTX_use_certificate_chain_file(ctx, opts->cert);
            SSL_CTX_use_PrivateKey_file(ctx, opts->key ? opts->key : opts->cert, SSL_FILETYPE_PEM);
        }
        if (opts->options)
            SSL_CTX_set_options(ctx, opts->options);
    }

    return (ci_tls_pcontext_t)ctx;
}

int ci_tls_connect_nonblock(ci_connection_t *connection, const char *servername, int port, int proto, ci_tls_pcontext_t use_ctx)
{
    char buf[512];
    char hostname[CI_MAXHOSTNAMELEN + 1];
    SSL *ssl = NULL;

    assert(connection);
    BIO *connection_bio = _CI_CONN_BIO(connection);
    if (!connection_bio) {

        if (!ci_host_to_sockaddr_t(servername, &(connection->srvaddr), proto)) {
            ci_debug_printf(1, "Error getting address info for host '%s'\n",
                            servername);
            return -1;
        }
        ci_sockaddr_set_port(&(connection->srvaddr), port);

        SSL_CTX *ctx = NULL;
        if (use_ctx)
            ctx = _CI_CTX(use_ctx);
        else {
            if ( global_client_context ) {
                SSL_CTX_free(global_client_context);
            }
            ci_tls_client_options_t opts;
            memset((void *)&opts, 0, sizeof(ci_tls_client_options_t));
            opts.verify = 1;
            /*We need to store to global_client_context to release ctx after exit*/
            global_client_context = ci_tls_create_context(&opts);
            ctx = global_client_context;
        }

        if (ctx == NULL) {
            ci_debug_printf(1, "Failed to create SSL context\n");
            return -1;
        }
        /*SSL_new increases the reference count for ctx*/
        ssl = SSL_new(ctx);

#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x1000201fL
        if (SSL_CTX_get_verify_mode(ctx) & SSL_VERIFY_PEER) {
            X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
            /* Enable automatic hostname checks */
            X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            X509_VERIFY_PARAM_set1_host(param, servername, 0);
        }
#else
        // Implement
#endif

        SSL_set_connect_state(ssl);

        BIO* ssl_bio = BIO_new(BIO_f_ssl());
        BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);

#if defined(SSL_CTRL_SET_TLSEXT_HOSTNAME)
        SSL_set_tlsext_host_name(ssl, servername);
#endif

        snprintf(buf, sizeof(buf), "%d", port);

        connection_bio = BIO_new(BIO_s_connect());
        BIO_set_conn_port(connection_bio, buf);
        BIO_set_conn_hostname(connection_bio, servername);
        BIO_set_nbio(connection_bio, 1);

        connection_bio = BIO_push(ssl_bio, connection_bio);
        _CI_CONN_SET_BIO(connection, connection_bio);
    } else { /*if (connection_bio != NULL)*/
        BIO_get_ssl(connection_bio, &ssl);
        if (!ssl) {
            ci_debug_printf(1, "Error connecting to host  '%s': Missing SSL object\n", hostname);
            return -1;
        }
    }

    int ret = BIO_do_connect(connection_bio);
    if (!ci_socket_valid(connection->fd))
        BIO_get_fd(connection_bio, &connection->fd);

    if (ret <= 0) {
        if (BIO_should_retry(connection_bio))
            return 0;
        ci_sockaddr_t_to_host(&(connection->srvaddr), hostname, CI_MAXHOSTNAMELEN);
        ci_debug_printf(1, "Error connecting to host  '%s': %s \n",
                        hostname,
                        ERR_error_string(ERR_get_error(), buf));
        return -1;
    }

#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER <= 0x1000201fL
    if (SSL_get_verify_mode(ssl) & SSL_VERIFY_PEER) {
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (!match_X509_names(cert, servername)) {
            ci_debug_printf(1, "Error: server certificate subject does not match\n");
            return -1;
        }
    }
#endif

    if (!ci_connection_init(connection, ci_connection_client_side)) {
        ci_debug_printf(1, "Error getting client sockname: %s\n",
                        ci_strerror(errno,  buf, sizeof(buf)));
        return -1;
    }
    connection->flags |= CI_CONNECTION_CONNECTED;

    return 1;
}

ci_connection_t *ci_tls_connect(const char *servername, int port, int proto, ci_tls_pcontext_t use_ctx, int timeout)
{
    ci_connection_t *connection = ci_connection_create();
    if (!connection)
        return NULL;

    int ret = ci_tls_connect_nonblock(connection, servername, port, proto, use_ctx);
    while (ret == 0) {
        do {
            ret = ci_connection_wait_tls(connection, timeout, ci_wait_for_write);
        } while (ret > 0 && (ret & ci_wait_should_retry)); //while iterrupted by signal

        if (ret > 0)
            ret = ci_tls_connect_nonblock(connection, servername, port, proto, use_ctx);
    }

    if (ret < 0) {
        ci_debug_printf(1, "Connection to '%s:%d' failed/timedout\n",
                        servername, port);
        ci_connection_destroy(connection);
        return NULL;
    }

    return connection;
}

int ci_connection_should_read_tls(ci_connection_t *connection)
{
    BIO *connection_bio = _CI_CONN_BIO(connection);
    if (!connection_bio)
        return -1;

    return (BIO_should_read(connection_bio) | BIO_should_io_special(connection_bio) ? 1 : 0);
}

int ci_connection_should_write_tls(ci_connection_t *connection)
{
    BIO *connection_bio = _CI_CONN_BIO(connection);
    if (!connection_bio)
        return -1;

    return (BIO_should_write(connection_bio) | BIO_should_io_special(connection_bio) ? 1 : 0);
}

int ci_connection_read_pending_tls(ci_connection_t *connection)
{
    BIO *connection_bio = _CI_CONN_BIO(connection);
    if (!connection_bio)
        return 0;

    return BIO_pending(connection_bio);
}

int ci_connection_write_pending_tls(ci_connection_t *connection)
{
    BIO *connection_bio = _CI_CONN_BIO(connection);
    if (!connection_bio)
        return 0;

    return BIO_wpending(connection_bio);
}

BIO * ci_openssl_connection_BIO(ci_connection_t *conn)
{
    return _CI_CONN_BIO(conn);
}

SSL * ci_openssl_connection_SSL(ci_connection_t *conn)
{
    SSL *ssl = NULL;
    BIO *conn_bio = _CI_CONN_BIO(conn);
    if (conn_bio)
        BIO_get_ssl(conn_bio, &ssl);
    return ssl;
}

SSL_CTX * ci_openssl_connection_SSL_CTX(ci_connection_t *conn)
{
    SSL *ssl = NULL;
    BIO *conn_bio = _CI_CONN_BIO(conn);
    if (conn_bio)
        BIO_get_ssl(conn_bio, &ssl);
    return ssl ? SSL_get_SSL_CTX(ssl) : NULL;
}

SSL_CTX * ci_openssl_port_SSL_CTX(ci_port_t *port)
{
    if (port->tls_accept_details)
        return port->tls_accept_details->tls_context;
    return NULL;
}

SSL_CTX * ci_openssl_context_SSL_CTX(ci_tls_pcontext_t pcontext)
{
    return _CI_CTX(pcontext);
}
