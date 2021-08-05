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


#ifndef __C_ICAP_BODY_H
#define __C_ICAP_BODY_H

#include "c-icap.h"
#include "debug.h"
#include "util.h"
#include "array.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CI_MEMBUF_NULL_TERMINATED     0x01
#define CI_MEMBUF_HAS_EOF             0x02
#define CI_MEMBUF_RO                  0x04
#define CI_MEMBUF_CONST               0x08
#define CI_MEMBUF_FOREIGN_BUF         0x10
#define CI_MEMBUF_LOCKED              0x20

/*Flags can be set by user: */
#define CI_MEMBUF_USER_FLAGS (CI_MEMBUF_NULL_TERMINATED | CI_MEMBUF_RO)
#define CI_MEMBUF_FROM_CONTENT_FLAGS (CI_MEMBUF_NULL_TERMINATED | CI_MEMBUF_RO | CI_MEMBUF_CONST | CI_MEMBUF_HAS_EOF)

typedef struct ci_membuf {
    size_t endpos;
    size_t readpos;
    size_t bufsize;
    size_t unlocked;
    unsigned int flags;
    char *buf;
    ci_array_t *attributes;
} ci_membuf_t;

CI_DECLARE_FUNC(struct ci_membuf *) ci_membuf_new();
CI_DECLARE_FUNC(struct ci_membuf *) ci_membuf_new_sized(size_t size);
CI_DECLARE_FUNC(struct ci_membuf *) ci_membuf_from_content(char *buf, size_t buf_size, size_t content_size, unsigned int flags);
CI_DECLARE_FUNC(void) ci_membuf_free(struct ci_membuf *);
CI_DECLARE_FUNC(int) ci_membuf_write(struct ci_membuf *body, const char *buf, size_t len, int iseof);
CI_DECLARE_FUNC(int) ci_membuf_read(struct ci_membuf *body, char *buf, size_t len);
CI_DECLARE_FUNC(int) ci_membuf_attr_add(struct ci_membuf *body,const char *attr, const void *val, size_t val_size);
CI_DECLARE_FUNC(const void *) ci_membuf_attr_get(struct ci_membuf *body,const char *attr);
CI_DECLARE_FUNC(int) ci_membuf_truncate(struct ci_membuf *body, size_t new_size);
CI_DECLARE_FUNC(unsigned int) ci_membuf_set_flag(struct ci_membuf *body, unsigned int flag);

static inline void ci_membuf_lock_all(ci_membuf_t *body) {
    _CI_ASSERT(body);
    body->flags |= CI_MEMBUF_LOCKED;
    body->unlocked = 0;
}

static inline void  ci_membuf_unlock(ci_membuf_t *body, size_t len) {
    _CI_ASSERT(body);
    body->unlocked = ((body->readpos) > len ? (body->readpos) : len);
}

static inline void ci_membuf_unlock_all(ci_membuf_t *body) {
    _CI_ASSERT(body);
    body->flags &= ~CI_MEMBUF_LOCKED;
    body->unlocked = 0;
}

static inline const char *ci_membuf_raw(ci_membuf_t *body) {
    _CI_ASSERT(body);
    return body->buf;
}

static inline int ci_membuf_size(ci_membuf_t *body) {
    _CI_ASSERT(body);
    return body->endpos;
}

static inline int ci_membuf_flag(ci_membuf_t *body, unsigned int flag) {
    _CI_ASSERT(body);
    return body->flags & flag;
}

/*****************************************************************/
/* Cached file functions and structure                           */

#define CI_FILE_USELOCK    0x01
#define CI_FILE_HAS_EOF    0x02
#define CI_FILE_RING_MODE  0x04
#define CI_FILE_SHARED     0x08

typedef struct ci_cached_file {
    ci_off_t endpos;
    ci_off_t readpos;
    int      bufsize;
    int flags;
    ci_off_t unlocked;
    char *buf;
    int fd;
    char filename[CI_FILENAME_LEN+1];
    ci_array_t *attributes;
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


#define ci_cached_file_lock_all(body)     (body->flags |= CI_FILE_USELOCK,body->unlocked = 0)
#define ci_cached_file_unlock(body, len) (body->unlocked = ((body->readpos) > len ? (body->readpos) : len))
#define ci_cached_file_unlock_all(body)   (body->flags &= ~CI_FILE_USELOCK,body->unlocked = 0)
#define ci_cached_file_size(body)            (body->endpos)
#define ci_cached_file_ismem(body)           (body->fd < 0)
#define ci_cached_file_read_pos(body)      (body->readpos)
#define ci_cached_file_haseof(body)        (body->flags & CI_FILE_HAS_EOF)

/*****************************************************************/
/* simple file function and structures                           */

typedef struct ci_simple_file {
    ci_off_t endpos;
    ci_off_t readpos;
    ci_off_t max_store_size;
    ci_off_t bytes_in;
    ci_off_t bytes_out;
    unsigned int flags;
    ci_off_t unlocked;
    int fd;
    char filename[CI_FILENAME_LEN+1];
    ci_array_t *attributes;
    char *mmap_addr;
    ci_off_t mmap_size;
} ci_simple_file_t;


CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_new(ci_off_t maxsize);
CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_named_new(char *tmp,char*filename,ci_off_t maxsize);

CI_DECLARE_FUNC(void) ci_simple_file_release(ci_simple_file_t *);
CI_DECLARE_FUNC(void) ci_simple_file_destroy(ci_simple_file_t *body);
CI_DECLARE_FUNC(int) ci_simple_file_write(ci_simple_file_t *body, const char *buf, size_t len, int iseof);
CI_DECLARE_FUNC(int) ci_simple_file_read(ci_simple_file_t *body, char *buf, size_t len);
CI_DECLARE_FUNC(int) ci_simple_file_truncate(ci_simple_file_t *body, ci_off_t new_size);

/*Currently it is just creates a MAP_PRIVATE memory.
  Currently the CI_MEMBUF_CONST and CI_MEMBUF_NULL_TERMINATED flags
  are supported. The CI_MEMBUF_CONST flag is required.
  Works only if USE_POSIX_MAPPED_FILES is defined
*/
CI_DECLARE_FUNC(ci_membuf_t *) ci_simple_file_to_membuf(ci_simple_file_t *body, unsigned int flags);
CI_DECLARE_FUNC(const char *) ci_simple_file_to_const_string(ci_simple_file_t *body);
CI_DECLARE_FUNC(const char *) ci_simple_file_to_const_raw_data(ci_simple_file_t *body, size_t *data_size);

static inline void ci_simple_file_lock_all(ci_simple_file_t *body) {
    _CI_ASSERT(body);
    body->flags |= CI_FILE_USELOCK;
    body->unlocked = 0;
}

static inline void ci_simple_file_unlock(ci_simple_file_t *body, ci_off_t len) {
    _CI_ASSERT(body);
    body->unlocked = ((body->readpos) > len ? (body->readpos) : len);
}

static inline void ci_simple_file_unlock_all(ci_simple_file_t *body) {
    _CI_ASSERT(body);
    body->flags &= ~CI_FILE_USELOCK;
    body->unlocked = 0;
}

static inline ci_off_t ci_simple_file_size(ci_simple_file_t *body) {
    _CI_ASSERT(body);
    return body->endpos;
}

static inline int ci_simple_file_haseof(ci_simple_file_t *body) {
    _CI_ASSERT(body);
    return (body->flags & CI_FILE_HAS_EOF);
}


/*******************************************************************/
/*ring memory buffer functions and structures                      */


typedef struct ci_ring_buf {
    char *buf;
    char *end_buf;
    char *read_pos;
    char *write_pos;
    int full;
} ci_ring_buf_t;


CI_DECLARE_FUNC(struct ci_ring_buf *) ci_ring_buf_new(size_t size);
CI_DECLARE_FUNC(void) ci_ring_buf_destroy(struct ci_ring_buf *buf);
CI_DECLARE_FUNC(int) ci_ring_buf_write(struct ci_ring_buf *buf, const char *data, size_t size);
CI_DECLARE_FUNC(int) ci_ring_buf_read(struct ci_ring_buf *buf, char *data, size_t size);

/*low level functions for ci_ring_buf*/
CI_DECLARE_FUNC(int) ci_ring_buf_write_direct(struct ci_ring_buf *buf, char **wb, size_t *len);
CI_DECLARE_FUNC(int) ci_ring_buf_read_direct(struct ci_ring_buf *buf, char **rb, size_t *len);
CI_DECLARE_FUNC(void) ci_ring_buf_consume(struct ci_ring_buf *buf, size_t len);
CI_DECLARE_FUNC(void) ci_ring_buf_produce(struct ci_ring_buf *buf, size_t len);

/*The following functions are deprecated : */
CI_DECLARE_FUNC(int) ci_ring_buf_write_block(struct ci_ring_buf *buf, char **wb, int *len);
CI_DECLARE_FUNC(int) ci_ring_buf_read_block(struct ci_ring_buf *buf, char **rb, int *len);

#ifdef __cplusplus
}
#endif

#endif
