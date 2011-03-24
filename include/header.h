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


#ifndef __HEADERS_H
#define __HEADERS_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup HEADERS  Headers related API
 \ingroup API
 * Headers manipulation related API.
 */

enum ci_request_headers { ICAP_AUTHORIZATION, ICAP_ALLOW,
                       ICAP_FROM, ICAP_HOST, ICAP_REFERER,
                       ICAP_USERAGENT,ICAP_PREVIEW
                      };


extern const char *ci_common_headers[];
extern const char *ci_request_headers[];
extern const char *ci_responce_headers[];
extern const char *ci_options_headers[];


enum ci_encapsulated_entities {ICAP_REQ_HDR, ICAP_RES_HDR,
			       ICAP_REQ_BODY, ICAP_RES_BODY,
			       ICAP_NULL_BODY,ICAP_OPT_BODY };
CI_DECLARE_DATA extern const char *ci_encaps_entities[];

#ifdef __CYGWIN__

const char *ci_encaps_entity_string(int e);

#else

#define ci_encaps_entity_string(e) (e<=ICAP_OPT_BODY&&e>=ICAP_REQ_HDR?ci_encaps_entities[e]:"UNKNOWN")

#endif 

/**
 \typedef ci_headers_list_t
 \ingroup HEADERS
 * This is a struct which can store a set of headers.
 * The developers should not touch ci_headers_list_t objects directly but better use the
 * documented macros and functions
 */
typedef struct ci_headers_list{
     int size;
     int used;
     char **headers;
     int bufsize;
     int bufused;
     char *buf;
     int packed;
} ci_headers_list_t;


typedef struct ci_encaps_entity{
     int start;
     int type;
     void *entity;
} ci_encaps_entity_t;


#define BUFSIZE          4096
#define HEADERSTARTSIZE  64
#define HEADSBUFSIZE     BUFSIZE
#define MAX_HEADER_SIZE  1023

#define ci_headers_not_empty(h) ((h)->used)
#define ci_headers_is_empty(h) ((h)->used==0)

/**
 * Allocate memory for a ci_headers_list_t object and initialize it. 
 \ingroup HEADERS
 \return the allocated object on success, NULL otherwise.
 *
 */
CI_DECLARE_FUNC(ci_headers_list_t *) ci_headers_create();

/**
 * Destroy a ci_headers_list_t object
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object to be destroyed
 *
 */
CI_DECLARE_FUNC(void)    ci_headers_destroy(ci_headers_list_t *heads);

/**
 * Resets and initialize a ci_headers_list_t object
 \ingroup HEADERS
 \param heads pointer to the ci_headers_list_t object to be reset
 *
 */
CI_DECLARE_FUNC(void)    ci_headers_reset(ci_headers_list_t *heads);

CI_DECLARE_FUNC(int)     ci_headers_setsize(ci_headers_list_t *heads, int size);

/**
 * Add a header to a ci_headers_list_t object
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object in which the header will be added
 \param header is the header to be added
 \return Pointer to the newly add header on success, NULL otherwise
 *
 *example usage:
 \code
  ci_headers_add(heads,"Content-Length: 1025")
 \endcode
 *
 */
CI_DECLARE_FUNC(const char *)  ci_headers_add(ci_headers_list_t *heads, const char *header);

/**
 * Append a  headers list object to an other headers list
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object in which the headers will be added
 \param someheaders is a ci_headers_list_t object which contains the headers will be added to the heads
 \return non zero on success zero otherwise
 */
CI_DECLARE_FUNC(int)  ci_headers_addheaders(ci_headers_list_t *heads,const ci_headers_list_t *someheaders);

/**
 * Removes a header from a header list
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object
 \param header is the name of the header to be removed
 \return non zero on success, zero otherwise
 *
 *example usage:
 \code
  ci_headers_remove(heads,"Content-Length")
 \endcode
 *
 */
CI_DECLARE_FUNC(int)     ci_headers_remove(ci_headers_list_t *heads, const char *header);

/**
 * Search for a header in a header list
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object
 \param header is the name of the header
 \return a pointer to the start of the first occurrence of the header on success, NULL otherwise
 *
 *example usage:
 \code
 char *head;
 head=ci_headers_search(heads,"Content-Length")
 \endcode
 * In this example on success the head pointer will point to a \em "Content-Lenght: 1025" string
 *
 */
CI_DECLARE_FUNC(const char *)  ci_headers_search(ci_headers_list_t *heads, const char *header);

/**
 * Search for a header in a header list and return the value of the first occurrence of this header
 \ingroup HEADERS
 \param heads is a pointer to the ci_headers_list_t object
 \param header is the name of the header
 \return a pointer to the start of the header on success, NULL otherwise
 *
 *example usage:
 \code
 char *headval;
 int content_length;
 headval = ci_headers_value(heads,"Content-Length");
 content_length = strtol(headval,NULL,10);
 \endcode
 *
 */
CI_DECLARE_FUNC(const char *)  ci_headers_value(ci_headers_list_t *heads, const char *header);

/*The following headers are only used internally */
CI_DECLARE_FUNC(void) ci_headers_pack(ci_headers_list_t *heads);
CI_DECLARE_FUNC(int)  ci_headers_unpack(ci_headers_list_t *heads);
CI_DECLARE_FUNC(int)  sizeofheader(ci_headers_list_t *heads);

CI_DECLARE_FUNC(ci_encaps_entity_t) *mk_encaps_entity(int type,int val);
CI_DECLARE_FUNC(void) destroy_encaps_entity(ci_encaps_entity_t *e);
CI_DECLARE_FUNC(int) get_encaps_type(const char *buf,int *val,char **endpoint);
CI_DECLARE_FUNC(int)  sizeofencaps(ci_encaps_entity_t *e);

#ifdef __CI_COMPAT
#define ci_headers_make ci_header_create
#endif

#ifdef __cplusplus
}
#endif

#endif
