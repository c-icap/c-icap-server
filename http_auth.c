#include "c-icap.h"
#include "request.h"
#include "debug.h"




/*The folowing function extract the pair user-password from http request headers*/
/*Not used (yet)*/
int get_http_authorized_data(request_t *req,struct http_auth_data *auth_data){
     char *str;
     char dec_http_user[256];
     
     if((str=ci_req_reqmod_get_header(req,"Proxy-Authorization"))!=NULL){
	  if(strncasecmp(str,"basic",5)==0)
	       auth_data->http_auth_method=AUTH_BASIC;
	  else 	  if(strncasecmp(str,"digest",6)==0)
	       auth_data->http_auth_method=AUTH_DIGEST;
	  else{
	       auth_data->http_auth_method=AUTH_NONE;
	       return 0;
	  }

	  str=strchr(str,' ');	
	  if(str){
	       str++;
	       if(auth_data->http_auth_method==AUTH_BASIC){
		    ci_base64_decode(str,dec_http_user,256);
		    debug_printf(10,"The proxy user is:%s  method is %d\n",
				 dec_http_user, auth_data->http_auth_method);
		    if((str=strchr(dec_http_user,':'))!=NULL){
			 *str='\0';
			 str++;
			 strncpy(auth_data->http_pass,str,MAX_PASS_LEN);
			 auth_data->http_pass[MAX_PASS_LEN-1]='\0';
		    }
		    strncpy(auth_data->http_user,dec_http_user,MAX_USERNAME_LEN);
		    auth_data->http_user[MAX_USERNAME_LEN-1]='\0';
	       }
	       else
		    debug_printf(3,"Unsupported authorization scheme (digest)......\n");
	  }
	  else{
	       dec_http_user[0]='\0'; 
	       auth_data->http_user[0]='\0';
	       auth_data->http_pass[0]='\0';
	  }
     }
     return 1;
}


