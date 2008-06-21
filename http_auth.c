#include "c-icap.h"
#include "request.h"
#include "access.h"
#include "module.h"
#include "simple_api.h"
#include "debug.h"


/*To be repleced by http_authenticate .......*/
int http_authorize(ci_request_t * req)
{
     return http_authenticate(req);
     ci_debug_printf(1, "Allowing http_access.....\n");
     return CI_ACCESS_ALLOW;
}


int call_authenticators(authenticator_module_t ** authenticators,
                        void *method_data)
{
     int i, res;
     for (i = 0; authenticators[i] != NULL; i++) {
          if ((res =
               authenticators[i]->authenticate(method_data)) !=
              CI_ACCESS_UNKNOWN) {
               return res;
          }
     }
     return CI_ACCESS_DENY;
}


int http_authenticate(ci_request_t * req)
{
     struct http_auth_method *auth_method;
     authenticator_module_t **authenticators;
     void *method_data;
     char *auth_str, *method_str, *username;
     int len, res;

     if ((method_str =
          ci_http_request_get_header(req, "Proxy-Authorization")) != NULL) {
          ci_debug_printf(10, "Str is %s ....\n", method_str);
          if ((auth_str = strchr(method_str, ' ')) == NULL)
               return 0;
          len = auth_str - method_str;
          method_str[len] = '\0';       /*Just to pass it to the following method ..... */
          ci_debug_printf(10, "Method is %s ....\n", method_str);
          auth_method = get_authentication_schema(method_str, &authenticators);
          method_str[len] = ' ';        /*Put back the space ......... */

          if (auth_method == NULL) {
               ci_debug_printf(1, "Authentication method not found ...\n");
               return 0;
          }

          auth_str++;
          method_data = auth_method->create_auth_data(auth_str, &username);
          strncpy(req->user, username, MAX_USERNAME_LEN);
          req->user[MAX_USERNAME_LEN] = '\0';
          /*Call an authenticator now ........ */
          res = call_authenticators(authenticators, method_data);
          /*And release data ..... */
          auth_method->release_auth_data(method_data);
          return res;
     }
     ci_debug_printf(1, "Access denied!!!!!\n");
     return CI_ACCESS_DENY;
}




/**********************************************************************************/
/* basic auth method implementation                                               */


struct http_basic_auth_data *basic_create_auth_data(char *auth_line,
                                                    char **username);
void basic_release_auth_data(struct http_basic_auth_data *data);


http_auth_method_t basic_auth = {
     "basic",
     NULL,                      /*init */
     NULL,                      /*post_init */
     NULL,
     (void *(*)(char *, char **)) basic_create_auth_data,
     (void (*)(void *)) basic_release_auth_data,
     NULL
};


struct http_basic_auth_data *basic_create_auth_data(char *auth_line,
                                                    char **username)
{
     struct http_basic_auth_data *data;
     char dec_http_user[MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2], *str;
     int max_decode_len = MAX_USERNAME_LEN + HTTP_MAX_PASS_LEN + 2;

     data = malloc(sizeof(struct http_basic_auth_data));

     ci_base64_decode(auth_line, dec_http_user, max_decode_len);
     ci_debug_printf(10, "The proxy user is:%s (basic method) ", dec_http_user);

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



/***********************************************************************/
/*file_basic authenticator .......                                     */

int file_basic_athenticate(struct http_basic_auth_data *data);

authenticator_module_t file_basic = {
     "file_basic",              /* char *name; */
     "basic",                   /*char *method; */
     NULL,                      /* int (*init_authenticator)(); */
     NULL,                      /* int (*post_init_authenticator)(); */
     NULL,                      /*  void (*close_authenticator)(); */
     (int (*)(void *)) file_basic_athenticate,  /* int (*authenticate)(void *data); */
     NULL                       /* struct ci_conf_entry *conf_table; */
};


/*
 This function is under construction ........
*/

int file_basic_athenticate(struct http_basic_auth_data *data)
{

     ci_debug_printf(1, "Going to authenticate user %s:%s\n", data->http_user,
                     data->http_pass);

     if (strcmp(data->http_user, "squiduser") != 0)
          return CI_ACCESS_UNKNOWN;
     if (strcmp(data->http_pass, "123") != 0)
          return CI_ACCESS_DENY;

     ci_debug_printf(1, "User authenticated ........\n");

     return CI_ACCESS_ALLOW;
}
