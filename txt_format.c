/*
 *  Copyright (C) 2004-2010 Christos Tsantilas
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
#include "simple_api.h"
#include "debug.h"
#include "txt_format.h"

#define MAX_VARIABLE_SIZE 256

int fmt_none(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_percent(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_remoteip(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_localip(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_icapstatus(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_icapmethod(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_service(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_username(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_request(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_localtime(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_gmttime(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_seconds(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_httpclientip(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_httpserverip(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_http_req_url_o(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_http_req_head_o(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_http_res_head_o(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_icap_req_head(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_icap_res_head(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_bytes_rcv(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_bytes_sent(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_http_bytes_rcv(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_http_bytes_sent(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_body_bytes_rcv(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_body_bytes_sent(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_preview_hex(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_preview_len(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_logstr(ci_request_t *req_data, char *buf,int len, const char *param);
int fmt_req_attribute(ci_request_t *req_data, char *buf,int len, const char *param);

/**
   \brief Internal formating directives table.
   \ingroup FORMATING
   *
   * This table define the following directives:\n
   * \em "%a":  Remote IP-Address \n
   * \em "%la": Local IP Address \n
   * \em "%lp": Local port \n
   * \em "%>a": Http Client IP Address \n
   * \em "%<A": Http Server IP Address \n
   * \em "%ts": Seconds since epoch \n
   * \em "%tl": Local time \n
   * \em "%tg": GMT time \n
   * \em "%>ho": Modified Http request header \n
   * \em "%huo": Modified Http request url \n
   * \em "%<ho": Modified Http reply header \n
   * \em "%iu": Icap request url \n
   * \em "%im": Icap method \n
   * \em "%is": Icap status code \n
   * \em "%>ih": Icap request header \n
   * \em "%<ih": Icap response header \n
   * \em "%ipl": Icap preview length \n
   * \em "%Ih": Http bytes received \n
   * \em "%Oh": Http bytes sent \n
   * \em "%Ib": Http body bytes received \n
   * \em "%Ob": Http body bytes sent \n
   * \em "%I": Bytes received \n
   * \em "%O": Bytes sent \n
   * \em "%bph": Body data preview \n
   * \em "%un": Username \n
   * \em "%Sl": Log string set by service\n
   * \em "%Sa": Attribute value set by service\n
   *
   * Not yet implemented:\n
   * \em "%tr": Response time \n
   * \em "%hu": Http request url \n
   * \em "%>hi": Http request header \n
   * \em "%<hi": Http reply header \n
   * \em "%Hs": Http reply status \n
   * \em "%Hso": Modified Http reply status \n
 */
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
    {"%huo", "Modified Http request url", fmt_http_req_url_o},
    {"%hu", "Http request url", fmt_none},
    {"%<hi", "Http reply header", fmt_none},
    {"%<ho", "Modified Http reply header", fmt_http_res_head_o},
    {"%Hs", "Http reply status", fmt_none},
    {"%Hso", "Modified Http reply status", fmt_none},

    {"%iu", "Icap request url", fmt_request},
    {"%im", "Icap method", fmt_icapmethod},
    {"%is", "Icap status code", fmt_icapstatus},
    {"%>ih", "Icap request header", fmt_icap_req_head},
    {"%<ih", "Icap response header", fmt_icap_res_head},
    {"%ipl", "Icap preview length", fmt_req_preview_len},

    {"%Ih", "Http bytes received", fmt_req_http_bytes_rcv},
    {"%Oh", "Http bytes sent", fmt_req_http_bytes_sent},
    {"%Ib", "Http body bytes received", fmt_req_body_bytes_rcv},
    {"%Ob", "Http body bytes sent", fmt_req_body_bytes_sent},

    {"%I", "Bytes received", fmt_req_bytes_rcv},
    {"%O", "Bytes sent", fmt_req_bytes_sent},

    {"%bph", "Body data preview", fmt_req_preview_hex},
    {"%un", "Username", fmt_username},
    {"%Sl", "Service log string", fmt_logstr},
    {"%Sa", "Attribute set by service", fmt_req_attribute},
    {"%%", "% sign", fmt_percent},
    { NULL, NULL, NULL}
};

int fmt_none(ci_request_t *req, char *buf,int len, const char *param)
{
    if (!len)
        return 0;

    *buf = '-';
    return 1;
}

int fmt_percent(ci_request_t *req, char *buf,int len, const char *param)
{
    if (!len)
        return 0;

    *buf = '%';
    return 1;
}

unsigned int parse_directive(const char *var,
                             unsigned int *width,
                             int *left_align, char *parameter)
{
    const char *s1;
    int i = 0;
    char *e;
    s1 = var+1;
    parameter[0] = '\0';

    if (s1[0] == '-') {
        *left_align = 1;
        s1++;
    } else
        *left_align = 0;

    *width = strtol(s1, &e, 10);
    if (e == s1) {
        *width = 0;
    } else
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
    return  s1-var;
}

int check_directive( const char *var, const char *directive, int *directive_len)
{
    const char *s1, *s2;
    s1 = var;
    s2 = directive+1;
    *directive_len = 0;

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
    unsigned int params_len;
    params_len = parse_directive(var, width, left_align, parameter);
    for (i = 0; GlobalTable[i].directive; i++) {
        if (check_directive(var+params_len,GlobalTable[i].directive, directive_len)) {
            *directive_len += params_len;
            return &GlobalTable[i];
        }
    }
    if (u_table) {
        for (i = 0; u_table[i].directive; i++) {
            if (check_directive(var+params_len, u_table[i].directive, directive_len)) {
                *directive_len += params_len;
                return &u_table[i];
            }
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
    unsigned int width, space = 0;
    char parameter[MAX_VARIABLE_SIZE];

    lb = NULL;
    s = fmt;
    b = buffer;
    remains = len - 1;
    while (*s && remains > 0) {
        if (*s == '%') {
            fmte = check_tables(s, user_table, &directive_len,
                                &width, &left_align, parameter);
            ci_debug_printf(7,"Width: %d, Parameter:%s\n", width, parameter);
            if (width != 0)
                space = width = (remains<width?remains:width);
            else
                space = remains;
            if (fmte != NULL) {
                if (width) {
                    if (left_align) {
                        val_len = fmte->format(req_data, b, space, parameter);
                        if (val_len <= 0) val_len = fmt_none(req_data, b, space, parameter);

                        if (val_len > space) val_len = space;
                        b += val_len;
                        for (i = 0; i < width-val_len; i++) b[i]=' ';
                        b += width-val_len;
                    } else if ((lb = malloc((space+1)*sizeof(char))) != NULL) {
                        val_len = fmte->format(req_data, lb, space, parameter);
                        if (val_len <= 0) val_len = fmt_none(req_data, lb, space, parameter);

                        if (val_len > space) val_len = space;
                        for (i = 0; i < width-val_len; i++) b[i] = ' ';
                        b += width-val_len;
                        for (i = 0; i < val_len; i++) b[i] = lb[i];
                        b += val_len;
                        free(lb);
                        lb = NULL;
                    }
                    /*else allocation failed! Just ignore?????*/

                    remains -= width;
                } else {
                    val_len = fmte->format(req_data, b, space, parameter);
                    if (val_len <= 0) val_len = fmt_none(req_data, b, space, parameter);

                    if (val_len > space) val_len = space;
                    b += val_len;
                    remains -= val_len;
                }
                s += directive_len;
            } else
                *b++ = *s++, remains--;
        } else
            *b++ = *s++, remains--;
    }
    *b = '\0';
    return len-remains;
}


/******************************************************************/

int fmt_remoteip(ci_request_t *req, char *buf,int len, const char *param)
{
    if (len<CI_IPLEN)
        return 0;

    if (!ci_conn_remote_ip(req->connection, buf))
        strcpy(buf, "-" );

    return strlen(buf);
}

int fmt_localip(ci_request_t *req, char *buf,int len, const char *param)
{
    if (len<CI_IPLEN)
        return 0;

    if (!ci_conn_local_ip(req->connection, buf))
        strcpy(buf, "-" );

    return strlen(buf);
}

int fmt_icapmethod(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    const char *s = ci_method_string(req->type);
    for (i = 0; i < len && *s; i++,s++)
        buf[i] = *s;
    return i;
}

int fmt_service(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    char *s = req->service;
    for (i = 0; i < len && *s; i++,s++)
        buf[i] = *s;
    return i;
}

int fmt_username(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    char *s = req->user;
    for (i = 0; i < len && *s; i++,s++)
        buf[i] = *s;
    return i;
}

int fmt_request(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    char *s = req->service;
    for (i = 0;  i< len && *s; i++,s++)
        buf[i] = *s;

    if (req->args[0] != '\0' && i < len) {
        buf[i] = '?';
        s = req->args;
        i++;
        for (; i < len && *s; i++,s++)
            buf[i] = *s;
    }
    return i;
}

int fmt_localtime(ci_request_t *req, char *buf,int len, const char *param)
{
    struct tm tm;
    time_t t;
    const char *tfmt = "%d/%b/%Y:%H:%M:%S %z";

    if (!len)
        return 0;

    if (param && param[0] != '\0') {
        tfmt = param;
    }
    t = time(&t);
    localtime_r(&t, &tm);
    return strftime(buf, len, tfmt, &tm);
}

int fmt_gmttime(ci_request_t *req, char *buf,int len, const char *param)
{
    struct tm tm;
    time_t t;
    const char *tfmt = "%d/%b/%Y:%H:%M:%S";

    if (!len)
        return 0;

    if (param && param[0] != '\0') {
        tfmt = param;
    }
    t = time(&t);
    gmtime_r(&t, &tm);
    return strftime(buf, len, tfmt, &tm);
}

int fmt_icapstatus(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%d", ci_error_code(req->return_code));
}


int fmt_seconds(ci_request_t *req, char *buf,int len, const char *param)
{
    time_t tm;
    time(&tm);
    return snprintf(buf, len, "%ld", tm);
}

int fmt_httpclientip(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s;
    int i;
    if (!len)
        return 0;

    if ((s = ci_headers_value(req->request_header, "X-Client-IP")) != NULL) {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }

}

int fmt_httpserverip(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s;
    int i;
    if (!len)
        return 0;

    if ((s = ci_headers_value(req->request_header, "X-Server-IP")) != NULL) {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }
}

int fmt_http_req_url_o(ci_request_t *req, char *buf,int len, const char *param)
{
    if (!len)
        return 0;

    return ci_http_request_url(req, buf, len);
}

int fmt_http_req_head_o(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s = NULL;
    int i;
    if (!len)
        return 0;

    if (!param || param[0] == '\0') {
        s = ci_http_request(req);
    } else {
        s = ci_http_request_get_header(req, param);
    }

    if (s)  {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }
}

int fmt_http_res_head_o(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s = NULL;
    int i;
    ci_headers_list_t *http_resp_headers;

    if (!len)
        return 0;

    if (!param || param[0] == '\0') {
        http_resp_headers = ci_http_response_headers(req);
        if (http_resp_headers && http_resp_headers->used)
            s = http_resp_headers->headers[0];
    } else {
        s = ci_http_response_get_header(req, param);
    }

    if (s) {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }
}


int fmt_icap_req_head(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s = NULL;
    int i;
    if (!len)
        return 0;

    if (!param || param[0] == '\0') {
        if (req->request_header && req->request_header->used)
            s = req->request_header->headers[0];
    } else {
        s = ci_headers_value(req->request_header, param);
    }

    if (s) {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }
}

int fmt_icap_res_head(ci_request_t *req, char *buf,int len, const char *param)
{
    const char *s = NULL;
    int i;
    if (!len)
        return 0;

    if (!param || param[0] == '\0') {
        if (req->response_header && req->response_header->used)
            s = req->response_header->headers[0];
    } else {
        s = ci_headers_value(req->response_header, param);
        /*if not found try xheaders which will also sent to user*/
        if (!s && req->xheaders)
            s = ci_headers_value(req->xheaders, param);
    }

    if (s) {
        for (i = 0; i < len && *s != '\0' && *s != '\r' && *s != '\n'; i++,s++)
            buf[i] = *s;
        return i;
    } else {
        *buf = '-';
        return 1;
    }
}


int fmt_req_bytes_rcv(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->bytes_in);
}

int fmt_req_bytes_sent(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->bytes_out);
}

int fmt_req_http_bytes_rcv(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->http_bytes_in);
}

int fmt_req_http_bytes_sent(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->http_bytes_out);
}

int fmt_req_body_bytes_rcv(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->body_bytes_in);
}

int fmt_req_body_bytes_sent(ci_request_t *req, char *buf,int len, const char *param)
{
    return snprintf(buf, len, "%" PRINTF_OFF_T , (CAST_OFF_T) req->body_bytes_out);
}

int fmt_req_preview_hex(ci_request_t *req, char *buf,int len, const char *param)
{
    int  i, num, n, bytes;

    if (!len)
        return 0;

    if (req->preview_data.used <= 0) {
        *buf = '-';
        return 1;
    }

    if (param) {
        num = strtol(param, NULL, 10);
    } else
        num = 5;
    n = 0;
    for (i = 0; i < num && i < req->preview_data.used && len > 0; i++) {
        if (req->preview_data.buf[i] >= ' ' && req->preview_data.buf[i] <= '~') {
            buf[n++] = req->preview_data.buf[i];
            len --;
        } else {
            bytes = snprintf(buf+n, len, "\\x%X",0xFF & (buf[i]));
            if (bytes > len) bytes = len;
            n += bytes;
            len -= bytes;
        }
    }
    return n;
}

int fmt_req_preview_len(ci_request_t *req, char *buf, int len, const char *param)
{
    if (!len)
        return 0;

    if (req->preview >= 0)
        return snprintf(buf, len, "%d", req->preview_data.used);

    *buf = '-';
    return 1;
}

int fmt_logstr(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    const char *s;

    if (!req->log_str)
        return 0;

    s = req->log_str;
    for (i = 0; i < len && *s; i++,s++)
        buf[i] = *s;
    return i;
}

int fmt_req_attribute(ci_request_t *req, char *buf,int len, const char *param)
{
    int i;
    const char *s;
    if (!req->attributes)
        return 0;
    if (! (s =ci_str_array_search(req->attributes, param)))
        return 0;

    for (i = 0; i < len && *s; i++, s++)
        buf[i] = *s;
    return i;
}
