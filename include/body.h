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


#ifndef __BODY_H
#define __BODY_H

#include "c-icap.h"
#include <stdio.h>
#include "util.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ci_membuf{
     int len;
     int endpos;
     int readpos;
     int bufsize;
     int hasalldata;
     char *buf;
} ci_membuf_t;

CI_DECLARE_FUNC(struct ci_membuf) * ci_membuf_new();
CI_DECLARE_FUNC(struct ci_membuf) * ci_membuf_new_sized(int size);
CI_DECLARE_FUNC(void) ci_membuf_free(struct ci_membuf *);
CI_DECLARE_FUNC(int) ci_membuf_write(struct ci_membuf *body, const char *buf,int len, int iseof);
CI_DECLARE_FUNC(int) ci_membuf_read(struct ci_membuf *body,char *buf,int len);


/*****************************************************************/
/* Cached file functions and structure                           */

#define CI_FILE_USELOCK    0x01
#define CI_FILE_HAS_EOF    0x02
#define CI_FILE_RING_MODE  0x04

typedef struct ci_cached_file{
     ci_off_t endpos;
     ci_off_t readpos;
     int      bufsize;
     int flags;
     ci_off_t unlocked;
     char *buf;
     int fd;
     char filename[CI_FILENAME_LEN+1];
} ci_cached_file_t;


CI_DECLARE_DATA extern int  CI_BODY_MAX_MEM;
CI_DECLARE_DATA extern char *CI_TMPDIR;

CI_DECLARE_FUNC(ci_cached_file_t) * ci_cached_file_new(int size);
CI_DECLARE_FUNC(void) ci_cached_file_destroy(ci_cached_file_t *);
CI_DECLARE_FUNC(int) ci_cached_file_write(ci_cached_file_t *body,
					  const char *buf,int len, int iseof);
CI_DECLARE_FUNC(int) ci_cached_file_read(ci_cached_file_t *body,char *buf,int len);
CI_DECLARE_FUNC(void) ci_cached_file_reset(ci_cached_file_t *body,int new_size);
CI_DECLARE_FUNC(void) ci_cached_file_release(ci_cached_file_t *body);


#define ci_cached_file_lock_all(body)     (body->flags|=CI_FILE_USELOCK,body->unlocked=0)
#define ci_cached_file_unlock(body, len) (body->unlocked=len)
#define ci_cached_file_unlock_all(body)   (body->flags&=~CI_FILE_USELOCK,body->unlocked=0)
#define ci_cached_file_size(body)            (body->endpos)
#define ci_cached_file_ismem(body)           (body->fd<0)
#define ci_cached_file_read_pos(body)      (body->readpos)
#define ci_cached_file_haseof(body)        (body->flags&CI_FILE_HAS_EOF)

/*****************************************************************/
/* simple file function and structures                           */

typedef struct ci_simple_file{
     ci_off_t endpos;
     ci_off_t readpos;
     ci_off_t max_store_size;
     ci_off_t bytes_in;
     ci_off_t bytes_out;
     unsigned int flags;
     ci_off_t unlocked;
     int fd;
     char filename[CI_FILENAME_LEN+1];
} ci_simple_file_t;


CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_new(ci_off_t maxsize);
CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_named_new(char *tmp,char*filename,ci_off_t maxsize);

CI_DECLARE_FUNC(void) ci_simple_file_release(ci_simple_file_t *);
CI_DECLARE_FUNC(void) ci_simple_file_destroy(ci_simple_file_t *body);
CI_DECLARE_FUNC(int) ci_simple_file_write(ci_simple_file_t *body,
					  const char *buf,int len, int iseof);
CI_DECLARE_FUNC(int) ci_simple_file_read(ci_simple_file_t *body,char *buf,int len);


#define ci_simple_file_lock_all(body)            (body->flags|=CI_FILE_USELOCK,body->unlocked=0)
#define ci_simple_file_unlock(body, len)     (body->unlocked=len)
#define ci_simple_file_unlock_all(body)      (body->flags&=~CI_FILE_USELOCK,body->unlocked=0)
#define ci_simple_file_size(body)            (body->endpos)
#define ci_simple_file_haseof(body)        (body->flags&CI_FILE_HAS_EOF)


/*******************************************************************/
/*ring memory buffer functions and structures                      */


typedef struct ci_ring_buf {
    char *buf;
    char *end_buf;
    char *read_pos;
    char *write_pos;
    int full;
} ci_ring_buf_t;


CI_DECLARE_FUNC(struct ci_ring_buf *) ci_ring_buf_new(int size);
CI_DECLARE_FUNC(void) ci_ring_buf_destroy(struct ci_ring_buf *buf);
CI_DECLARE_FUNC(int) ci_ring_buf_write(struct ci_ring_buf *buf, const char *data,int size);
CI_DECLARE_FUNC(int) ci_ring_buf_read(struct ci_ring_buf *buf, char *data,int size);

/*low level functions for ci_ring_buf*/
CI_DECLARE_FUNC(int) ci_ring_buf_write_block(struct ci_ring_buf *buf, char **wb, int *len);
CI_DECLARE_FUNC(int) ci_ring_buf_read_block(struct ci_ring_buf *buf, char **rb, int *len);
CI_DECLARE_FUNC(void) ci_ring_buf_consume(struct ci_ring_buf *buf, int len);
CI_DECLARE_FUNC(void) ci_ring_buf_produce(struct ci_ring_buf *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
