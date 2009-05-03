#include "common.h"
#include "c-icap.h"
#include "request.h"
#include "debug.h"

#define MAX_VARIABLE_SIZE 256

struct ci_fmt_entry {
    const char *directive;
    const char *description;
    int (*format)(ci_request_t *req_data, char *buf, int len, char *param);
};

int fmt_none(ci_request_t *req_data, char *buf,int len, char *param);


struct ci_fmt_entry GlobalTable [] = {
    { "%a", "Remote IP-Address", fmt_none },
    {"%la", "Local IP Address", fmt_none },
    {"%lp", "Local port", fmt_none},
    {"%>a", "Http Client IP Address", fmt_none},
    {"%<A", "Http Server IP Address", fmt_none},
    {"%ts", "Seconds since epoch", fmt_none},
    {"%tl", "Local time", fmt_none},
    {"%tg", "GMT time", fmt_none},
    {"%tr", "Response time", fmt_none},
    {"%>hi", "Http request header", fmt_none},
    {"%>ho", "Http request header", fmt_none},
    {"%<hi", "Modified Http reply header", fmt_none},
    {"%<ho", "Modified Http reply header", fmt_none},
    {"%Hs", "Http status", fmt_none},
    {"%Hso", "Modified Http status", fmt_none},

    {"%>ih", "Icap request header", fmt_none},
    {"%>ih", "Icap response header", fmt_none},
    
    {"%I", "Bytes received", fmt_none},
    {"%O", "Bytes sent", fmt_none},
    {"%Ih", "Http object bytes received", fmt_none},
    {"%Oh", "Http object bytes sent", fmt_none},
    {"%Ib", "Http body bytes received", fmt_none},
    {"%Ob", "Http body bytes sent", fmt_none},

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

