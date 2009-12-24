#include "common.h"
#include "c-icap.h"
#include "request.h"
#include "access.h"
#include "module.h"
#include "simple_api.h"
#include "acl.h"
#include "lookup_table.h"
#include "debug.h"

char *DEFAULT_AUTH_METHOD="basic";
char *REMOTE_PROXY_USER_HEADER = "X-Authenticated-User";
int ALLOW_REMOTE_PROXY_USERS = 0;
int REMOTE_PROXY_USER_HEADER_ENCODED = 1;

/*To be repleced by http_authenticate .......*/
int http_authorize(ci_request_t * req, char *method)
{
    return http_authenticate(req, method);
     ci_debug_printf(1, "Allowing http_access.....\n");
     return CI_ACCESS_ALLOW;
}


int call_authenticators(authenticator_module_t ** authenticators,
                        void *method_data)
{
     int i, res;
     for (i = 0; authenticators[i] != NULL; i++) {
          if ((res =
               authenticators[i]->authenticate(method_data, NULL)) !=
              CI_ACCESS_UNKNOWN) {
               return res;
          }
     }
     return CI_ACCESS_DENY;
}


int http_authenticate(ci_request_t * req, char *use_method)
{
     struct http_auth_method *auth_method;
     authenticator_module_t **authenticators;
     void *method_data;
     char *auth_str, *method_str, *username;
     char *auth_header = NULL;
     int len, res;

     if (ALLOW_REMOTE_PROXY_USERS && !use_method) {
	 username = ci_headers_value(req->request_header, REMOTE_PROXY_USER_HEADER);
	 if (username) {
	     if (REMOTE_PROXY_USER_HEADER_ENCODED)
		 ci_base64_decode(username, req->user, MAX_USERNAME_LEN);
	     else {
		 strncpy(req->user, username, MAX_USERNAME_LEN);
		 req->user[MAX_USERNAME_LEN] = '\0';;
	     }
	     return CI_ACCESS_ALLOW;
	 }
     }

     if (!use_method)
	 use_method = DEFAULT_AUTH_METHOD;

     auth_method = get_authentication_schema(use_method, &authenticators);
     
     if (auth_method == NULL) {
	 ci_debug_printf(1, "Authentication method not found ...\n");
	 return CI_ACCESS_DENY;
     }

     res = CI_ACCESS_DENY;
     if ((method_str =
          ci_headers_value(req->request_header, "Proxy-Authorization")) != NULL) {
          ci_debug_printf(5, "Str is %s ....\n", method_str);
          if ((auth_str = strchr(method_str, ' ')) == NULL)
               return CI_ACCESS_DENY;
          len = auth_str - method_str;
          method_str[len] = '\0';       /*Just to check if we are support it ..... */
	  if (strcmp(method_str,use_method) != 0)
	      return CI_ACCESS_DENY;
          ci_debug_printf(5, "Method is %s ....\n", method_str);
	  method_str[len] = ' ';        /*Put back the space ......... */


          auth_str++;
          method_data = auth_method->create_auth_data(auth_str, &username);
          strncpy(req->user, username, MAX_USERNAME_LEN);
          req->user[MAX_USERNAME_LEN] = '\0';
          /*Call an authenticator now ........ */
          res = call_authenticators(authenticators, method_data);
          /*And release data ..... */
          auth_method->release_auth_data(method_data);
     }

     if (res == CI_ACCESS_DENY) {
	 auth_header = auth_method->authentication_header();
	 ci_headers_add(req->xheaders, auth_header);
	 if (auth_method->release_authentication_header)
	     auth_method->release_authentication_header(auth_header);
	 req->auth_required = 1;
	 ci_debug_printf(3, "Access denied. Authentication required!!!!!\n");
     }
     
     return res;
}


/******************************************************/
/* The auth acl implementation                        */

void *get_auth(ci_request_t *req, char *param);
void free_auth(ci_request_t *req,void *data);

ci_acl_type_t acl_auth={
     "auth",
     get_auth,
     free_auth,
     &ci_str_ext_ops
};


void *get_auth(ci_request_t *req, char *param)
{
  int res;
  if ((res = http_authorize(req, param)) != CI_ACCESS_ALLOW) {
      return NULL;
  }

  ci_debug_printf(5, "Authenticated user: %s\n", req->user);
  return req->user;
}

void free_auth(ci_request_t *req, void *data)
{
  
}


void init_http_auth()
{ 
    ci_acl_type_add(&acl_auth);
}

/**********************************************************************************/
/* basic auth method implementation                                               */

static char *basic_realm = "Basic authentication";
static char *basic_authentication = NULL;

/*Configuration Table .....*/
static struct ci_conf_entry basic_conf_params[] = {
     {"BasicRealm", &basic_realm, ci_cfg_set_str, NULL},
     {NULL, NULL, NULL, NULL}
};

int basic_post_init(struct ci_server_conf *server_conf);
void basic_close();
struct http_basic_auth_data *basic_create_auth_data(char *auth_line,
                                                    char **username);
void basic_release_auth_data(struct http_basic_auth_data *data);
char *basic_authentication_header();

http_auth_method_t basic_auth = {
     "basic",
     NULL,                      /*init */
     basic_post_init,
     basic_close,
     (void *(*)(char *, char **)) basic_create_auth_data,
     (void (*)(void *)) basic_release_auth_data,
     basic_authentication_header,
     NULL,   /*release_authentication_header*/
     basic_conf_params
};

#define BASIC_HEAD_PREFIX  "Proxy-Authenticate: Basic realm="
int basic_post_init(struct ci_server_conf *server_conf)
{
     int size = strlen(BASIC_HEAD_PREFIX)+strlen(basic_realm)+3;
     basic_authentication = malloc((size+1)*sizeof(char));

     if (!basic_authentication)
          return 0;

     snprintf(basic_authentication, size, "%s\"%s\"", BASIC_HEAD_PREFIX, basic_realm);
     return 1;
}

void basic_close()
{
     if (basic_authentication) {
	  free(basic_authentication);
	  basic_authentication = NULL;
     }
}

struct http_basic_auth_data *basic_create_auth_data(char *auth_line,
                                                    char **username)
{
     struct http_basic_auth_data *data;
     char dec_http_user[MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2], *str;
     int max_decode_len = MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2;

     data = malloc(sizeof(struct http_basic_auth_data));

     ci_base64_decode(auth_line, dec_http_user, max_decode_len);
     ci_debug_printf(5, "The proxy user is:%s (basic method) \n", dec_http_user);

     if ((str = strchr(dec_http_user, ':')) != NULL) {
          *str = '\0';
          str++;
          strncpy(data->http_pass, str, HTTP_MAX_PASS_LEN);
          data->http_pass[HTTP_MAX_PASS_LEN - 1] = '\0';
     }
     strncpy(data->http_user, dec_http_user, MAX_USERNAME_LEN);
     data->http_user[MAX_USERNAME_LEN] = '\0';
     if (username)
          *username = data->http_user;
     return data;
}


void basic_release_auth_data(struct http_basic_auth_data *data)
{
     free(data);
}

char *basic_authentication_header()
{
     return basic_authentication;
}

/***********************************************************************/
/*file_basic authenticator .......                                     */

char *USERS_DB_PATH = NULL;
int PASSWORDS_ENCRYPTED = 1;

struct ci_lookup_table *users_db = NULL;

/*Configuration Table .....*/
static struct ci_conf_entry basic_simple_db_conf_variables[] = {
     {"UsersDB", &USERS_DB_PATH, ci_cfg_set_str, NULL},
     {NULL, NULL, NULL, NULL}
};

int basic_simple_db_post_init(struct ci_server_conf *server_conf);
void basic_simple_db_close();
int basic_simple_db_athenticate(struct http_basic_auth_data *data, char *usedb);


authenticator_module_t basic_simple_db = {
     "basic_simple_db",              /* char *name; */
     "basic",                   /* char *method; */
     NULL,                      /* int (*init_authenticator)(); */
     basic_simple_db_post_init,      /* int (*post_init_authenticator)(); */
     basic_simple_db_close,          /* void (*close_authenticator)(); */
     (int (*)(void *)) basic_simple_db_athenticate,  /* int (*authenticate)(void *data); */
     basic_simple_db_conf_variables  /* struct ci_conf_entry *conf_table; */
};


int basic_simple_db_post_init(struct ci_server_conf *server_conf)
{
    if (USERS_DB_PATH) {
	users_db = ci_lookup_table_create(USERS_DB_PATH);
	if (!users_db->open(users_db)) {
	    ci_lookup_table_destroy(users_db);
	    users_db = NULL;
	}
    }
    return 1;
}

void basic_simple_db_close()
{
    if (users_db) {
	ci_lookup_table_destroy(users_db);
	users_db = NULL;
    }
}

/*
 This function is under construction ........
*/

int basic_simple_db_athenticate(struct http_basic_auth_data *data, char *usedb)
{
     char **pass = NULL;
#ifdef HAVE_CRYPT_R
     char *pass_enc;
     struct crypt_data crypt_data;
#endif
     void *user_ret;
     int ret = CI_ACCESS_ALLOW;
     ci_debug_printf(1, "Going to authenticate user %s:%s\n", data->http_user,
                     data->http_pass);

     if (!users_db)
	 return CI_ACCESS_DENY;

     user_ret = users_db->search(users_db, data->http_user, (void ***)&pass);
     if (!user_ret)
	 return CI_ACCESS_DENY;

     if(!pass || pass[0]=='\0') {
	 if (data->http_pass[0] != '\0')
	     ret = CI_ACCESS_DENY;
	 else
	     ret = CI_ACCESS_ALLOW;
     }
#ifdef HAVE_CRYPT_R
     else if (PASSWORDS_ENCRYPTED) {
	 pass_enc = crypt_r(data->http_pass, pass[0], &crypt_data);
	 if (strcmp(pass_enc, pass[0]) != 0)
	     ret = CI_ACCESS_DENY;
     }
#endif
     else {
	 if (strcmp(data->http_pass, pass[0]) != 0)
	     ret = CI_ACCESS_DENY;
     }

     users_db->release_result(users_db, (void **)pass);

     ci_debug_printf(4, "User %s, %s\n", data->http_user,
		     ret == CI_ACCESS_ALLOW?"authenticated":"authentication/authorization fails");

     return ret;

/*
     if (strcmp(data->http_user, "squiduser") != 0)
          return CI_ACCESS_UNKNOWN;
     if (strcmp(data->http_pass, "123") != 0)
          return CI_ACCESS_DENY;

     ci_debug_printf(1, "User authenticated ........\n");
*/
     return CI_ACCESS_ALLOW;
}
