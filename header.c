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
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include "debug.h"
#include "header.h"


const char *ci_common_headers[] = {
    "Cache-Control",
    "Connection",
    "Date",
    "Expires",
    "Pragma",
    "Trailer",
    "Upgrade",
    /*And ICAP speciffic headers ..... */
    "Encapsulated"
};



const char *ci_methods[] = {
    "",                        /*0x00 */
    "OPTIONS",                 /*0x01 */
    "REQMOD",                  /*0x02 */
    "",                        /*0x03 */
    "RESPMOD"                  /*0x04 */
};


const char *ci_request_headers[] = {
    "Authorization",
    "Allow",
    "From",
    "Host",                    /*REQUIRED ...... */
    "Referer",
    "User-Agent",
    /*And ICAP specific headers ..... */
    "Preview"
};

const char *ci_responce_headers[] = {
    "Server",
    /*ICAP spacific headers */
    "ISTag"
};

const char *ci_options_headers[] = {
    "Methods",
    "Service",
    "ISTag",
    "Encapsulated",
    "Opt-body-type",
    "Max-Connections",
    "Options-TTL",
    "Date",
    "Service-ID",
    "Allow",
    "Preview",
    "Transfer-Preview",
    "Transfer-Ignore",
    "Transfer-Complete"
};


const struct ci_error_code ci_error_codes[] = {
    {100, "Continue"},         /*Continue after ICAP Preview */
    {200, "OK"},
    {204, "Unmodified"},       /*No modifications needed */
    {206, "Partial Content"},  /*Partial content modification*/
    {400, "Bad request"},      /*Bad request */
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Service not found"},        /*ICAP Service not found */
    {405, "Not allowed"},      /*Method not allowed for service (e.g., RESPMOD requested for
                                   service that supports only REQMOD). */
    {407, "Authentication Required"},
    {408, "Request timeout"},  /*Request timeout.  ICAP server gave up waiting for a request
                                   from an ICAP client */
    {500, "Server error"},     /*Server error.  Error on the ICAP server, such as "out of disk
                                   space" */
    {501, "Not implemented"},  /*Method not implemented.  This response is illegal for an
                                   OPTIONS request since implementation of OPTIONS is mandatory. */
    {502, "Bad Gateway"},      /*Bad Gateway.  This is an ICAP proxy and proxying produced an
                                   error. */
    {503, "Service overloaded"},       /*Service overloaded.  The ICAP server has exceeded a maximum
                                           connection limit associated with this service; the ICAP client
                                           should not exceed this limit in the future. */
    {505, "Unsupported version"}       /*ICAP version not supported by server. */
};

/*
#ifdef __CYGWIN__
int ci_error_code(int ec){
     return (ec>=EC_100&&ec<EC_MAX?ci_error_codes[ec].code:1000);
}

const char *unknownerrorcode="UNKNOWN ERROR CODE";

const char *ci_error_code_string(int ec){
     return (ec>=EC_100&&ec<EC_MAX?ci_error_codes[ec].str:unknownerrorcode);
}
#endif
*/


const char *ci_encaps_entities[] = {
    "req-hdr",
    "res-hdr",
    "req-body",
    "res-body",
    "null-body",
    "opt-body"
};

#ifdef __CYGWIN__

const char *unknownentity = "UNKNOWN";
const char *unknownmethod = "UNKNOWN";

const char *ci_method_string(int method)
{
    return (method <= ICAP_RESPMOD
            && method >= ICAP_OPTIONS ? CI_Methods[method] : unknownmethod);
}


const char *ci_encaps_entity_string(int e)
{
    return (e <= ICAP_OPT_BODY
            && e >= ICAP_REQ_HDR ? CI_EncapsEntities[e] : unknownentity);
}
#endif

ci_headers_list_t *ci_headers_create()
{
    ci_headers_list_t *h;
    h = malloc(sizeof(ci_headers_list_t));
    if (!h) {
        ci_debug_printf(1, "Error allocation memory for ci_headers_list_t (header.c: ci_headers_create)\n");
        return NULL;
    }
    h->headers = NULL;
    h->buf = NULL;
    if (!(h->headers = malloc(HEADERSTARTSIZE * sizeof(char *)))
            || !(h->buf = malloc(HEADSBUFSIZE * sizeof(char)))) {
        ci_debug_printf(1, "Server Error: Error allocation memory \n");
        if (h->headers)
            free(h->headers);
        if (h->buf)
            free(h->buf);
        free(h);
        return NULL;
    }

    h->size = HEADERSTARTSIZE;
    h->used = 0;
    h->bufsize = HEADSBUFSIZE;
    h->bufused = 0;
    h->packed = 0;

    return h;
}

void ci_headers_destroy(ci_headers_list_t * h)
{
    free(h->headers);
    free(h->buf);
    free(h);
}



int ci_headers_setsize(ci_headers_list_t * h, int size)
{
    char *newbuf;
    int new_size;
    if (size < h->bufsize)
        return 1;
    /*Allocate buffer of size multiple of HEADSBUFSIZE */
    new_size = (size / HEADSBUFSIZE + 1) * HEADSBUFSIZE;
    newbuf = realloc(h->buf, new_size * sizeof(char));
    if (!newbuf) {
        ci_debug_printf(1, "Server Error:Error allocation memory \n");
        return 0;
    }
    h->buf = newbuf;
    h->bufsize = new_size;
    return 1;
}

void ci_headers_reset(ci_headers_list_t * h)
{
    h->packed = 0;
    h->used = 0;
    h->bufused = 0;
}

const char *ci_headers_add(ci_headers_list_t * h, const char *line)
{
    char *newhead, **newspace, *newbuf;
    int len, linelen;
    int i = 0;

    if (h->packed) { /*Not in edit mode*/
        return NULL;
    }

    if (h->used == h->size) {
        len = h->size + HEADERSTARTSIZE;
        newspace = realloc(h->headers, len * sizeof(char *));
        if (!newspace) {
            ci_debug_printf(1, "Server Error:Error allocation memory \n");
            return NULL;
        }
        h->headers = newspace;
        h->size = len;
    }
    linelen = strlen(line);
    len = h->bufsize;
    while ( len - h->bufused < linelen + 4 )
        len += HEADSBUFSIZE;
    if (len > h->bufsize) {
        newbuf = realloc(h->buf, len * sizeof(char));
        if (!newbuf) {
            ci_debug_printf(1, "Server Error:Error allocation memory \n");
            return NULL;
        }
        h->buf = newbuf;
        h->bufsize = len;
        h->headers[0] = h->buf;
        for (i = 1; i < h->used; i++)
            h->headers[i] = h->headers[i - 1] + strlen(h->headers[i - 1]) + 2;
    }
    newhead = h->buf + h->bufused;
    strcpy(newhead, line);
    h->bufused += linelen + 2; //2 char size for \r\n at the end of each header
    *(newhead + linelen + 1) = '\n';
    *(newhead + linelen + 3) = '\n';
    if (newhead)
        h->headers[h->used++] = newhead;

    return newhead;
}


int ci_headers_addheaders(ci_headers_list_t * h, const ci_headers_list_t * headers)
{
    int len, i;
    char *newbuf, **newspace;

    if (h->packed) { /*Not in edit mode*/
        return 0;
    }
    len = h->size;
    while ( len - h->used < headers->used )
        len += HEADERSTARTSIZE;

    if ( len > h->size ) {
        newspace = realloc(h->headers, len * sizeof(char *));
        if (!newspace) {
            ci_debug_printf(1, "Server Error: Error allocating memory \n");
            return 0;
        }
        h->headers = newspace;
        h->size = len;
    }

    len = h->bufsize;
    while (len - h->bufused < headers->bufused + 2)
        len += HEADSBUFSIZE;
    if (len > h->bufsize) {
        newbuf = realloc(h->buf, len * sizeof(char));
        if (!newbuf) {
            ci_debug_printf(1, "Server Error: Error allocating memory \n");
            return 0;
        }
        h->buf = newbuf;
        h->bufsize = len;
    }

    memcpy(h->buf + h->bufused, headers->buf, headers->bufused + 2);

    h->bufused += headers->bufused;
    h->used += headers->used;

    h->headers[0] = h->buf;
    for (i = 1; i < h->used; i++)
        h->headers[i] = h->headers[i - 1] + strlen(h->headers[i - 1]) + 2;
    return 1;
}

const char *ci_headers_first_line2(ci_headers_list_t *h, size_t *return_size)
{
    const char *eol;
    if (h->used == 0)
        return NULL;

    eol = h->used > 1 ? (h->headers[1] - 1)  : (h->buf + h->bufused);
    while ((eol > h->buf) && (*eol == '\0' || *eol == '\r' || *eol == '\n')) --eol;
    *return_size = eol - h->buf + 1;

    return h->buf;
}

const char *ci_headers_first_line(ci_headers_list_t *h)
{
    if (h->used == 0)
        return NULL;
    return h->buf;
}

static const char *do_header_search(ci_headers_list_t * h, const char *header, const char **value, const char **end)
{
    int i;
    size_t header_size = strlen(header);
    const char *h_end = (h->buf + h->bufused);
    const char *check_head, *lval;

    if (!header_size)
        return NULL;

    for (i = 0; i < h->used; i++) {
        check_head = h->headers[i];
        if (h_end < check_head + header_size)
            return NULL;
        if (*(check_head + header_size) != ':')
            continue;
        if (strncasecmp(check_head, header, header_size) == 0) {
            lval = check_head + header_size + 1;
            if (value)
                *value = lval;
            if (end) {
                *end = (i < h->used -1) ? (h->headers[i + 1] - 1) : (h->buf + h->bufused - 1);
                while ((*end > lval) && (**end == '\0' || **end == '\r' || **end == '\n')) --(*end);
            }
            return check_head;
        }
    }
    return NULL;
}

const char *ci_headers_search(ci_headers_list_t * h, const char *header)
{
    return do_header_search(h, header, NULL, NULL);
}

const char *ci_headers_search2(ci_headers_list_t * h, const char *header, size_t *return_size)
{
    const char *phead, *pend = NULL;
    if ((phead = do_header_search(h, header, NULL, &pend))) {
        *return_size = (pend != NULL) ? (pend - phead + 1) : 0;
        return phead;
    }
    *return_size = 0;
    return NULL;
}

const char *ci_headers_value(ci_headers_list_t * h, const char *header)
{
    const char *pval, *phead;
    pval = NULL;
    if ((phead = do_header_search(h, header, &pval, NULL)))
        return pval;
    return NULL;
}

const char *ci_headers_value2(ci_headers_list_t * h, const char *header, size_t *return_size)
{
    const char *pval, *phead, *pend = NULL;
    pval = NULL;
    if ((phead = do_header_search(h, header, &pval, &pend))) {
        *return_size = (pend != NULL) ? (pend - pval + 1) : 0;
        return pval;
    }
    return NULL;
}

const char *ci_headers_copy_value(ci_headers_list_t * h, const char *header, char *buf, size_t len)
{
    const char *phead = NULL, *pval = NULL, *pend = NULL;
    char *dest, *dest_end;
    phead = do_header_search(h, header, &pval, &pend);
    if (phead == NULL || pval == NULL || pend == NULL)
        return NULL;

    /*skip spaces at the beginning*/
    while (isspace(*pval) && pval < pend)
        pval++;
    while (isspace(*pend) && pend > pval)
        pend--;

    /*copy value to buf*/
    dest = buf;
    dest_end = buf + len -1;
    for (; dest < dest_end && pval <= pend; dest++, pval++)
        *dest = *pval;
    *dest = '\0';
    return buf;
}

int ci_headers_remove(ci_headers_list_t * h, const char *header)
{
    const char *h_end;
    char *phead;
    int i, j, cur_head_size, rest_len;
    size_t header_size;

    if (h->packed) { /*Not in edit mode*/
        return 0;
    }

    h_end = (h->buf + h->bufused);
    header_size = strlen(header);
    for (i = 0; i < h->used; i++) {
        phead = h->headers[i];
        if (h_end < phead + header_size)
            return 0;
        if (*(phead + header_size) != ':')
            continue;
        if (strncasecmp(phead, header, header_size) == 0) {
            /*remove it........ */
            if (i == h->used - 1) {
                phead = h->headers[i];
                *phead = '\r';
                *(phead + 1) = '\n';
                h->bufused = (phead - h->buf);
                (h->used)--;
                return 1;
            } else {
                cur_head_size = h->headers[i + 1] - h->headers[i];
                rest_len =
                    h->bufused - (h->headers[i] - h->buf) - cur_head_size;
                ci_debug_printf(5, "remove_header : remain len %d\n",
                                rest_len);
                memmove(phead, h->headers[i + 1], rest_len);
                /*reconstruct index..... */
                h->bufused -= cur_head_size;
                (h->used)--;
                for (j = i + 1; j < h->used; j++) {
                    cur_head_size = strlen(h->headers[j - 1]);
                    h->headers[j] = h->headers[j - 1] + cur_head_size + 1;
                    if (h->headers[j][0] == '\n')
                        (h->headers[j])++;
                }

                return 1;
            }
        }
    }
    return 0;
}

const char *ci_headers_replace(ci_headers_list_t * h, const char *header, const char *newval)
{
    if (h->packed) /*Not in edit mode*/
        return NULL;

    return NULL;
}

#define eoh(s) ((*s == '\r' && *(s+1) == '\n' && *(s+2) != '\t' && *(s+2) != ' ') || (*s == '\n' && *(s+1) != '\t' && *(s+1) != ' '))

int ci_headers_iterate(ci_headers_list_t * h, void *data, void (*fn)(void *, const char  *head, const char  *value))
{
    char header[256];
    char value[8196];
    char *s;
    int i, j;
    for (i = 0; i < h->used; i++) {
        s = h->headers[i];
        for (j = 0;  j < sizeof(header)-1 && *s != ':' && *s != ' ' &&  *s != '\0' && *s != '\r' && *s!='\n'; s++, j++)
            header[j] = *s;
        header[j] = '\0';
        j = 0;
        if (*s == ':') {
            s++;
        } else {
            header[0] = '\0';
            s = h->headers[i];
        }
        while (*s == ' ') s++;
        for (j = 0;  j < sizeof(value)-1 &&  *s != '\0' && !eoh(s); s++, j++)
            value[j] = *s;
        value[j] = '\0';
        fn(data, header, value);
    }
    return 1;
}

void ci_headers_pack(ci_headers_list_t * h)
{
    /*Put the \r\n sequence at the end of each header before sending...... */
    int i = 0, len = 0;
    for (i = 0; i < h->used; i++) {
        len = strlen(h->headers[i]);
        if (h->headers[i][len + 1] == '\n') {
            h->headers[i][len] = '\r';
            /*         h->headers[i][len+1]='\n';*/
        } else {              /*   handle the case that headers seperated with a '\n' only */
            h->headers[i][len] = '\n';
        }
    }

    if (h->buf[h->bufused + 1] == '\n') {
        h->buf[h->bufused] = '\r';
        /*    h->buf[h->bufused+1]='\n';*/
        h->bufused += 2;
    } else {                   /*   handle the case that headers seperated with a '\n' only */
        h->buf[h->bufused] = '\n';
        h->bufused++;
    }
    h->packed = 1;
}


int ci_headers_unpack(ci_headers_list_t * h)
{
    int len, eoh;
    char **newspace;
    char *ebuf, *str;

    if (h->bufused < 2)        /*???????????? */
        return EC_400;

    ebuf = h->buf + h->bufused - 2;
    /* ebuf now must indicate the last \r\n so: */
    if (*ebuf != '\r' && *ebuf != '\n') {      /*Some sites return (this is bug ) a simple '\n' as end of header ..... */
        ci_debug_printf(3,
                        "Parse error. The end chars are %c %c (%d %d) not the \\r \n",
                        *ebuf, *(ebuf + 1), (unsigned int) *ebuf,
                        (unsigned int) *(ebuf + 1));
        return EC_400;        /*Bad request .... */
    }
    *ebuf = '\0';

    h->headers[0] = h->buf;
    h->used = 1;

    for (str = h->buf; str < ebuf; str++) {    /*Construct index of headers */
        eoh = 0;

        if ((*str == '\r' && *(str + 1) == '\n')) {
            if ((str + 2) >= ebuf
                    || (*(str + 2) != '\t' && *(str + 2) != ' '))
                eoh = 1;
        } else if (*str == '\n' && *(str + 1) != '\t' && *(str + 1) != ' ') {
            /*handle the case that headers seperated with a '\n' only */
            eoh = 1;
        } else if (*str == '\0')      /*Then we have a problem. This char is important for us. Yes can happen! */
            *str = ' ';

        if (eoh) {
            *str = '\0';
            if (h->size <= h->used) {        /*  Resize the headers index space ........ */
                len = h->size + HEADERSTARTSIZE;
                newspace = realloc(h->headers, len * sizeof(char *));
                if (!newspace) {
                    ci_debug_printf(1,
                                    "Server Error: Error allocating memory \n");
                    return EC_500;
                }
                h->headers = newspace;
                h->size = len;
            }
            str++;
            if (*str == '\n')
                str++;      /*   handle the case that headers seperated with a '\n' only */
            h->headers[h->used] = str;
            h->used++;
        }
    }
    h->packed = 0;
    /*OK headers index construction ...... */
    return EC_100;
}

size_t ci_headers_pack_to_buffer(ci_headers_list_t *heads, char *buf, size_t size)
{
    size_t n;
    int i;
    char *pos;

    n = heads->bufused;
    if (!heads->packed)
        n += 2;

    if (n > size)
        return 0;

    memcpy(buf, heads->buf, heads->bufused);

    if (!heads->packed) {
        pos = buf;
        for (i = 0; i < heads->used; ++i) {
            pos = strchr(pos, '\0');
            if (pos[1] == '\n')
                pos[0] = '\r';
            else
                pos[0] = '\n';
        }
        buf[heads->bufused] = '\r';
        buf[heads->bufused+1] = '\n';
    }
    return n;
}

/********************************************************************************************/
/*             Entities List                                                                */


ci_encaps_entity_t *mk_encaps_entity(int type, int val)
{
    ci_encaps_entity_t *h;
    h = malloc(sizeof(ci_encaps_entity_t));
    if (!h)
        return NULL;

    h->start = val;
    h->type = type;
    if (type == ICAP_REQ_HDR || type == ICAP_RES_HDR)
        h->entity = ci_headers_create();
    else
        h->entity = NULL;
    return h;
}

void destroy_encaps_entity(ci_encaps_entity_t * e)
{
    if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR) {
        ci_headers_destroy((ci_headers_list_t *) e->entity);
    } else
        free(e->entity);
    free(e);
}

int get_encaps_type(const char *buf, int *val, char **endpoint)
{

    if (0 == strncmp(buf, "req-hdr", 7)) {
        *val = strtol(buf + 8, endpoint, 10);
        return ICAP_REQ_HDR;
    }
    if (0 == strncmp(buf, "res-hdr", 7)) {
        *val = strtol(buf + 8, endpoint, 10);
        return ICAP_RES_HDR;
    }
    if (0 == strncmp(buf, "req-body", 8)) {
        *val = strtol(buf + 9, endpoint, 10);
        return ICAP_REQ_BODY;
    }
    if (0 == strncmp(buf, "res-body", 8)) {
        *val = strtol(buf + 9, endpoint, 10);
        return ICAP_RES_BODY;
    }
    if (0 == strncmp(buf, "null-body", 9)) {
        *val = strtol(buf + 10, endpoint, 10);
        return ICAP_NULL_BODY;
    }
    return -1;
}


int sizeofheader(ci_headers_list_t * h)
{
    /*
      int size=0,i;
      for(i=0;i<h->used;i++){
        size+=strlen(h->headers[i])+2;
      }
      size+=2;
      return size;
    */
    return h->bufused + 2;
}

int sizeofencaps(ci_encaps_entity_t * e)
{
    if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR) {
        return sizeofheader((ci_headers_list_t *) e->entity);
    }
    return 0;
}
