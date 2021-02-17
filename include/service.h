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


#ifndef __SERVICE_H
#define __SERVICE_H

#include "header.h"
#include "cfg_param.h"
#include "ci_threads.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup SERVICES Services API
 \ingroup API
 * Services related API. For detailed information about implementing a service
 * look the documentation of struct ci_service_module
 */

#define CI_MOD_NOT_READY  0
#define CI_MOD_DONE       1
#define CI_MOD_CONTINUE 100
#define CI_MOD_ALLOW204 204
#define CI_MOD_ALLOW206 206
#define CI_MOD_ERROR     -1

#define MAX_SERVICE_NAME  63
#define MAX_SERVICE_ARGS 255

#define CI_ISTAG_SIZE 32
#define SERVICE_ISTAG_SIZE 26 /* Deprecated */
#define CI_SERVICE_ISTAG_SIZE 26
#define XINCLUDES_SIZE    511 /* it is enough I think ....*/

#define CI_XCLIENTIP              1
#define CI_XSERVERIP              2
#define CI_XSUBSCRIBERID          4
#define CI_XAUTHENTICATEDUSER     8
#define CI_XAUTHENTICATEDGROUPS  16

struct ci_request;
struct ci_list;

typedef struct  ci_service_module ci_service_module_t;

enum SERVICE_STATUS {CI_SERVICE_NOT_INITIALIZED = -1,
                     CI_SERVICE_OK = 0,
                     CI_SERVICE_ERROR = 1
                    };

/*For internal use only*/
struct ci_option_handler {
    char name[64];
    int (*handler)(struct ci_request *);
};


/**
 \typedef  ci_service_xdata_t
 \ingroup SERVICES
 *Stores required data and settings for a service
 */
typedef struct ci_service_xdata {
    ci_thread_rwlock_t lock;
    int status;
    struct ci_conf_entry *intl_srv_conf_table;
    uint64_t xopts;
    char ISTag[sizeof("ISTag: \"") + CI_ISTAG_SIZE + sizeof("\"") + 1];
    char xincludes[XINCLUDES_SIZE+1];
    char TransferPreview[MAX_HEADER_SIZE+1];
    char TransferIgnore[MAX_HEADER_SIZE+1];
    char TransferComplete[MAX_HEADER_SIZE+1];
    int preview_size;
    int max_connections;
    int options_ttl;
    int allow_204;
    int allow_206;
    int disable_206; /*even if service support it do not use 206*/
    struct ci_list *option_handlers;
    /*statistics IDS*/
    int stat_bytes_in;
    int stat_bytes_out;
    int stat_http_bytes_in;
    int stat_http_bytes_out;
    int stat_body_bytes_in;
    int stat_body_bytes_out;
    int stat_reqmods;
    int stat_respmods;
    int stat_options;
    int stat_allow204;
} ci_service_xdata_t;

/**
 \ingroup SERVICES
 * Is the structure  which implements a service
 *
 * To implement a service someones needs to implement the member functions of
 * this struct. These functions will be called by c-icap as follows:
 *   - When a new request arrives for this service then the
 *     ci_service_module::mod_init_request_data is called
 *   - When the icap client sends preview data then the
 *     ci_service_module::mod_check_preview_handler is called.
 *     If this function return CI_MOD_ALLOW204 the ICAP transaction stops here.
 *     If this function return CI_MOD_CONTINUE the ICAP client will send the
 *     rest body data if exists.
 *   - When he client starts sends more data then the
 *     ci_service_module::mod_service_io is called multiple times
 *     untill the client has send all the body data. The service
 *     can start send data using this function to the client
 *     before all data received
 *   - When the client finishes sending body data the
 *     ci_service_module::mod_end_of_data_handler is called
 *   - While the icap client waits to read the body data from
 *     the c-icap then the ci_service_module::mod_service_io
 *     is called multiple times until all the body data sent to
 *     the client
 */
struct  ci_service_module {

    /**
     \example services/echo/srv_echo.c
     \ingroup SERVICES
     \brief The srv_echo.c is an example service implementation,
     *       which does not modifies the content
     *
     */
    /**
      \brief The service name
     */
    const char *mod_name;

    /**
      \brief Service short description
     */
    const char *mod_short_descr;

    /**
     \brief Service type
     *
     * The service type can be ICAP_RESPMOD for a responce modification
     * service, ICAP_REQMOD for request modification service or
     * ICAP_RESPMOD|ICAP_REQMOD for a service implements both response and
     * request modification
     */
    int  mod_type;

    /**
       \brief Pointer to the function called when the service loaded.
     *
     * This function called exactly when the service loaded by c-icap. Can
     * be used to initialize the service.
     \param srv_xdata Pointer to the ci_service_xdata_t object of this service
     \param server_conf Pointer to the struct holds the main c-icap server
            configuration
     \return CI_OK on success, CI_ERROR on any error.
     */
    int (*mod_init_service)(ci_service_xdata_t *srv_xdata,struct ci_server_conf *server_conf);

    /**
       \brief Pointer to the function which called after the c-icap initialized,
     * but before the c-icap start serves requests.
     *
     * This function can be used to initialize the service. Unlike to the
     * ci_service_module::mod_init_service, when this function called the
     * c-icap has initialized and it is known system parameters like the
     * services and modules which are loaded, network ports and addresses
     * the c-icap is listening to, etc.
     \param srv_xdata Pointer to the ci_service_xadata_t object of this service
     \param server_conf Pointer to the struct holds the main c-icap server
            configuration
     \return CI_OK on success, CI_ERROR on errors.
     */
    int (*mod_post_init_service)(ci_service_xdata_t *srv_xdata,struct ci_server_conf *server_conf);

    /**
     \brief Pointer to the function which called on c-icap server shutdown
     *
     * This function can be used to release service allocated resources
     */
    void (*mod_close_service)();

    /**
     \brief Pointer to the function called when a new request for this services
     * arrives to c-icap server.
     *
     * This function should inititalize the data and structures required for
     * serving the request.
     *
     \param req a pointer to the related ci_request_t structure
     \return a void pointer to the "service data", the user defined data
     *       required for serving the
     *       request.The developer can obtain the service data from the
     *       related ci_request_t object using the macro ci_service_data
     */
    void *(*mod_init_request_data)(struct ci_request *req);

    /**
     \brief Pointer to the function which releases the service data.
     *
     * This function called after the user request served to release the
     * service data
     \param srv_data pointer to the service data returned by the
     *      ci_service_module::mod_init_request_data call
     */
    void (*mod_release_request_data)(void *srv_data);

    /**
     \brief Pointer to the function which is used to preview the ICAP client
     *      request
     *
     * The client if supports preview sends some data for examination.
     * The service using this function will decide if the client request must
     * processed so the client must send more data or no processing is needed
     * so the request ended here.
     \param preview_data Pointer to the preview data
     \param preview_data_len The size of preview data
     \param req Pointer to the related ci_request struct
     \return CI_MOD_CONTINUE if the client must send more data, CI_MOD_ALLOW204
     * if the service does not want to modify anything, or CI_ERROR on errors.
     */
    int (*mod_check_preview_handler)(char *preview_data,int preview_data_len,struct ci_request *req);

    /**
     \brief Pointer to the function called when the icap client has send all the data to the service
     *
     *This function called when the ICAP client has send all data.
     \param req pointer to the related ci_request struct
     \return CI_MOD_DONE if all are OK, CI_MOD_ALLOW204 if the ICAP client
     *       request supports 204 responses and we are not planning to modify
     *       anything, or CI_ERROR on errors.
     *       The service must not return CI_MOD_ALLOW204 if has already send
     *       some data to the client, or when the client does not support
     *       allow204 responses. To examine if client supports 204 responses
     *       the ci_req_allow204 macro can be used
     */
    int (*mod_end_of_data_handler)(struct ci_request *req);

    /**
     \brief Pointer to the function called to read/send body data from/to
     *      icap client.
     *
     * This function reads body data from the ICAP client and sends back the
     * modified body data. To allow c-icap send data to the ICAP client before
     * all data received by the c-icap, a call to the ci_req_unlock_data
     * function is required.
     \param wbuf The buffer for writing data to the ICAP client
     \param wlen The size of the write buffer. It must modified to be the size
     *      of writing data. If the service has send all the data to the
     *      client, this parameter must set to CI_EOF.
     \param rbuf Pointer to the data read from the ICAP client
     \param rlen The lenght of the data read from the ICAP client. If this
     *      function for a reason can not read all the data, it must modify
     *      the rlen to be equal to the read data
     \param iseof It has non zero value if the data in rbuf buffer are the
     *      last data from the ICAP client.
     \param req pointer to the related ci_request struct
     \return Return CI_OK if all are OK or CI_ERROR on errors
     */
    int (*mod_service_io)(char *wbuf,int *wlen,char *rbuf,int *rlen,int iseof, struct ci_request *req);

    /**
     \brief Pointer to the config table of the service
     *
     * Is an array which contains the definitions of configuration parameters
     * used by the service. The configuration parameters defined in this array
     * can be used in c-icap.conf file.
     */
    struct ci_conf_entry *mod_conf_table;

    /**
     \brief NULL pointer
     *
     * This field does not used. Set it to NULL.
     */
    void *mod_data;
};

typedef struct service_alias {
    char alias[MAX_SERVICE_NAME+1];
    char args[MAX_SERVICE_ARGS+1];
    ci_service_module_t *service;
} service_alias_t;

/*Internal function */
ci_service_module_t *add_service(ci_service_module_t *service);
ci_service_module_t *register_service(const char *module_file, const char *argv[]);

service_alias_t *add_service_alias(const char *service_alias, const char *service_name,const char *args);
ci_service_module_t *find_service(const char *service_name);
service_alias_t *find_service_alias(const char *service_name);
ci_service_xdata_t *service_data(ci_service_module_t *srv);
int init_services();
int post_init_services();
int release_services();
int run_services_option_handlers(ci_service_xdata_t *srv_xdata, struct ci_request *req);

/*Library functions */
/*Undocumented, are not usefull to users*/
CI_DECLARE_FUNC(void) ci_service_data_read_lock(ci_service_xdata_t *srv_xdata);
CI_DECLARE_FUNC(void) ci_service_data_read_unlock(ci_service_xdata_t *srv_xdata);

/**
 \ingroup SERVICES
 \brief Sets the ISTAG for the service.
 *
 *Normally this function called in ci_service_module::mod_init_service()  or
 * ci_service_module::mod_post_init_service() function, while the service
 * initialization.
 \param srv_xdata is a pointer to the c-icap internal service data.
 \param istag is a string contains the new ISTAG for the service. The istag
 *      size can not be more than a size of SERVICE_ISTAG_SIZE. If the length
 *      of istag is greater than SERVICE_ISTAG_SIZE the extra bytes are ignored.
 */
CI_DECLARE_FUNC(void) ci_service_set_istag(ci_service_xdata_t *srv_xdata, const char *istag);

/**
 \ingroup SERVICES
 \brief Sets the service x-headers mask which defines the X-Headers supported
 *      by the service.The c-icap server will advertise these headers in
 *      options responses.
 *
 * Normally this function called in ci_service_module::mod_init_service()  or
 * ci_service_module::mod_post_init_service() function, while the service
 * is initialized.
 \param srv_xdata is a pointer to the c-icap internal service data.
 \param xopts is a compination of one or more of the following defines:
 * - CI_XCLIENTIP: Refers to the X-Client-IP header. The HTTP proxy (or the
 *   ICAP client) will sends  the ip address of the HTTP client using this
 *   header if supports this header.
 * - CI_XSERVERIP: The X-Server-IP header. The HTTP proxy will incluse the IP
 *   of the HTTP destination host in the X-Server-IP header if supports this
 *   header.
 * - CI_XSUBSCRIBERID: The X-Subscriber-ID header. This header can include a
 *   unique subscriber ID of the user who issued the HTTP request
 * - CI_XAUTHENTICATEDUSER: The X-Authenticated-User header. If the user
 *   authenticated on  HTTP proxy side includes the username
 * - CI_XAUTHENTICATEDGROUPS: The X-Authenticated-Group header. If the user is
 *   authenticated on HTTP proxy side includes the user groups
 *
 * example usage:
 \code
 * ci_service_set_xopts(srv_xdata,CI_XCLIENTIP|CI_XAUTHENTICATEDUSER);
 \endcode
 *
 * For more informations about ICAP common X-Headers look at:
 * http://www.icap-forum.org/documents/specification/draft-stecher-icap-subid-00.txt
 */
CI_DECLARE_FUNC(void) ci_service_set_xopts(ci_service_xdata_t *srv_xdata, uint64_t xopts);

/**
 \ingroup SERVICES
 \brief it is similar to the function ci_service_set_xopts but just adds
 *      (not sets) the X-Headers defined by the xopts parameter to the
 *      existing x-headers mask of service.
 *
 */
CI_DECLARE_FUNC(void) ci_service_add_xopts(ci_service_xdata_t *srv_xdata, uint64_t xopts);

/**
 \ingroup SERVICES
 \brief Set the list of file extensions that should previewed by the service.
 *
 * The c-icap will inform the ICAP client that should send preview data for the
 * files which have the extensions contained in the preview string. The
 * wildcard value "*" specifies all files extensions, which is the default.
 \param srv_xdata is a pointer to the c-icap internal service data.
 \param preview is the string which contains the list of the file extensions.
 *
 * example usage:
 \code
 *   ci_service_set_transfer_preview(srv_xdata,"zip, tar");
 \endcode
 */
CI_DECLARE_FUNC(void) ci_service_set_transfer_preview(ci_service_xdata_t *srv_xdata,const char *preview);

/**
 \ingroup SERVICES
 \brief Set the list of file extensions that should NOT be send for this
 *      service.
 *
 * The c-icap will inform the ICAP client that should not send files which
 * have the extensions contained in the ignore string.
 \param srv_xdata is a pointer to the c-icap internal service data.
 \param ignore is the string which contains the list of the file extensions.
 *
 * example usage:
 \code
 *   ci_service_set_transfer_ignore(srv_xdata,"gif, jpeg");
 \endcode
 */
CI_DECLARE_FUNC(void) ci_service_set_transfer_ignore(ci_service_xdata_t *srv_xdata, const char *ignore);

/**
 \ingroup SERVICES
 \brief Set the list of file extensions that should be send in their entirety
 *      (without preview) to this service.
 *
 * The c-icap will inform the ICAP client that should send files which have
 * the extensions contained in the complete string, in their entirety to this
 * service.
 \param srv_xdata is a pointer to the c-icap internal service data.
 \param complete is the string which contains the list of the file extensions.
 *
 *example usage:
 \code
 *   ci_service_set_transfer_complete(srv_xdata,"exe, bat, com, ole");
 \endcode
 */
CI_DECLARE_FUNC(void) ci_service_set_transfer_complete(ci_service_xdata_t *srv_xdata, const char *complete);

/**
  \ingroup SERVICES
  \brief Sets the maximum preview size supported by this service
  *
  \param srv_xdata is a pointer to the c-icap internal service data.
  \param preview is the size of preview data supported by this service
 */
CI_DECLARE_FUNC(void) ci_service_set_preview(ci_service_xdata_t *srv_xdata, int preview);


/**
  \ingroup SERVICES
  \brief  Enable the allow 204 responses for this service.
  *
  * The service will supports the allow 204 responses if the icap client
  * support it too.
  \param srv_xdata is a pointer to the c-icap internal service data.
 */
CI_DECLARE_FUNC(void) ci_service_enable_204(ci_service_xdata_t *srv_xdata);

/**
  \ingroup SERVICES
  \brief  Enable the Partial Content 206 responses for this service.
  *
  * The service will supports the Partial Content 206 responses if the icap
  * client support it too.
  \param srv_xdata is a pointer to the c-icap internal service data.
 */
CI_DECLARE_FUNC(void) ci_service_enable_206(ci_service_xdata_t *srv_xdata);

/**
  \ingroup SERVICES
  \brief Sets the maximum connection should opened by icap client to the c-icap
  *      for this service
  *
  \param srv_xdata is a pointer to the c-icap internal service data.
  \param max_connections is the maximum connections
 */
CI_DECLARE_FUNC(void) ci_service_set_max_connections(ci_service_xdata_t *srv_xdata, int max_connections);

/**
  \ingroup SERVICES
  \brief Sets the Options ttl for this service
  *
  \param srv_xdata is a pointer to the c-icap internal service data.
  \param ttl is the ttl value in seconds
 */
CI_DECLARE_FUNC(void) ci_service_set_options_ttl(ci_service_xdata_t *srv_xdata, int ttl);

CI_DECLARE_FUNC(void) ci_service_add_xincludes(ci_service_xdata_t *srv_xdata, char **xincludes);

/**
  \ingroup SERVICES
  \brief Add a service handler for the service
  *
  * Normally this function called in ci_service_module::mod_init_service()  or
  * ci_service_module::mod_post_init_service() function.
  * The options handlers are running when the service receives an OPTIONS
  * request to check for service health. They can add ICAP headers to the
  * OPTIONS response and must return CI_OK on success, or CI_ERROR on failure.
  * If one or more handlers failed the c-icap will produce a "500 Server Error"
  * response.
  *
  \param srv_xdata is a pointer to the c-icap internal service data.
  \param name a name for the handler, used for debuging reasons
  \param handler the handler
*/
CI_DECLARE_FUNC(void) ci_service_add_option_handler(ci_service_xdata_t *srv_xdata, const char *name, int (*handler)(struct ci_request *));

#ifdef __CI_COMPAT
#define service_module_t ci_service_module_t
#define service_extra_data_t ci_service_xdata_t

/*The Old CI_X* defines*/
#define CI_XClientIP              1
#define CI_XServerIP              2
#define CI_XSubscriberID          4
#define CI_XAuthenticatedUser     8
#define CI_XAuthenticatedGroups  16

#endif

#ifdef __cplusplus
}
#endif

#endif
