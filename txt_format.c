#include "common.h"
#include "c-icap.h"
#include "request.h"
#include "simple_api.h"
#include "debug.h"

#define MAX_VARIABLE_SIZE 256

struct ci_fmt_entry {
    const char *directive;
    const char *description;
    int (*format)(ci_request_t *req_data, char *buf, int len, char *param);
};

int fmt_none(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_remoteip(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_localip(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_icapstatus(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_icapmethod(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_service(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_request(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_localtime(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_gmttime(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_seconds(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_httpclientip(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_httpserverip(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_http_req_head_o(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_http_res_head_o(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_icap_req_head(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_icap_res_head(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_bytes_rcv(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_bytes_sent(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_http_bytes_rcv(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_http_bytes_sent(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_body_bytes_rcv(ci_request_t *req_data, char *buf,int len, char *param);
int fmt_req_body_bytes_sent(ci_request_t *req_data, char *buf,int len, char *param);


struct ci_fmt_entry GlobalTable [] = {
    { "%a", "Remote IP-Address", fmt_remoteip },
    {"%la", "Local IP Address", fmt_localip },
    {"%lp", "Local port", fmt_none},
    {"%>a", "Http Client IP Address", fmt_httpclientip},
    {"%<A", "Http Server IP Address", fmt_httpserverip},
    {"%ts", "Seconds since epoch", fmt_seconds},
    {"%tl", "Local time", fmt_localtime},
    {"%tg", "GMT time", fmt_gmttime},
    {"%tr", "Response time", fmt_none},
    {"%>hi", "Http request header", fmt_none},
    {"%>ho", "Modified Http request header", fmt_http_req_head_o},
    {"%<hi", "Http reply header", fmt_none},
    {"%<ho", "Modified Http reply header", fmt_http_res_head_o},
    {"%Hs", "Http status", fmt_none},
    {"%Hso", "Modified Http status", fmt_none},

    {"%iu", "Icap request url", fmt_request},
    {"%im", "Icap method", fmt_icapmethod},
    {"%is", "Icap status code", fmt_icapstatus},
    {"%>ih", "Icap request header", fmt_icap_req_head},
    {"%<ih", "Icap response header", fmt_icap_res_head},
    
    {"%Ih", "Http bytes received", fmt_req_http_bytes_rcv},
    {"%Oh", "Http bytes sent", fmt_req_http_bytes_sent},
    {"%Ib", "Http body bytes received", fmt_req_body_bytes_rcv},
    {"%Ob", "Http body bytes sent", fmt_req_body_bytes_sent},

    {"%I", "Bytes received", fmt_req_bytes_rcv},
    {"%O", "Bytes sent", fmt_req_bytes_sent},

    {"%un", "Username", fmt_none}, 
    { NULL, NULL, NULL} 
};

int fmt_none(ci_request_t *req, char *buf,int len, char *param)
{
  *buf = '-';
   return 1;
}


int check_directive( const char *var, const char *directive, 
                     int *directive_len, unsigned int *width,
                     int *left_align, char *parameter)
{
   const char *s1, *s2;
   int i = 0;
   char *e;
   s1 = var+1;
   s2 = directive+1;
   *directive_len = 0;
   parameter[0] = '\0';

   if (s1[0] == '-') {
       *left_align = 1;
       s1++;
   } 
   else
       *left_align = 0;

   *width = strtol(s1, &e, 10);
   if (e == s1) {
      *width = 0;
   }
   else
      s1 = e;
  
   if (*s1 == '{') {
       s1++;
       i = 0;
       while (*s1 && *s1!='}' && i < MAX_VARIABLE_SIZE -1 ) {
            parameter[i] = *s1;
            i++,s1++;
       }
       if (*s1 != '}')
           return 0;

       parameter[i] = '\0'; 
       s1++;
   }

   while (*s2) {
       if (!s1)
          return 0;
       if (*s1 != *s2)
          return 0;
        s1++,s2++;
   }
   *directive_len = s1-var;
   return 1; 
}

struct ci_fmt_entry *check_tables(const char *var, struct ci_fmt_entry *u_table, int *directive_len, unsigned int *width, int *left_align, char *parameter)
{
   int i;
   for (i=0; GlobalTable[i].directive; i++) {
       if(check_directive(var,GlobalTable[i].directive, directive_len, width, left_align, parameter))
           return &GlobalTable[i];
   }
   if (u_table) {
     for (i=0; u_table[i].directive; i++) {
       if(check_directive(var, u_table[i].directive, directive_len, width, left_align, parameter))
           return &u_table[i];
     }
   }
   return NULL;
}

int ci_format_text(
                 ci_request_t *req_data,
                 const char *fmt,
                 char *buffer, int len,
                 struct ci_fmt_entry *user_table)
{
   const char *s;
   char *b, *lb;
   struct ci_fmt_entry *fmte;
   int directive_len, val_len, remains, left_align, i;
   unsigned int width, space=0;
   char parameter[MAX_VARIABLE_SIZE];

   s = fmt;
   b = buffer;
   remains = len;
   while (*s && remains > 0) {
     if (*s == '%') {
       fmte = check_tables(s, user_table, &directive_len, 
                           &width, &left_align, parameter);
       ci_debug_printf(7,"Width: %d, Parameter:%s\n", width, parameter);
       if (width != 0) 
            space = width = (remains<width?remains:width);
       else
            space = remains;
       if(fmte != NULL) { 
            if (width) {
                if (left_align) {
                    val_len=fmte->format(req_data, b, space, parameter); 
                    b += val_len;
                    for (i=0; i < width-val_len; i++) b[i]=' ';
                    b += width-val_len;
                }
                else {
                    lb = malloc((space+1)*sizeof(char));      
                    val_len=fmte->format(req_data, lb, space, parameter);
                    for (i=0; i < width-val_len; i++) b[i]=' ';
                    b += width-val_len; 
                    for (i=0; i < val_len; i++) b[i]=lb[i];
                    b += val_len;
                }

                remains -= width;
            } 
             else {
		 val_len=fmte->format(req_data, b, space, parameter);
                if (val_len > space) {
                    ci_debug_printf(1,"format_line BUG! Please contact authors!!!\n");
                    return 0;
                }
                b += val_len; 
                remains -= val_len;
             }
            s += directive_len;
       }
       else
         *b++ = *s++, remains--; 
     }
     else 
        *b++ = *s++, remains--;
   }
   *b = '\0';
   return len-remains;
}


/******************************************************************/

int fmt_remoteip(ci_request_t *req, char *buf,int len, char *param)
{
    if (len<CI_IPLEN)
	return 0;

    if (!ci_conn_remote_ip(req->connection, buf))
          strcpy(buf, "-" );

    return strlen(buf);
}

int fmt_localip(ci_request_t *req, char *buf,int len, char *param)
{
    if (len<CI_IPLEN)
        return 0;

    if (!ci_conn_local_ip(req->connection, buf))
          strcpy(buf, "-" );

    return strlen(buf);
}

int fmt_icapmethod(ci_request_t *req, char *buf,int len, char *param)
{
   int i;
   const char *s = ci_method_string(req->type);
   for(i=0;i<len && *s;i++,s++)
        buf[i] = *s;
   return i;
}

int fmt_service(ci_request_t *req, char *buf,int len, char *param)
{
   int i;
   char *s = req->service;
   for(i=0;i<len && *s;i++,s++)
        buf[i] = *s;
   return i;
}

int fmt_request(ci_request_t *req, char *buf,int len, char *param)
{
   int i;
   char *s = req->service;
   for(i=0;i<len && *s;i++,s++)
        buf[i] = *s;

   if (req->args[0]!='\0' && i<len) {
      buf[i] = '?';
      s = req->args;
      i++;
      for(;i<len && *s;i++,s++)
        buf[i] = *s;
   }
   return i;
}

int fmt_localtime(ci_request_t *req, char *buf,int len, char *param)
{
    struct tm tm;
    time_t t;
    char *tfmt = "%d/%b/%Y:%H:%M:%S %z";
    if (param && param[0]!='\0') {
        tfmt = param;
    }
    t = time(&t);
    localtime_r(&t, &tm);
    return strftime(buf, len, tfmt, &tm);
}

int fmt_gmttime(ci_request_t *req, char *buf,int len, char *param)
{
    struct tm tm;
    time_t t;
    char *tfmt = "%d/%b/%Y:%H:%M:%S";
    if (param && param[0]!='\0') {
        tfmt = param;
    }
    t = time(&t);
    gmtime_r(&t, &tm);
    return strftime(buf, len, tfmt, &tm);
}

int fmt_icapstatus(ci_request_t *req, char *buf,int len, char *param)
{
   return snprintf(buf, len, "%d", ci_error_code(req->return_code));
}


int fmt_seconds(ci_request_t *req, char *buf,int len, char *param)
{
   time_t tm;
   time(&tm);
   return snprintf(buf, len, "%ld", tm);
}

int fmt_httpclientip(ci_request_t *req, char *buf,int len, char *param)
{
    char *s;
  int i;
  if (!len)
     return 0;

  if ((s = ci_headers_value(req->request_header, "X-Client-IP")) != NULL) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }

}

int fmt_httpserverip(ci_request_t *req, char *buf,int len, char *param)
{
  char *s;
  int i;
  if (!len)
     return 0;

  if ((s = ci_headers_value(req->request_header, "X-Server-IP")) != NULL) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }
}

int fmt_http_req_head_o(ci_request_t *req, char *buf,int len, char *param)
{
  char *s;
  int i;
  if (!len)
     return 0;

  if(param && (s = ci_http_request_get_header(req, param))) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }
   
}

int fmt_http_res_head_o(ci_request_t *req, char *buf,int len, char *param)
{
  char *s;
  int i;
  if (!len)
     return 0;

  if(param && (s = ci_http_response_get_header(req, param))) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }
}


int fmt_icap_req_head(ci_request_t *req, char *buf,int len, char *param)
{
  char *s;
  int i;
  if (!len)
     return 0;

  if(param && (s = ci_headers_value(req->request_header, param))) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }
}

int fmt_icap_res_head(ci_request_t *req, char *buf,int len, char *param)
{
  char *s;
  int i;
  if (!len)
     return 0;

  if(param && (s = ci_headers_value(req->response_header, param))) {
     for(i=0;i<len && *s!= '\0' && *s != '\r' && *s!='\n'; i++,s++)
        buf[i] = *s;
     return i;
  }
  else {
     *buf = '-';
     return 1;
  }
}


int fmt_req_bytes_rcv(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->bytes_in);
}

int fmt_req_bytes_sent(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->bytes_out);
}

int fmt_req_http_bytes_rcv(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->http_bytes_in);
}

int fmt_req_http_bytes_sent(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->http_bytes_out);
}

int fmt_req_body_bytes_rcv(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->body_bytes_in);
}

int fmt_req_body_bytes_sent(ci_request_t *req, char *buf,int len, char *param) {
    return snprintf(buf, len, "%" PRINTF_OFF_T , req->body_bytes_out);
}


