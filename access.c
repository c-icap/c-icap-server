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
#include "request.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include "access.h"
#include "simple_api.h"
#include "net_io.h"


/*********************************************************************************************/
/* Default Authenticator  definitions                                                        */
int default_acl_init(struct ci_server_conf *server_conf);
int default_acl_post_init(struct ci_server_conf *server_conf);
void default_acl_release();
int default_acl_client_access(ci_sockaddr_t * client_address,
                              ci_sockaddr_t * server_address);
int default_acl_request_access(char *dec_user, char *service, int req_type,
                               ci_sockaddr_t * client_address,
                               ci_sockaddr_t * server_address);
int default_acl_http_request_access(char *dec_user, char *service, int req_type,
                                    ci_sockaddr_t * client_address,
                                    ci_sockaddr_t * server_address);
int default_acl_log_access(char *dec_user, char *service, int req_type,
                           ci_sockaddr_t * client_address,
                           ci_sockaddr_t * server_address);



int cfg_acl_add(char *directive, char **argv, void *setdata);
int cfg_acl_access(char *directive, char **argv, void *setdata);



/*Configuration Table .....*/
static struct ci_conf_entry acl_conf_variables[] = {
     {"acl", NULL, cfg_acl_add, NULL},
     {"icap_access", NULL, cfg_acl_access, NULL},
     {NULL, NULL, NULL, NULL}
};


access_control_module_t default_acl = {
     "default_acl",
     default_acl_init,
     default_acl_post_init,     /*post_init */
     default_acl_release,
     default_acl_client_access,
     default_acl_request_access,
     default_acl_http_request_access,
     default_acl_log_access,
     acl_conf_variables
};

/********************************************************************************************/


access_control_module_t *default_access_controllers[] = {
     &default_acl,
     NULL
};


access_control_module_t **used_access_controllers = default_access_controllers;

int access_reset()
{
     used_access_controllers = default_access_controllers;
     return 1;
}

int access_check_client(ci_connection_t * connection)
{
     int i = 0, res;
     ci_sockaddr_t *client_address, *server_address;
     if (!used_access_controllers)
          return CI_ACCESS_ALLOW;
     client_address = &(connection->claddr);
     server_address = &(connection->srvaddr);
     i = 0;
     while (used_access_controllers[i] != NULL) {
          if (used_access_controllers[i]->client_access) {
               res =
                   used_access_controllers[i]->client_access(client_address,
                                                             server_address);
               if (res != CI_ACCESS_UNKNOWN)
                    return res;
          }
          i++;
     }
     return CI_ACCESS_ALLOW;
}



int access_check_request(ci_request_t * req)
{
     char *user;
     int i, res;

     if (!used_access_controllers)
          return CI_ACCESS_ALLOW;

     user = ci_headers_value(req->request_header, "X-Authenticated-User");
     if (user) {
          ci_base64_decode(user, req->user, MAX_USERNAME_LEN);
     }

     i = 0;
     while (used_access_controllers[i] != NULL) {
          if (used_access_controllers[i]->request_access) {
               res = used_access_controllers[i]->request_access(req->user,
                                                                req->service,
                                                                req->type,
                                                                &(req->
                                                                  connection->
                                                                  claddr),
                                                                &(req->
                                                                  connection->
                                                                  srvaddr));
               if (res != CI_ACCESS_UNKNOWN)
                    return res;
          }
          i++;
     }

     return CI_ACCESS_ALLOW;
}


/* Returns CI_ACCESS_DENY to log CI_ACCESS_ALLOW to not log .........*/

int access_check_logging(ci_request_t * req)
{
     char *user;
     int i, res;

     if (!used_access_controllers)
          return CI_ACCESS_DENY;

     user = ci_headers_value(req->request_header, "X-Authenticated-User");
     if (user) {
          ci_base64_decode(user, req->user, MAX_USERNAME_LEN);
     }

     i = 0;
     while (used_access_controllers[i] != NULL) {
          if (used_access_controllers[i]->log_access) {
               res = used_access_controllers[i]->log_access(req->user,
                                                            req->service,
                                                            req->type,
                                                            &(req->connection->
                                                              claddr),
                                                            &(req->connection->
                                                              srvaddr));
               if (res != CI_ACCESS_UNKNOWN)
                    return res;
          }
          i++;
     }

     return CI_ACCESS_DENY;     /*By default log this request ....... */
}



int access_authenticate_request(ci_request_t * req)
{
     int i, res;

     if ((res = http_authorize(req)) == CI_ACCESS_DENY) {
          return res;
     }

     if (!used_access_controllers)
          return CI_ACCESS_ALLOW;

     i = 0;
     while (used_access_controllers[i] != NULL) {
          if (used_access_controllers[i]->request_access) {
               res = used_access_controllers[i]->http_request_access(req->user,
                                                                     req->
                                                                     service,
                                                                     req->type,
                                                                     &(req->
                                                                       connection->
                                                                       claddr),
                                                                     &(req->
                                                                       connection->
                                                                       srvaddr));
               if (res != CI_ACCESS_UNKNOWN) {
                    return res;
               }
          }
          i++;
     }

     return CI_ACCESS_ALLOW;
}

#define MAX_NAME_LEN 31         /*Must implement it as general limit of names ....... */


#ifdef USE_IPV6

void acl_list_ipv4_to_ipv6();

typedef union acl_inaddr {
     struct in_addr ipv4_addr;
     struct in6_addr ipv6_addr;
} acl_in_addr_t;

#define acl_ipv4_inaddr_is_zero(addr) ((addr).ipv4_addr.s_addr==0)
#define acl_ipv4_inaddr_are_equal(addr1,addr2) ((addr1).ipv4_addr.s_addr == (addr2).ipv4_addr.s_addr)
#define acl_ipv4_inaddr_check_net(addr1,addr2,mask) (((addr1).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr)==((addr2).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr))

#define acl_ipv4_inaddr_hostnetmask(addr)((addr).ipv4_addr.s_addr=htonl(0xFFFFFFFF))
#define acl_ipv4_inaddr_zero(addr) ((addr).ipv4_addr.s_addr=0)


#define acl_in6_addr_u32(addr) ((uint32_t *)&((addr).ipv6_addr))

#define acl_ipv6_inaddr_is_zero(addr) ( acl_in6_addr_u32(addr)[0]==0 && \
                                        acl_in6_addr_u32(addr)[1]==0 && \
                                        acl_in6_addr_u32(addr)[2]==0 && \
                                        acl_in6_addr_u32(addr)[3]==0)

#define acl_ipv6_inaddr_are_equal(addr1,addr2) ( acl_in6_addr_u32(addr1)[0]==acl_in6_addr_u32(addr2)[0] && \
                                                 acl_in6_addr_u32(addr1)[1]==acl_in6_addr_u32(addr2)[1] && \
                                                 acl_in6_addr_u32(addr1)[2]==acl_in6_addr_u32(addr2)[2] && \
                                                 acl_in6_addr_u32(addr1)[3]==acl_in6_addr_u32(addr2)[3])

#define acl_ipv6_inaddr_check_net(addr1,addr2,mask) ((acl_in6_addr_u32(addr1)[0] & acl_in6_addr_u32(mask)[0])==(acl_in6_addr_u32(addr2)[0] & acl_in6_addr_u32(mask)[0]) &&\
                                                     (acl_in6_addr_u32(addr1)[1] & acl_in6_addr_u32(mask)[1])==(acl_in6_addr_u32(addr2)[1] & acl_in6_addr_u32(mask)[1]) &&\
                                                     (acl_in6_addr_u32(addr1)[2] & acl_in6_addr_u32(mask)[2])==(acl_in6_addr_u32(addr2)[2] & acl_in6_addr_u32(mask)[2]) &&\
                                                     (acl_in6_addr_u32(addr1)[3] & acl_in6_addr_u32(mask)[3])==(acl_in6_addr_u32(addr2)[3] & acl_in6_addr_u32(mask)[3]))

#define acl_ipv6_inaddr_hostnetmask(addr)(acl_in6_addr_u32(addr)[0]=htonl(0xFFFFFFFF),\
                                          acl_in6_addr_u32(addr)[1]=htonl(0xFFFFFFFF),\
                                          acl_in6_addr_u32(addr)[2]=htonl(0xFFFFFFFF),\
                                          acl_in6_addr_u32(addr)[3]=htonl(0xFFFFFFFF))

#define acl_ipv6_inaddr_is_v4mapped(addr) (acl_in6_addr_u32(addr)[0]==0 &&\
                                           acl_in6_addr_u32(addr)[1]==0 &&\
                                           acl_in6_addr_u32(addr)[2]== htonl(0xFFFF))

#define acl_inaddr_zero(addr) (memset(&(addr),0,sizeof(acl_in_addr_t)))
#define acl_inaddr_copy(dest,src) (memcpy(&(dest),&(src),sizeof(acl_in_addr_t)))

/*We can do this because ipv4_addr in practice exists in s6_addr[0]*/
#define acl_inaddr_ipv4_to_ipv6(addr)( acl_in6_addr_u32(addr)[3]=(addr).ipv4_addr.s_addr,\
                                       acl_in6_addr_u32(addr)[0]=0,\
                                       acl_in6_addr_u32(addr)[1]=0,\
                                       acl_in6_addr_u32(addr)[2]= htonl(0xFFFF))
#define acl_netmask_ipv4_to_ipv6(addr)(acl_in6_addr_u32(addr)[3]=(addr).ipv4_addr.s_addr,\
                                       acl_in6_addr_u32(addr)[0]= htonl(0xFFFFFFFF),\
                                       acl_in6_addr_u32(addr)[1]= htonl(0xFFFFFFFF),\
                                       acl_in6_addr_u32(addr)[2]= htonl(0xFFFFFFFF))
#else                           /*if no USE_IPV6 */

typedef struct in_addr acl_in_addr_t;

#define acl_ipv4_inaddr_is_zero(addr) ((addr).s_addr==0)
#define acl_ipv4_inaddr_are_equal(addr1,addr2) ((addr1).s_addr == (addr2).s_addr)
#define acl_ipv4_inaddr_check_net(addr1,addr2,mask) (((addr1).s_addr & (mask).s_addr)==((addr2).s_addr & (mask).s_addr))

#define acl_ipv4_inaddr_hostnetmask(addr)((addr).s_addr=htonl(0xFFFFFFFF))
#define acl_ipv4_inaddr_zero(addr) ((addr).s_addr=0)

#define acl_inaddr_zero(addr) ((addr).s_addr=0)
#define acl_inaddr_copy(dest,src) ((dest)=(src))

#endif                          /*ifdef USE_IPV6 */

#define acl_copy_inaddr(dest,src,len) memcpy(dest,src,len)

typedef struct acl_spec acl_spec_t;
struct acl_spec {
     char name[MAX_NAME_LEN + 1];
     char *username;
     char *servicename;
     int ci_request_type;
     int family;
     unsigned int port;
     acl_in_addr_t hclient_address;
     acl_in_addr_t hclient_netmask;
     acl_in_addr_t hserver_address;
     acl_spec_t *next;
};


typedef struct access_entry access_entry_t;
struct access_entry {
     int type;                  /*CI_ACCESS_DENY or CI_ACCESS_ALLOW or CI_ACCESS_AUTH */
     acl_spec_t *spec;
     access_entry_t *next;
};

struct access_entry_list {
     access_entry_t *access_entry_list;
     access_entry_t *access_entry_last;
};


acl_spec_t *acl_spec_list = NULL;
acl_spec_t *acl_spec_last = NULL;

struct access_entry_list acl_access_list;
struct access_entry_list acl_log_access_list;

int match_ipv4_connection(acl_spec_t * spec, acl_spec_t * conn_spec);
int match_ipv6_connection(acl_spec_t * spec, acl_spec_t * conn_spec);
int (*match_connection) (acl_spec_t *, acl_spec_t *);

int match_request(acl_spec_t * spec, acl_spec_t * req_spec);
void release_access_list(struct access_entry_list *list);
void release_acl_list(acl_spec_t * list);

int default_acl_init(struct ci_server_conf *server_conf)
{
     acl_spec_list = NULL;      /*Not needed ...... */
     acl_spec_last = NULL;
     acl_access_list.access_entry_list = NULL;
     acl_access_list.access_entry_last = NULL;
     acl_log_access_list.access_entry_list = NULL;
     acl_log_access_list.access_entry_last = NULL;
     return 1;
}

int default_acl_post_init(struct ci_server_conf *server_conf)
{
#ifdef USE_IPV6
     if (server_conf->PROTOCOL_FAMILY == AF_INET6) {
          ci_debug_printf(5,
                          "We are listening to a ipv6 address. Going to change all acl address to ipv6 address!\n");
          acl_list_ipv4_to_ipv6();
          match_connection = match_ipv6_connection;
     }
     else
#endif
          match_connection = match_ipv4_connection;

     return 1;
}

void default_acl_release()
{
     release_acl_list(acl_spec_list);
     release_access_list(&acl_access_list);
     release_access_list(&acl_log_access_list);
     acl_spec_list = NULL;
     acl_spec_last = NULL;
     acl_access_list.access_entry_list = NULL;
     acl_access_list.access_entry_last = NULL;
     acl_log_access_list.access_entry_list = NULL;
     acl_log_access_list.access_entry_last = NULL;
}

int default_acl_client_access(ci_sockaddr_t * client_address,
                              ci_sockaddr_t * server_address)
{
     access_entry_t *entry;
     acl_spec_t *spec, conn_spec;

     entry = acl_access_list.access_entry_list;
     if (!entry)
          return CI_ACCESS_UNKNOWN;

     conn_spec.family = server_address->ci_sin_family;
     conn_spec.port = server_address->ci_sin_port;
     acl_copy_inaddr(&conn_spec.hserver_address, server_address->ci_sin_addr,
                     server_address->ci_inaddr_len);
     acl_copy_inaddr(&conn_spec.hclient_address, client_address->ci_sin_addr,
                     client_address->ci_inaddr_len);

     while (entry) {
          spec = entry->spec;
          if ((*match_connection) (spec, &conn_spec)) {
               if (entry->type == CI_ACCESS_HTTP_AUTH) {
                    return CI_ACCESS_HTTP_AUTH;
               }
               else if (spec->username == NULL && spec->servicename == NULL
                        && spec->ci_request_type == 0) {
                    /* So no user or service name and type check needed */
                    return entry->type;
               }
               else
                    return CI_ACCESS_PARTIAL;   /*If service or username or request type needed for this spec
                                                   must get icap headers for this connection */
          }
          entry = entry->next;
     }
     return CI_ACCESS_UNKNOWN;

}


int default_acl_request_access(char *dec_user, char *service,
                               int req_type,
                               ci_sockaddr_t * client_address,
                               ci_sockaddr_t * server_address)
{
     access_entry_t *entry;
     acl_spec_t *spec, req_spec;
     entry = acl_access_list.access_entry_list;
     if (!entry)
          return CI_ACCESS_UNKNOWN;

     req_spec.username = dec_user;      /*dec_user always non null (required) */
     req_spec.servicename = service;
     req_spec.ci_request_type = req_type;
     req_spec.family = server_address->ci_sin_family;
     req_spec.port = server_address->ci_sin_port;
     acl_copy_inaddr(&req_spec.hserver_address, server_address->ci_sin_addr,
                     server_address->ci_inaddr_len);
     acl_copy_inaddr(&req_spec.hclient_address, client_address->ci_sin_addr,
                     client_address->ci_inaddr_len);

     while (entry) {
          spec = entry->spec;
          if (match_request(spec, &req_spec)) {
               return entry->type;
          }
          entry = entry->next;
     }
     return CI_ACCESS_UNKNOWN;
}

int default_acl_http_request_access(char *dec_user, char *service,
                                    int req_type,
                                    ci_sockaddr_t * client_address,
                                    ci_sockaddr_t * server_address)
{
     access_entry_t *entry;
     acl_spec_t *spec, req_spec;
     entry = acl_access_list.access_entry_list;

     if (!entry)
          return CI_ACCESS_UNKNOWN;

     req_spec.username = dec_user;      /*dec_user always non null (required) */
     req_spec.servicename = service;
     req_spec.ci_request_type = req_type;
     req_spec.family = server_address->ci_sin_family;
     req_spec.port = server_address->ci_sin_port;
     acl_copy_inaddr(&req_spec.hserver_address, server_address->ci_sin_addr,
                     server_address->ci_inaddr_len);
     acl_copy_inaddr(&req_spec.hclient_address, client_address->ci_sin_addr,
                     client_address->ci_inaddr_len);


     while (entry) {
          spec = entry->spec;
          if (match_request(spec, &req_spec)) {
               return (entry->type ==
                       CI_ACCESS_HTTP_AUTH ? CI_ACCESS_ALLOW : entry->type);
          }
          entry = entry->next;
     }
     return CI_ACCESS_UNKNOWN;
}


int default_acl_log_access(char *dec_user, char *service,
                           int req_type,
                           ci_sockaddr_t * client_address,
                           ci_sockaddr_t * server_address)
{

     access_entry_t *entry;
     acl_spec_t *spec, req_spec;
     entry = acl_log_access_list.access_entry_list;

     if (!entry)
          return CI_ACCESS_UNKNOWN;

     req_spec.username = dec_user;      /*dec_user always non null (required) */
     req_spec.servicename = service;
     req_spec.ci_request_type = req_type;
     req_spec.family = server_address->ci_sin_family;
     req_spec.port = server_address->ci_sin_port;
     acl_copy_inaddr(&req_spec.hserver_address, server_address->ci_sin_addr,
                     server_address->ci_inaddr_len);
     acl_copy_inaddr(&req_spec.hclient_address, client_address->ci_sin_addr,
                     client_address->ci_inaddr_len);


     while (entry) {
          spec = entry->spec;
          if (match_request(spec, &req_spec)) {
               return entry->type;
          }
          entry = entry->next;
     }
     return CI_ACCESS_UNKNOWN;
}



/*********************************************************************/
/*ACL list managment functions                                       */

int match_ipv4_connection(acl_spec_t * spec, acl_spec_t * conn_spec)
{

#ifdef USE_IPV6
     if (spec->family != conn_spec->family)
          return 0;
#endif

     if (spec->port != 0 && spec->port != conn_spec->port)
          return 0;

     if (!acl_ipv4_inaddr_is_zero(spec->hserver_address) &&
         !acl_ipv4_inaddr_are_equal(spec->hserver_address,
                                    conn_spec->hserver_address))
          return 0;

     if (!acl_ipv4_inaddr_is_zero(spec->hclient_address)
         && !acl_ipv4_inaddr_is_zero(spec->hclient_netmask)
         && !acl_ipv4_inaddr_check_net(spec->hclient_address,
                                       conn_spec->hclient_address,
                                       spec->hclient_netmask))
          return 0;

     return 1;
}

#ifdef USE_IPV6
int match_ipv6_connection(acl_spec_t * spec, acl_spec_t * conn_spec)
{
/*
     char ip1[64],ip2[64],mask[64];
     ci_inet_ntoa(AF_INET6,&(spec->hclient_netmask.ipv6_addr),mask,64);     
     ci_inet_ntoa(AF_INET6,&(conn_spec->hclient_address.ipv6_addr),ip1,64);
     ci_inet_ntoa(AF_INET6,&(conn_spec->hserver_address.ipv6_addr),ip2,64);
     ci_debug_printf(9,"To match_ipv6:Going to match %s/%s -> %s\n",ip1,mask,ip2);     
     ci_inet_ntoa(AF_INET6,&(spec->hclient_address.ipv6_addr),ip1,64);
     ci_inet_ntoa(AF_INET6,&(spec->hserver_address.ipv6_addr),ip2,64);
     ci_debug_printf(9,"match_ipv6:With spec %s/%s -> %s\n",ip1,mask,ip2);
*/
     if (spec->port != 0 && spec->port != conn_spec->port)
          return 0;

     if (!acl_ipv6_inaddr_is_zero(spec->hserver_address) &&
         !acl_ipv6_inaddr_are_equal(spec->hserver_address,
                                    conn_spec->hserver_address))
          return 0;

     if (!acl_ipv6_inaddr_is_zero(spec->hclient_address)
         && !acl_ipv6_inaddr_is_zero(spec->hclient_netmask)
         && !acl_ipv6_inaddr_check_net(spec->hclient_address,
                                       conn_spec->hclient_address,
                                       spec->hclient_netmask))
          return 0;

     return 1;
}

#endif


int match_request(acl_spec_t * spec, acl_spec_t * req_spec)
{

     if (!(*match_connection) (spec, req_spec))
          return 0;
     if (spec->servicename != NULL
         && strcmp(spec->servicename, req_spec->servicename) != 0)
          return 0;
     if (spec->ci_request_type != 0
         && spec->ci_request_type != req_spec->ci_request_type) {
          return 0;
     }

     if (spec->username != NULL) {
          if (req_spec->username == NULL)
               return 0;
          if (strcmp(spec->username, "*") == 0) /*All users ...... */
               return 1;
          if (strcmp(spec->username, req_spec->username) != 0)  /*here we are assuming that req_spec->username */
               return 0;        /* is always not null !!!!!!!!!! */
     }

     return 1;
}


acl_spec_t *find_acl_spec_byname(char *name)
{
     acl_spec_t *spec;
     if (acl_spec_list == NULL)
          return NULL;
     for (spec = acl_spec_list; spec != NULL; spec = spec->next) {
          if (strcmp(spec->name, name) == 0)
               return spec;
     }
     return NULL;
}


access_entry_t *new_access_entry(struct access_entry_list * list, int type,
                                 char *name)
{
     access_entry_t *a_entry;
     acl_spec_t *spec;

     if ((spec = find_acl_spec_byname(name)) == NULL)
          return NULL;

     /*create the access entry ....... */
     if ((a_entry = malloc(sizeof(access_entry_t))) == NULL)
          return NULL;
     a_entry->next = NULL;
     a_entry->type = type;
     a_entry->spec = spec;

     /*Add access entry to the end of list ........ */
     if (list->access_entry_list == NULL) {
          list->access_entry_list = a_entry;
          list->access_entry_last = a_entry;
     }
     else {
          list->access_entry_last->next = a_entry;
          list->access_entry_last = a_entry;
     }

     ci_debug_printf(10, "ACL entry %s %d  added\n", name, type);
     return a_entry;
}

void release_access_list(struct access_entry_list *list)
{
     access_entry_t *access_cur, *access_next;

     access_cur = list->access_entry_list;
     while (access_cur) {
          access_next = access_cur->next;
          free(access_cur);
          access_cur = access_next;
     }
     list->access_entry_list = NULL;
     list->access_entry_last = NULL;
}

void fill_ipv4_addresses(acl_spec_t * a_spec,
                         acl_in_addr_t * client_address,
                         acl_in_addr_t * client_netmask,
                         acl_in_addr_t * server_address)
{

     acl_inaddr_copy(a_spec->hclient_address, *client_address);

     if (!acl_ipv4_inaddr_is_zero(*client_netmask))
          acl_inaddr_copy(a_spec->hclient_netmask, *client_netmask);
     else {
          if (!acl_ipv4_inaddr_is_zero(*client_address))
               acl_ipv4_inaddr_hostnetmask(a_spec->hclient_netmask);
          else
               acl_ipv4_inaddr_zero(a_spec->hclient_netmask);
     }
     acl_inaddr_copy(a_spec->hserver_address, *server_address);
}

#ifdef USE_IPV6

void acl_list_ipv4_to_ipv6()
{
     acl_spec_t *spec;
     if (acl_spec_list == NULL)
          return;
     for (spec = acl_spec_list; spec != NULL; spec = spec->next) {
          if (spec->family == AF_INET) {
               spec->family = AF_INET6;
               if (acl_ipv4_inaddr_is_zero(spec->hclient_address))
                    acl_inaddr_zero(spec->hclient_address);
               else
                    acl_inaddr_ipv4_to_ipv6(spec->hclient_address);

               if (acl_ipv4_inaddr_is_zero(spec->hclient_netmask))
                    acl_inaddr_zero(spec->hclient_netmask);
               else
                    acl_netmask_ipv4_to_ipv6(spec->hclient_netmask);

               if (acl_ipv4_inaddr_is_zero(spec->hserver_address))
                    acl_inaddr_zero(spec->hserver_address);
               else
                    acl_inaddr_ipv4_to_ipv6(spec->hserver_address);
          }
     }
     return;
}


void fill_ipv6_addresses(acl_spec_t * a_spec,
                         acl_in_addr_t * client_address,
                         acl_in_addr_t * client_netmask,
                         acl_in_addr_t * server_address)
{

     acl_inaddr_copy(a_spec->hclient_address, *client_address);

     if (!acl_ipv6_inaddr_is_zero(*client_netmask))
          acl_inaddr_copy(a_spec->hclient_netmask, *client_netmask);
     else {
          if (!acl_ipv6_inaddr_is_zero(*client_address))
               acl_ipv6_inaddr_hostnetmask(a_spec->hclient_netmask);
          else
               acl_inaddr_zero(a_spec->hclient_netmask);
     }
     acl_inaddr_copy(a_spec->hserver_address, *server_address);
}

#endif

acl_spec_t *new_acl_spec(char *name, char *username, int port, char *service, int ci_request_type, int socket_family,      /*AF_INET, AF_INET6 */
                         acl_in_addr_t * client_address,
                         acl_in_addr_t * client_netmask,
                         acl_in_addr_t * server_address)
{
     acl_spec_t *a_spec;
     char str_cl_addr[CI_IPLEN], str_cl_netmask[CI_IPLEN],
         str_srv_addr[CI_IPLEN];

     if ((a_spec = malloc(sizeof(acl_spec_t))) == NULL)
          return NULL;
     a_spec->next = NULL;
     strncpy(a_spec->name, name, MAX_NAME_LEN);
     a_spec->name[MAX_NAME_LEN] = '\0';
     if (username) {
          a_spec->username = strdup(username);
     }
     else
          a_spec->username = NULL;

     if (service) {
          a_spec->servicename = strdup(service);
     }
     else
          a_spec->servicename = NULL;

     a_spec->ci_request_type = ci_request_type;
     a_spec->port = htons(port);

     a_spec->family = socket_family;    /*AF_INET */

#ifdef USE_IPV6
     if (socket_family == AF_INET6)
          fill_ipv6_addresses(a_spec, client_address, client_netmask,
                              server_address);
     else
#endif
          fill_ipv4_addresses(a_spec, client_address, client_netmask,
                              server_address);

     if (acl_spec_list == NULL) {
          acl_spec_list = a_spec;
          acl_spec_last = a_spec;
     }
     else {
          acl_spec_last->next = a_spec;
          acl_spec_last = a_spec;
     }

     ci_debug_printf(6,
                     "ACL spec name:%s username:%s service:%s type:%d port:%d src_ip:%s src_netmask:%s server_ip:%s  \n",
                     name, (username != NULL ? username : "-"),
                     (service != NULL ? service : "-"), ci_request_type, port,
                     ci_inet_ntoa(socket_family, &(a_spec->hclient_address),
                                  str_cl_addr, CI_IPLEN),
                     ci_inet_ntoa(socket_family, &(a_spec->hclient_netmask),
                                  str_cl_netmask, CI_IPLEN),
                     ci_inet_ntoa(socket_family, &(a_spec->hserver_address),
                                  str_srv_addr, CI_IPLEN)
         );
     return a_spec;
}

#ifdef USE_IPV6
int check_protocol_family(char *ip)
{
     if (strchr(ip, ':') != NULL)
          return AF_INET6;
     return AF_INET;
}

#endif

void release_acl_list(acl_spec_t * list)
{
     acl_spec_t *acl_cur, *acl_next;
     acl_cur = list;
     while (acl_cur != NULL) {
          if (acl_cur->username)
               free(acl_cur->username);
          if (acl_cur->servicename)
               free(acl_cur->servicename);
          free(acl_cur);
          acl_next = acl_cur->next;
          acl_cur = acl_next;
     }
}

/********************************************************************/
/*Configuration functions ...............                           */

int cfg_acl_add(char *directive, char **argv, void *setdata)
{
     char *name, *username, *service, *str;
     int i, res, ci_request_type;
     unsigned int port;
#ifdef USE_IPV6
     int family = 0;
#else
     int family = AF_INET;
#endif
     acl_in_addr_t client_address, client_netmask, server_address;
     username = NULL;
     service = NULL;
     port = 0;
     ci_request_type = 0;
     acl_inaddr_zero(client_address);
     acl_inaddr_zero(client_netmask);
     acl_inaddr_zero(server_address);

     if (argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Parse error in directive %s \n", directive);
          return 0;
     }
     name = argv[0];
     i = 1;
     while (argv[i] != NULL) {
          if (argv[i + 1] == NULL) {
               ci_debug_printf(1, "Parse error in directive %s \n", directive);
               return 0;
          }

          if (strcmp(argv[i], "src") == 0) {    /*has the form ip/netmask */
#ifdef USE_IPV6
               if (family == 0)
                    family = check_protocol_family(argv[i]);
               else {
                    if (family != check_protocol_family(argv[i])) {
                         ci_debug_printf(1,
                                         "Mixing ipv4/ipv6 address in the same acl spec does not allowed."
                                         " Disabling %s acl spec \n", name);
                         return 0;
                    }
               }

#endif
               if ((str = strchr(argv[i + 1], '/')) != NULL) {
                    *str = '\0';
                    str = str + 1;
                    if (!(res = ci_inet_aton(family, str, &client_netmask))) {
                         ci_debug_printf(1,
                                         "Invalid src netmask address %s. Disabling %s acl spec \n",
                                         str, name);
                         return 0;
                    }
               }

               if (!(res = ci_inet_aton(family, argv[i + 1], &client_address))) {
                    ci_debug_printf(1,
                                    "Invalid src ip address %s. Disabling %s acl spec \n",
                                    argv[i + 1], name);
                    return 0;
               }
          }
          else if (strcmp(argv[i], "srvip") == 0) {     /*has the form ip */
#ifdef USE_IPV6
               if (family == 0)
                    family = check_protocol_family(argv[i]);
               else {
                    if (family != check_protocol_family(argv[i])) {
                         ci_debug_printf(1,
                                         "Mixing ipv4/ipv6 address in the same acl spec does not allowed."
                                         " Disabling %s acl spec \n", name);
                         return 0;
                    }
               }
#endif
               if (!(res = ci_inet_aton(family, argv[i + 1], &server_address))) {
                    ci_debug_printf(1,
                                    "Invalid server ip address %s. Disabling %s acl spec \n",
                                    argv[i + 1], name);
                    return 0;
               }
          }
          else if (strcmp(argv[i], "port") == 0) {      /*an integer */
               if ((port = strtol(argv[i + 1], NULL, 10)) == 0) {
                    ci_debug_printf(1,
                                    "Invalid server port  %s. Disabling %s acl spec \n",
                                    argv[i + 1], name);
                    return 0;
               }
          }
          else if (strcmp(argv[i], "user") == 0) {      /*a string */
               username = argv[i + 1];
          }
          else if (strcmp(argv[i], "service") == 0) {   /*a string */
               service = argv[i + 1];
          }
          else if (strcmp(argv[i], "type") == 0) {
               if (strcasecmp(argv[i + 1], "options") == 0)
                    ci_request_type = ICAP_OPTIONS;
               else if (strcasecmp(argv[i + 1], "reqmod") == 0)
                    ci_request_type = ICAP_REQMOD;
               else if (strcasecmp(argv[i + 1], "respmod") == 0)
                    ci_request_type = ICAP_RESPMOD;
               else {
                    ci_debug_printf(1,
                                    "Invalid request type  %s. Disabling %s acl spec \n",
                                    argv[i + 1], name);
                    return 0;
               }
          }
          else {
               ci_debug_printf(1,
                               "Invalid directive :%s. Disabling %s acl spec \n",
                               argv[i], name);
               return 0;
          }
          i += 2;
     }

     new_acl_spec(name, username, port, service, ci_request_type, family,
                  &client_address, &client_netmask, &server_address);

     return 1;
}


int cfg_acl_access(char *directive, char **argv, void *setdata)
{
     int type;
     char *acl_spec;
     struct access_entry_list *tolist;

     if (argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Parse error in directive %s \n", directive);
          return 0;
     }
     if (strcmp(argv[0], "allow") == 0) {
          type = CI_ACCESS_ALLOW;
          tolist = &acl_access_list;
     }
     else if (strcmp(argv[0], "deny") == 0) {
          type = CI_ACCESS_DENY;
          tolist = &acl_access_list;
     }
     else if (strcmp(argv[0], "http_auth") == 0) {
          type = CI_ACCESS_HTTP_AUTH;
          tolist = &acl_access_list;
     }
     else if (strcmp(argv[0], "log") == 0) {
          type = CI_ACCESS_DENY;
          tolist = &acl_log_access_list;
     }
     else if (strcmp(argv[0], "nolog") == 0) {
          type = CI_ACCESS_ALLOW;
          tolist = &acl_log_access_list;
     }
     else {
          ci_debug_printf(1, "Invalid directive :%s. Disabling %s acl rule \n",
                          argv[0], argv[1]);
          return 0;
     }
     acl_spec = argv[1];

     if (!new_access_entry(tolist, type, acl_spec))
          return 0;
     return 1;
}
