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
#include "debug.h"
#include "header.h"
#include <ctype.h>

const char *ci_methods[] = {
    "UNKNOWN",                 /*0x00 */
    "OPTIONS",                 /*0x01 */
    "REQMOD",                  /*0x02 */
    "UNKNOWN",                 /*0x03 */
    "RESPMOD"                  /*0x04 */
};

const struct ci_status_code ci_error_codes[] = {
    {100, "Continue"},         /*Continue after ICAP Preview */
    {200, "OK"},
    {204, "No Content"},       /*No modifications needed */
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

const char *ci_encaps_entities[] = {
    "req-hdr",
    "res-hdr",
    "req-body",
    "res-body",
    "null-body",
    "opt-body"
};

const char *ci_method_string_non_inline(int method)
{
    return ci_method_string_inline(method);
}

int ci_status_code_non_inline(int ec)
{
    return ci_status_code_inline(ec);
}

const char * ci_status_code_string_non_inline(int ec)
{
    return ci_status_code_string_inline(ec);
}

int ci_headers_empty_non_inline(const ci_headers_list_t *h)
{
    return ci_headers_empty_inline(h);
}

ci_headers_list_t *ci_headers_create2(int num, size_t buf_size)
{
    ci_headers_list_t *h;
    h = malloc(sizeof(ci_headers_list_t));
    if (!h) {
        ci_debug_printf(1, "Error allocation memory for ci_headers_list_t (header.c: ci_headers_create)\n");
        return NULL;
    }
    h->headers = NULL;
    h->buf = NULL;
    assert(buf_size > 0);
    assert(num > 0);
    if (!(h->headers = malloc(num * sizeof(char *)))
            || !(h->buf = malloc(buf_size))) {
        ci_debug_printf(1, "Server Error: Error allocation memory \n");
        if (h->headers)
            free(h->headers);
        if (h->buf)
            free(h->buf);
        free(h);
        return NULL;
    }

    h->size = num;
    h->used = 0;
    h->bufsize = buf_size;
    h->bufused = 0;
    h->packed = 0;

    return h;
}

ci_headers_list_t *ci_headers_create()
{
    return ci_headers_create2(HEADERSTARTSIZE, HEADSBUFSIZE);
}

void ci_headers_destroy(ci_headers_list_t * h)
{
    if (!h)
        return;
    free(h->headers);
    free(h->buf);
    free(h);
}



int ci_headers_setsize(ci_headers_list_t * h, int size)
{
    char *newbuf;
    int new_size;
    assert(h);
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
    assert(h);
    h->packed = 0;
    h->used = 0;
    h->bufused = 0;
}


static int headers_check_space(ci_headers_list_t * h, int new_headers, int new_space)
{
    int i;
    int required_index_space = h->size;
    while(new_headers > required_index_space - h->used) {
        required_index_space = required_index_space + HEADERSTARTSIZE;
        if (required_index_space > 2048)
            return 0; // just a limit in the case of a bug
    }

    if (required_index_space > h->size) {
        char *newspace = realloc(h->headers, required_index_space * sizeof(char *));
        if (!newspace) {
            ci_debug_printf(1, "Server Error:Error allocation memory \n");
            return 0;
        }
        h->headers = (char **)newspace;
        h->size = required_index_space;
    }
    int required_mem_space = h->bufsize;
    while (required_mem_space - h->bufused < new_space + 4 )
        required_mem_space += HEADSBUFSIZE;
    if (required_mem_space > h->bufsize) {
        char *newbuf = realloc(h->buf, required_mem_space * sizeof(char));
        if (!newbuf) {
            ci_debug_printf(1, "Server Error:Error allocation memory \n");
            return 0;
        }
        char *old_buf = h->buf;
        h->buf = newbuf;
        h->bufsize = required_mem_space;
        // Rebuild index:
        for (i = 0; i < h->used; i++) {
            size_t pos = h->headers[i] - old_buf;
            h->headers[i] = h->buf + pos;
        }
    }
    return 1;
}

static int headers_terminated(const ci_headers_list_t *h)
{
    if (h->bufused < 4)
        return 0;
    int ok =
        (h->buf[h->bufused - 1] == '\n') &&
        (h->buf[h->bufused - 2] == '\r') &&
        (h->buf[h->bufused - 3] == '\n');
    ok = ok &&
        ((h->packed && h->buf[h->bufused - 4] == '\r') ||
         (!h->packed && h->buf[h->bufused - 4] == '\0'));
    return ok;
}

static int headers_terminate(ci_headers_list_t *h)
{
    if (!h->used) {
        if (h->bufused)
            h->bufused = 0;
        return 1;
    }

    int needs_fix = h->bufused < 4;
    if (h->bufused >= 4) {
        needs_fix = needs_fix ||
            (h->buf[h->bufused - 1] != '\n') ||
            (h->buf[h->bufused - 2] != '\r') ||
            (h->buf[h->bufused - 3] != '\n') ||
            (h->buf[h->bufused - 4] != '\r' && h->buf[h->bufused - 4] != '\0');
    }
    if (!needs_fix)
        return 1;

    int i = 0;
    while((h->bufused - i) >= 0 && strchr("\r\n\0", h->buf[h->bufused - i - 1]))
        i++;
    if ((h->bufused - i + 4) > h->bufsize)
        return 0; // Not enough space to terminate
    h->bufused -= i;
    h->buf[h->bufused++] = h->packed ? '\r' : '\0';
    h->buf[h->bufused++] = '\n';
    h->buf[h->bufused++] = '\r';
    h->buf[h->bufused++] = '\n';
    return 1;
}

const char *ci_headers_add(ci_headers_list_t * h, const char *line)
{
    char *newhead;
    int linelen;

    assert(h);
    if (h->packed) { /*Not in edit mode*/
        return NULL;
    }

    linelen = strlen(line);
    if (!headers_check_space(h, 1, linelen + 2))
        return NULL;

    assert(headers_terminate(h));
    if (h->used) {
        assert(h->bufused >= 4);
        h->bufused -= 2;
    } else {
        assert(h->bufused == 0);
    }
    newhead = h->buf + h->bufused;
    memcpy(newhead, line, linelen);
    newhead[linelen] = h->packed ? '\r' : '\0';
    *(newhead + linelen + 1) = '\n';
    *(newhead + linelen + 2) = '\r';
    *(newhead + linelen + 3) = '\n';
    h->bufused = h->bufused + linelen + 4;
    assert(h->bufused <= h->bufsize);
    if (newhead)
        h->headers[h->used++] = newhead;

    return newhead;
}


int ci_headers_addheaders(ci_headers_list_t * h, const ci_headers_list_t * headers)
{
    int i;
    assert(h);
    if (h->packed || headers->packed) { /*Not in edit mode*/
        return 0;
    }
    if (!headers_check_space(h, headers->used, headers->bufused))
        return 0;
    assert(headers_terminate(h));
    if (h->used) {
        assert(h->bufused >= 4);
        h->bufused -= 2;
    } else {
        assert(h->bufused == 0);
    }
    char *pos = h->buf + h->bufused;
    memcpy(pos, headers->buf, headers->bufused);
    h->bufused += headers->bufused;
    for (i = 0; i < headers->used; ++i) {
        int hdr_indx = headers->headers[i] - headers->buf;
        h->headers[h->used++] = pos + hdr_indx;
    }
    assert(headers_terminate(h));
    return 1;
}

const char *ci_headers_first_line2(const ci_headers_list_t *h, size_t *return_size)
{
    const char *eol;
    assert(h);
    if (h->used == 0)
        return NULL;

    eol = h->used > 1 ? (h->headers[1] - 1)  : (h->buf + h->bufused);
    while ((eol > h->buf) && (*eol == '\0' || *eol == '\r' || *eol == '\n')) --eol;
    *return_size = eol - h->buf + 1;

    return h->buf;
}

const char *ci_headers_first_line(const ci_headers_list_t *h)
{
    assert(h);
    if (h->used == 0)
        return NULL;
    return h->buf;
}

static const char *do_header_search(const ci_headers_list_t * h, const char *header, const char **value, const char **end)
{
    int i;
    size_t header_size;
    const char *h_end;
    const char *check_head, *lval;

    header_size = strlen(header);
    if (!header_size)
        return NULL;

    assert(h);
    h_end = h->buf + h->bufused;
    for (i = 0; i < h->used; i++) {
        check_head = h->headers[i];
        if (h_end < check_head + header_size)
            return NULL;
        if (*(check_head + header_size) != ':')
            continue;
        if (strncasecmp(check_head, header, header_size) == 0) {
            lval = check_head + header_size + 1;
            if (value) {
                while (lval <= h_end && (*lval == ' ' || *lval == '\t'))
                    ++(lval);
                *value = lval;
            }
            if (end) {
                *end = (i < h->used -1) ? (h->headers[i + 1] - 1) : (h->buf + h->bufused - 1);
                if (*end < lval) /*parse error in headers ?*/
                    return NULL;
                while ((*end > lval) && (**end == '\0' || **end == '\r' || **end == '\n')) --(*end);
            }
            return check_head;
        }
    }
    return NULL;
}

const char *ci_headers_search(const ci_headers_list_t * h, const char *header)
{
    return do_header_search(h, header, NULL, NULL);
}

const char *ci_headers_search2(const ci_headers_list_t * h, const char *header, size_t *return_size)
{
    const char *phead, *pend = NULL;
    if ((phead = do_header_search(h, header, NULL, &pend))) {
        *return_size = (pend != NULL) ? (pend - phead + 1) : 0;
        return phead;
    }
    *return_size = 0;
    return NULL;
}

const char *ci_headers_value(const ci_headers_list_t * h, const char *header)
{
    const char *pval, *phead;
    pval = NULL;
    if ((phead = do_header_search(h, header, &pval, NULL)))
        return pval;
    return NULL;
}

const char *ci_headers_value2(const ci_headers_list_t * h, const char *header, size_t *return_size)
{
    const char *pval, *phead, *pend = NULL;
    pval = NULL;
    if ((phead = do_header_search(h, header, &pval, &pend))) {
        *return_size = (pend != NULL) ? (pend - pval + 1) : 0;
        return pval;
    }
    return NULL;
}

const char *ci_headers_copy_value(const ci_headers_list_t * h, const char *header, char *buf, size_t len)
{
    const char *phead = NULL, *pval = NULL, *pend = NULL;
    char *dest, *dest_end;
    phead = do_header_search(h, header, &pval, &pend);
    if (phead == NULL || pval == NULL || pend == NULL)
        return NULL;

    /*skip spaces at the beginning*/
    while (isspace((int)*pval) && pval < pend)
        pval++;
    while (isspace((int)*pend) && pend > pval)
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

    assert(h);
    if (h->packed) { /*Not in edit mode*/
        return 0;
    }

    // The following two lines should not needed
    // TODO: replace with a simple "headers_terminated" check
    headers_check_space(h, 0, 0);
    assert(headers_terminate(h));

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
                h->bufused = (phead - h->buf + 2);
                assert(h->bufused <= h->bufsize);
                (h->used)--;
            } else {
                cur_head_size = h->headers[i + 1] - h->headers[i];
                rest_len =
                    h->bufused - (h->headers[i] - h->buf) - cur_head_size;
                assert(rest_len > 0);
                ci_debug_printf(5, "remove_header : remain len %d\n",
                                rest_len);
                memmove(phead, h->headers[i + 1], rest_len);
                /*reconstruct index..... */
                h->bufused -= cur_head_size;
                (h->used)--;
                for (j = i; j < h->used; j++) {
                    h->headers[j] = h->headers[j + 1] - cur_head_size;
                }
            }
            assert(headers_terminate(h));
            return 1;
        }
    }
    return 0;
}

const char *ci_headers_replace(ci_headers_list_t * h, const char *header, const char *newval)
{
    assert(h);
    if (h->packed) /*Not in edit mode*/
        return NULL;

    return NULL;
}

#define eoh(s) ((*s == '\r' && *(s+1) == '\n' && *(s+2) != '\t' && *(s+2) != ' ') || (*s == '\n' && *(s+1) != '\t' && *(s+1) != ' '))

int ci_headers_iterate(const ci_headers_list_t * h, void *data, void (*fn)(void *, const char  *head, const char  *value))
{
    char header[256];
    char value[8196];
    char *s;
    int i, j;
    assert(h);
    for (i = 0; i < h->used; i++) {
        s = h->headers[i];
        for (j = 0;  j < sizeof(header)-1 && *s != ':' && *s != ' ' &&  *s != '\0' && *s != '\r' && *s != '\n'; s++, j++)
            header[j] = *s;
        header[j] = '\0';
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
    int i = 0;
    assert(h);
    if (h->packed)
        return;
    if (h->bufused == 0 || h->used == 0)
        return;

    // The following two lines should not needed
    // TODO: replace with a simple "headers_terminated" check
    headers_check_space(h, 0, 0);
    assert(headers_terminate(h));

    for (i = 1; i < h->used; i++) {
        size_t hpos =  h->headers[i] - h->buf;
        assert(hpos < h->bufused);
        assert(hpos >= 1);
        if (h->buf[hpos - 1] == '\0')
            h->buf[hpos - 1] = '\n';
        if (hpos >= 2 && h->buf[hpos - 2] == '\0')
            h->buf[hpos - 2] = '\r';
    }

    assert(h->bufused >= 4);
    assert(h->buf[h->bufused - 1] == '\n');
    assert(h->buf[h->bufused - 2] == '\r');
    assert(h->buf[h->bufused - 3] == '\n');
    h->buf[h->bufused - 4] = '\r';
    h->packed = 1;
}


int ci_headers_unpack(ci_headers_list_t * h)
{
    int len, eoh;
    char **newspace;
    char *ebuf, *str;

    assert(h);
    int unParsed = h->bufused > 0 && h->used == 0;
    if (!h->packed && !unParsed)
        return EC_100;
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
    //handle the case the empty header "\r\n\r\n" is already accounted in h->bufused
    while(ebuf - 1 > h->buf && (*(ebuf - 1) == '\r' || *(ebuf - 1) =='\n')) ebuf--;
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
            if (*str == '\n') /* if headers separated with a '\n' only this is false */
                str++;
            h->headers[h->used] = str;
            h->used++;
        }
    }
    h->packed = 0;
    /*OK headers index construction ...... */
    headers_check_space(h, 0, 0);
    assert(headers_terminate(h));
    return EC_100;
}

size_t ci_headers_pack_to_buffer(const ci_headers_list_t *heads, char *buf, size_t size)
{
    int i;
    assert(heads);
    if (heads->used == 0 || heads->bufused == 0)
        return 0;
    if (!headers_terminated(heads)) {
        ci_debug_printf(1, "Headers corrupted? can not pack to buffer\n");
        return 0;
    }
    if (size < heads->bufused) {
        ci_debug_printf(1, "Not enough space, can not pack to buffer\n");
        return 0;
    }
    memcpy(buf, heads->buf, heads->bufused);
    if (!heads->packed) {
        for (i = 1; i < heads->used; ++i) {
            size_t hpos =  heads->headers[i] - heads->buf;
            assert(hpos < heads->bufused);
            assert(hpos >= 1);
            if (buf[hpos - 1] == '\0')
                buf[hpos - 1] = '\n';
            if (hpos >= 2 && buf[hpos - 2] == '\0')
                buf[hpos - 2] = '\r';
        }
        assert(heads->bufused >= 4);
        assert(buf[heads->bufused - 1] == '\n');
        assert(buf[heads->bufused - 2] == '\r');
        assert(buf[heads->bufused - 3] == '\n');
        buf[heads->bufused - 4] = '\r';
    }
    return heads->bufused;
}

ci_headers_list_t *ci_headers_clone(const ci_headers_list_t *head)
{
    int i;
    ci_headers_list_t *cloned = ci_headers_create2(head->size, head->bufsize);
    if (!cloned) {
        ci_debug_printf(1, "ci_headers_clone: memory allocation failure\n");
    }
    memcpy(cloned->buf, head->buf, head->bufused);
    cloned->bufused = head->bufused;
    cloned->packed = head->packed;
    cloned->used = head->used;
    for (i = 0; i < head->used; i++) {
        size_t hpos = head->headers[i] - head->buf;
        cloned->headers[i] = cloned->buf + hpos;
    }
    return cloned;
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


int sizeofheader(const ci_headers_list_t * h)
{
    /*
      int size=0,i;
      for(i=0;i<h->used;i++){
        size+=strlen(h->headers[i])+2;
      }
      size+=2;
      return size;
    */
    return h->bufused;
}

int sizeofencaps(const ci_encaps_entity_t * e)
{
    if (e->type == ICAP_REQ_HDR || e->type == ICAP_RES_HDR) {
        return sizeofheader((ci_headers_list_t *) e->entity);
    }
    return 0;
}
