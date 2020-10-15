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
#include "body.h"
#include "debug.h"
#include "request_util.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#if defined USE_POSIX_MAPPED_FILES
#include <sys/mman.h>
#endif


#define STARTLEN 8192           /*8*1024*1024 */
#define INCSTEP  4096

#define min(x,y) ((x)>(y)?(y):(x))
#define max(x,y) ((x)>(y)?(x):(y))

static int MEMBUF_POOL = -1;
static int CACHED_FILE_POOL = -1;
static int SIMPLE_FILE_POOL = -1;
static int RING_BUF_POOL = -1;

CI_DECLARE_FUNC(int) init_body_system()
{
    MEMBUF_POOL = ci_object_pool_register("ci_membuf_t",
                                          sizeof(ci_membuf_t));
    if (MEMBUF_POOL < 0)
        return CI_ERROR;

    CACHED_FILE_POOL = ci_object_pool_register("ci_cached_file_t",
                       sizeof(ci_cached_file_t));
    if (CACHED_FILE_POOL < 0)
        return CI_ERROR;

    SIMPLE_FILE_POOL = ci_object_pool_register("ci_simple_file_t",
                       sizeof(ci_simple_file_t));
    if (SIMPLE_FILE_POOL < 0)
        return CI_ERROR;

    RING_BUF_POOL = ci_object_pool_register("ci_ring_buf_t",
                                            sizeof(ci_ring_buf_t));
    if (RING_BUF_POOL < 0)
        return CI_ERROR;

    return CI_OK;
}

void release_body_system()
{
    ci_object_pool_unregister(MEMBUF_POOL);
    ci_object_pool_unregister(CACHED_FILE_POOL);
    ci_object_pool_unregister(SIMPLE_FILE_POOL);
    ci_object_pool_unregister(RING_BUF_POOL);
}

struct ci_membuf *ci_membuf_new()
{
    return ci_membuf_new_sized(STARTLEN);
}

struct ci_membuf *ci_membuf_new_sized(size_t size)
{
    struct ci_membuf *b;
    b = ci_object_pool_alloc(MEMBUF_POOL);
    if (!b)
        return NULL;

    b->endpos = 0;
    b->readpos = 0;
    b->flags = 0;
    b->buf = ci_buffer_alloc(size * sizeof(char));
    if (b->buf == NULL) {
        ci_object_pool_free(b);
        return NULL;
    }
    b->bufsize = size;
    b->unlocked = 0;
    b->attributes = NULL;
    return b;
}

struct ci_membuf *ci_membuf_from_content(char *buf, size_t buf_size, size_t content_size, unsigned int flags)
{
    struct ci_membuf *b;

    if (!buf || buf_size <= 0 || buf_size < content_size) {
        ci_debug_printf(1, "ci_membuf_from_content: Wrong arguments: %p, of size=%u and content size=%u\n", buf, (unsigned int)buf_size, (unsigned int)content_size);
        return NULL;
    }

    if ((flags & CI_MEMBUF_FROM_CONTENT_FLAGS) != flags) {
        ci_debug_printf(1, "ci_membuf_from_content: Wrong flags: %u\n", flags);
        return NULL;
    }

    if ((flags & CI_MEMBUF_NULL_TERMINATED)) {
        if (buf[content_size - 1] == '\0')
            content_size--;
        else if (content_size >= buf_size || buf[content_size] != '\0') {
            ci_debug_printf(1, "ci_membuf_from_content: content is not NULL terminated!\n");
            return NULL;
        }
    }

    b = ci_object_pool_alloc(MEMBUF_POOL);
    if (!b) {
        ci_debug_printf(1, "ci_membuf_from_content: memory allocation failed\n");
        return NULL;
    }

    b->flags = CI_MEMBUF_FOREIGN_BUF | flags;
    b->endpos = content_size;
    b->readpos = 0;
    b->buf = buf;
    b->bufsize = buf_size;
    b->unlocked = 0;
    b->attributes = NULL;
    return b;
}

void ci_membuf_free(struct ci_membuf *b)
{
    if (!b)
        return;
    if (b->buf && !(b->flags & CI_MEMBUF_FOREIGN_BUF))
        ci_buffer_free(b->buf);
    if (b->attributes)
        ci_array_destroy(b->attributes);
    ci_object_pool_free(b);
}

unsigned int ci_membuf_set_flag(struct ci_membuf *body, unsigned int flag)
{
    if (!(flag & CI_MEMBUF_USER_FLAGS))
        return 0;

    assert(body);
    body->flags |= flag;
    return body->flags;
}

int ci_membuf_write(struct ci_membuf *b, const char *data, size_t len, int iseof)
{
    size_t has_space, required_space, newsize;
    char *newbuf;
    int terminate;

    assert(b);
    terminate = b->flags & CI_MEMBUF_NULL_TERMINATED;

    if ((b->flags & CI_MEMBUF_RO) || (b->flags & CI_MEMBUF_CONST)) {
        ci_debug_printf(1, "ci_membuf_write: can not write: buffer is read-only!\n");
        return 0;
    }

    if ((b->flags & CI_MEMBUF_HAS_EOF)) {
        if (len > 0) {
            ci_debug_printf(1, "Cannot write to membuf: the eof flag is set!\n");
        }
        return 0;
    }

    if (iseof) {
        b->flags |= CI_MEMBUF_HAS_EOF;
        /*    ci_debug_printf(10,"Buffer size=%d, Data size=%d\n ",
                       ((struct membuf *)b)->bufsize,((struct membuf *)b)->endpos);
        */
    }

    assert(b->bufsize >= b->endpos);
    has_space = b->bufsize- b->endpos;
    required_space = len + ((terminate ? 1 : 0));

    if (has_space < required_space) {
        /*adjust required space to be at least INCSTEP*/
        if (required_space < INCSTEP)
            required_space = INCSTEP;
        newsize = b->bufsize + (required_space - has_space);
        newbuf = ci_buffer_realloc(b->buf, newsize);
        if (newbuf == NULL) {
            ci_debug_printf(1, "ci_membuf_write: Failed to grow membuf for new data!\n");
            return 0;
        }
        b->buf = newbuf;
        b->bufsize = newsize;
    }                          /*while remains<len */
    if (len) {
        memcpy(b->buf + b->endpos, data, len);
        b->endpos += len;
    }
    if (terminate)
        b->buf[b->endpos] = '\0';

    return len;
}

int ci_membuf_read(struct ci_membuf *b, char *data, size_t len)
{
    size_t remains, copybytes;

    assert(b);
    assert(b->endpos >= b->readpos);

    if (b->readpos == b->endpos && (b->flags & CI_MEMBUF_HAS_EOF))
        return CI_EOF;

    if (b->flags & CI_MEMBUF_LOCKED) {
        assert(b->unlocked <= b->endpos);
        remains = (b->unlocked > b->readpos) ? (b->unlocked - b->readpos) : 0;
    } else {
        remains = b->endpos - b->readpos;
    }
    copybytes = (len <= remains ? len : remains);
    if (copybytes) {
        memcpy(data, b->buf + b->readpos, copybytes);
        b->readpos += copybytes;
    }

    return (int)copybytes;
}

#define BODY_ATTRS_SIZE 1024
int ci_membuf_attr_add(struct ci_membuf *body,const char *attr, const void *val, size_t val_size)
{
    assert(body);
    if (!body->attributes)
        body->attributes = ci_array_new(BODY_ATTRS_SIZE);

    if (body->attributes)
        return (ci_array_add(body->attributes, attr, val, val_size) != NULL);

    return 0;
}

const void * ci_membuf_attr_get(struct ci_membuf *body,const char *attr)
{
    assert(body);
    if (body->attributes)
        return ci_array_search(body->attributes, attr);
    return NULL;
}

int ci_membuf_truncate(struct ci_membuf *body, size_t new_size)
{
    assert(body);
    if (body->endpos < new_size)
        return 0;
    body->endpos = new_size;

    if (body->flags & CI_MEMBUF_NULL_TERMINATED)
        body->buf[body->endpos] = '\0';

    if (body->readpos > body->endpos)
        body->readpos = body->endpos;

    if (body->unlocked > body->endpos)
        body->unlocked = body->endpos;

    return 1;
}

/****/
/*To be removed after ci_cached_file removed*/
static int do_write_old(int fd, const void *buf, size_t count)
{
    int bytes;
    errno = 0;
    do {
        bytes = write(fd, buf, count);
    } while (bytes < 0 && errno == EINTR);

    return bytes;
}

static int do_write(int fd, const void *buf, size_t count, ci_off_t pos)
{
    int bytes;
    errno = 0;
#if !defined(HAVE_PREAD)
    lseek(fd, pos, SEEK_SET);
#endif
    do {
#if !defined(HAVE_PREAD)
        bytes = write(fd, buf, count);
#else
        bytes = pwrite(fd, buf, count, pos);
#endif
    } while (bytes < 0 && errno == EINTR);

    return bytes;
}

static int do_read(int fd, void *buf, size_t count, ci_off_t pos)
{
    int bytes;
    errno = 0;
#if !defined(HAVE_PREAD)
    lseek(fd, pos, SEEK_SET);
#endif
    do {
#if !defined(HAVE_PREAD)
        bytes = read(fd, buf, count);
#else
        bytes = pread(fd, buf, count, pos);
#endif
    } while (bytes < 0 && errno == EINTR);

    return bytes;
}

#ifdef _WIN32
#define F_PERM S_IREAD|S_IWRITE
#else
#define F_PERM S_IREAD|S_IWRITE|S_IRGRP|S_IROTH
#endif

static int do_open(const char *pathname, int flags)
{
    int fd;
    errno = 0;
    do {
        fd = open(pathname, flags, F_PERM);
    } while (fd < 0 && errno == EINTR);

    return fd;
}

static void do_close(int fd)
{
    errno = 0;
    while (close(fd) < 0 && errno == EINTR);
}

/**************************************************************************/
/*                                                                        */
/*                                                                        */

#define tmp_template "CI_TMP_XXXXXX"

/*
extern int  BODY_MAX_MEM;
extern char *TMPDIR;
*/

int CI_BODY_MAX_MEM = 131072;
char *CI_TMPDIR = "/var/tmp/";

/*
int open_tmp_file(char *tmpdir,char *filename){
     return  ci_mktemp_file(tmpdir,tmp_template,filename);
}
*/

int resize_buffer(ci_cached_file_t * body, int new_size)
{
    char *newbuf;

    if (new_size < body->bufsize)
        return 1;
    if (new_size > CI_BODY_MAX_MEM)
        return 0;

    newbuf = ci_buffer_realloc(body->buf, new_size);
    if (newbuf) {
        body->buf = newbuf;
        body->bufsize = new_size;
    }
    return 1;
}

ci_cached_file_t *ci_cached_file_new(int size)
{
    ci_cached_file_t *body;
    if (!(body = ci_object_pool_alloc(CACHED_FILE_POOL)))
        return NULL;

    if (size == 0)
        size = CI_BODY_MAX_MEM;

    if (size > 0 && size <= CI_BODY_MAX_MEM) {
        body->buf = ci_buffer_alloc(size * sizeof(char));
    } else
        body->buf = NULL;

    if (body->buf == NULL) {
        body->bufsize = 0;
        if ((body->fd =
                    ci_mktemp_file(CI_TMPDIR, tmp_template, body->filename)) < 0) {
            ci_debug_printf(1,
                            "Can not open temporary filename in directory:%s\n",
                            CI_TMPDIR);
            ci_object_pool_free(body);
            return NULL;
        }
    } else {
        body->bufsize = size;
        body->fd = -1;
    }
    body->endpos = 0;
    body->readpos = 0;
    body->flags = 0;
    body->unlocked = 0;
    body->attributes = NULL;
    return body;
}

void ci_cached_file_reset(ci_cached_file_t * body, int new_size)
{

    if (body->fd > 0) {
        do_close(body->fd);
        unlink(body->filename);       /*Comment out for debuging reasons */
    }

    body->endpos = 0;
    body->readpos = 0;
    body->flags = 0;
    body->unlocked = 0;
    body->fd = -1;

    if (body->attributes)
        ci_array_destroy(body->attributes);
    body->attributes = NULL;

    if (!resize_buffer(body, new_size)) {
        /*free memory and open a file. */
    }
}



void ci_cached_file_destroy(ci_cached_file_t * body)
{
    if (!body)
        return;
    if (body->buf)
        ci_buffer_free(body->buf);

    if (body->fd >= 0) {
        do_close(body->fd);
        unlink(body->filename);       /*Comment out for debuging reasons */
    }

    if (body->attributes)
        ci_array_destroy(body->attributes);

    ci_object_pool_free(body);
}


void ci_cached_file_release(ci_cached_file_t * body)
{
    if (!body)
        return;
    if (body->buf)
        ci_buffer_free(body->buf);

    if (body->fd >= 0) {
        do_close(body->fd);
    }

    if (body->attributes)
        ci_array_destroy(body->attributes);

    ci_object_pool_free(body);
}



int ci_cached_file_write(ci_cached_file_t * body, const char *buf, int len, int iseof)
{
    int remains;
    int ret;

    if (iseof) {
        body->flags |= CI_FILE_HAS_EOF;
        ci_debug_printf(10, "Buffer size=%d, Data size=%" PRINTF_OFF_T "\n ",
                        ((ci_cached_file_t *) body)->bufsize,
                        (CAST_OFF_T) ((ci_cached_file_t *) body)->endpos);
    }

    if (len == 0) /*If no data to write just return 0;*/
        return 0;

    if (body->fd > 0) {        /*A file was open so write the data at the end of file....... */
        lseek(body->fd, 0, SEEK_END);
        if ((ret = do_write_old(body->fd, buf, len)) < 0) {
            ci_debug_printf(1, "Cannot write to file!!! (errno=%d)\n",
                            errno);
        }
        body->endpos += len;
        return len;
    }

    remains = body->bufsize - body->endpos;
    assert(remains >= 0);
    if (remains < len) {

        if ((body->fd =
                    ci_mktemp_file(CI_TMPDIR, tmp_template, body->filename)) < 0) {
            ci_debug_printf(1,
                            "I cannot create the temporary file: %s!!!!!!\n",
                            body->filename);
            return -1;
        }
        ret = do_write_old(body->fd, body->buf, body->endpos);
        if (ret >= 0 && do_write_old(body->fd, buf, len) >= 0) {
            body->endpos += len;
            return len;
        } else {
            ci_debug_printf(1, "Cannot write to cachefile: %s\n", strerror(errno));
            return CI_ERROR;
        }
    }                          /*  if remains<len */

    if (len > 0) {
        memcpy(body->buf + body->endpos, buf, len);
        body->endpos += len;
    }
    return len;

}

/*
body->unlocked=?
*/

int ci_cached_file_read(ci_cached_file_t * body, char *buf, int len)
{
    int remains, bytes;

    if ((body->readpos == body->endpos) && (body->flags & CI_FILE_HAS_EOF))
        return CI_EOF;

    if (len == 0) /*If no data to read just return 0*/
        return 0;


    if (body->fd > 0) {
        if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0)
            remains = body->unlocked - body->readpos;
        else
            remains = len;

        assert(remains >= 0);

        bytes = (remains > len ? len : remains);      /*Number of bytes that we are going to read from file..... */

        if ((bytes = do_read(body->fd, buf, bytes, body->readpos)) > 0)
            body->readpos += bytes;
        return bytes;
    }

    if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0)
        remains = body->unlocked - body->readpos;
    else
        remains = body->endpos - body->readpos;

    assert(remains >= 0);

    bytes = (len <= remains ? len : remains);
    if (bytes > 0) {
        memcpy(buf, body->buf + body->readpos, bytes);
        body->readpos += bytes;
    } else {                   /*?????????????????????????????? */
        bytes = 0;
        ci_debug_printf(10, "Read 0, %" PRINTF_OFF_T " %" PRINTF_OFF_T "\n",
                        (CAST_OFF_T) body->readpos, (CAST_OFF_T) body->endpos);
    }
    return bytes;
}


/********************************************************************************/
/*ci_simple_file function implementation                                        */

ci_simple_file_t *ci_simple_file_new(ci_off_t maxsize)
{
    ci_simple_file_t *body;

    if (!(body = ci_object_pool_alloc(SIMPLE_FILE_POOL)))
        return NULL;

    if ((body->fd =
                ci_mktemp_file(CI_TMPDIR, tmp_template, body->filename)) < 0) {
        ci_debug_printf(1,
                        "ci_simple_file_new: Can not open temporary filename in directory:%s\n",
                        CI_TMPDIR);
        ci_object_pool_free(body);
        return NULL;
    }
    ci_debug_printf(5, "ci_simple_file_new: Use temporary filename: %s\n", body->filename);
    body->endpos = 0;
    body->readpos = 0;
    body->flags = 0;
    body->unlocked = 0;        /*Not use look */
    body->max_store_size = (maxsize>0?maxsize:0);
    body->bytes_in = 0;
    body->bytes_out = 0;
    body->attributes = NULL;
    body->mmap_addr = NULL;
    body->mmap_size = 0;

    return body;
}



ci_simple_file_t *ci_simple_file_named_new(char *dir, char *filename,ci_off_t maxsize)
{
    ci_simple_file_t *body;

    if (!(body = ci_object_pool_alloc(SIMPLE_FILE_POOL)))
        return NULL;

    if (filename) {
        snprintf(body->filename, CI_FILENAME_LEN, "%s/%s", dir, filename);
        if ((body->fd =
                    do_open(body->filename, O_CREAT | O_RDWR | O_EXCL)) < 0) {
            ci_debug_printf(1, "Can not open temporary filename: %s\n",
                            body->filename);
            ci_object_pool_free(body);
            return NULL;
        }
    } else if ((body->fd =
                    ci_mktemp_file(dir, tmp_template, body->filename)) < 0) {
        ci_debug_printf(1,
                        "Can not open temporary filename in directory: %s\n",
                        dir);
        ci_object_pool_free(body);
        return NULL;
    }
    body->endpos = 0;
    body->readpos = 0;
    body->flags = 0;
    body->unlocked = 0;
    body->max_store_size = (maxsize>0?maxsize:0);
    body->bytes_in = 0;
    body->bytes_out = 0;
    body->attributes = NULL;

    body->mmap_addr = NULL;
    body->mmap_size = 0;

    return body;
}


void ci_simple_file_destroy(ci_simple_file_t * body)
{
    if (!body)
        return;

    if (body->fd >= 0) {
        do_close(body->fd);
        unlink(body->filename);       /*Comment out for debuging reasons */
    }

    if (body->attributes)
        ci_array_destroy(body->attributes);

#if defined(USE_POSIX_MAPPED_FILES)
    if (body->mmap_addr)
        munmap(body->mmap_addr, body->mmap_size);
#endif

    ci_object_pool_free(body);
}


void ci_simple_file_release(ci_simple_file_t * body)
{
    if (!body)
        return;

    if (body->fd >= 0) {
        do_close(body->fd);
    }

    if (body->attributes)
        ci_array_destroy(body->attributes);

#if defined(USE_POSIX_MAPPED_FILES)
    if (body->mmap_addr)
        munmap(body->mmap_addr, body->mmap_size);
#endif

    ci_object_pool_free(body);
}


int ci_simple_file_write(ci_simple_file_t * body, const char *buf, size_t len, int iseof)
{
    int ret;
    size_t wsize = 0;

    assert(body);
    if (body->flags & CI_FILE_HAS_EOF) {
        if (len > 0) {
            ci_debug_printf(1, "Cannot write to file: '%s', the eof flag is set!\n", body->filename);
        }
        return 0;
    }

    if (len <= 0) {
        if (iseof)
            body->flags |= CI_FILE_HAS_EOF;
        return 0;
    }

    assert(!body->max_store_size || body->endpos <= body->max_store_size);
    assert(!body->max_store_size || body->readpos <= body->max_store_size);

    if (body->endpos < body->readpos) {
        assert(body->flags & CI_FILE_RING_MODE);
        wsize = (size_t)min(body->readpos - body->endpos - (ci_off_t)1, (ci_off_t)len);
    } else if (body->max_store_size && body->endpos == body->max_store_size) {
        /*If we are going to entre ring mode. If we are using locking we can not enter ring mode.*/
        if (body->readpos != 0 && (body->flags & CI_FILE_USELOCK) == 0) {
            body->endpos = 0;
            if (!(body->flags & CI_FILE_RING_MODE)) {
                body->flags |= CI_FILE_RING_MODE;
                ci_debug_printf(5, "ci_simple_file_write: Entering Ring mode!\n");
            }
            wsize = (size_t)min(body->readpos - (ci_off_t)1, (ci_off_t)len);
        } else {
            if ((body->flags & CI_FILE_USELOCK) != 0)
                ci_debug_printf(1, "File locked and no space on file for writing data, (Is this a bug?)!\n");
            return 0;
        }
    } else {
        if (body->max_store_size)
            wsize = (size_t)min(body->max_store_size - body->endpos, (ci_off_t)len);
        else
            wsize = len;
    }

    if ((ret = do_write(body->fd, buf, wsize, body->endpos)) < 0) {
        ci_debug_printf(1, "Cannot write to file: %s\n", strerror(errno));
    } else {
        body->endpos += ret;
        body->bytes_in += ret;
    }

    if (iseof && ((size_t)ret) == len) {
        body->flags |= CI_FILE_HAS_EOF;
        ci_debug_printf(9, "Body data size=%" PRINTF_OFF_T "\n ",
                        (CAST_OFF_T) body->endpos);
    }

    return ret;
}



int ci_simple_file_read(ci_simple_file_t * body, char *buf, size_t len)
{
    ci_off_t available;
    size_t needs;
    int ret;

    if (len <= 0)
        return 0;

    assert(body);
    if (body->readpos == body->endpos) {
        if ((body->flags & CI_FILE_HAS_EOF)) {
            ci_debug_printf(9, "Has EOF and no data to read, send EOF\n");
            return CI_EOF;
        } else {
            return 0;
        }
    }

    if (body->max_store_size && body->readpos == body->max_store_size) {
        assert(body->endpos < body->readpos);
        assert((body->flags & CI_FILE_RING_MODE));
        body->readpos = 0;
    }

    if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0) {
        assert(!(body->flags & CI_FILE_RING_MODE)); /*locks and ring modes can not coexist*/
        available = body->unlocked - body->readpos;
    } else if (body->endpos > body->readpos) {
        available = body->endpos - body->readpos;
    } else {
        assert((body->endpos < body->readpos) || (body->readpos == 0));
        assert((body->flags & CI_FILE_RING_MODE));
        if (body->max_store_size) {
            available = body->max_store_size - body->readpos;
        } else {
            ci_debug_printf(9, "Error? anyway send EOF\n");
            return CI_EOF;
        }
    }

    assert(available >= 0);
    needs = (size_t)min(available, (ci_off_t)len);   /*Number of bytes that we are going to read from file..... */
    if ((ret = do_read(body->fd, buf, needs, body->readpos)) > 0) {
        body->readpos += ret;
        body->bytes_out += ret;
    }
    return ret;

}

int ci_simple_file_truncate(ci_simple_file_t *body, ci_off_t new_size)
{
    assert(body);
    if (new_size > body->endpos)
        return 0;

    if (new_size == 0) {
        new_size = lseek(body->fd, 0, SEEK_END);
        if (new_size > body->endpos) /* ????? */
            return 0;
    } else {
        if (ftruncate(body->fd, new_size) != 0)
            return 0; /*failed to resize*/
    }

    body->endpos = new_size;

    if (body->readpos > new_size)
        body->readpos = new_size;

    if (body->unlocked > new_size)
        body->unlocked = new_size;

    return 1;
}

static int mmap_body_prerequisites(ci_simple_file_t *body, const char *msg)
{
#if defined(USE_POSIX_MAPPED_FILES)
     if (!(body->flags & CI_FILE_HAS_EOF)) {
         ci_debug_printf(1, "%s: '%s' failed, the eof flag is not set!\n", msg, body->filename);
        return 0;
    }

    if (body->endpos > SIZE_MAX) {
        ci_debug_printf(1, "%s: '%s' failed, huge body data!\n", msg, body->filename);
        return 0;
    }
    return 1;
#else
    ci_debug_printf( 1, "%s: Requires posix mapped files to work, which is not supported.", msg);
    return 0;
#endif
}

static void mmap_body_map(ci_simple_file_t *body, size_t map_size, const char *msg)
{
#if defined(USE_POSIX_MAPPED_FILES)
    int flags = body->flags & CI_FILE_SHARED ? MAP_SHARED : MAP_PRIVATE;
    char *addr = mmap(NULL, map_size,  PROT_READ | PROT_WRITE, flags, body->fd, 0);
    if (!addr)
        return;

    body->mmap_addr = addr;
    body->mmap_size = map_size;
#endif
}

const char * ci_simple_file_to_const_string(ci_simple_file_t *body)
{
    ci_off_t map_size;
    assert(body);
    if (!mmap_body_prerequisites(body, "ci_simple_file_to_const_string"))
        return NULL;

    if (body->mmap_addr) {
        if (body->mmap_size > body->endpos && body->mmap_addr[body->endpos == '\0'])
            return body->mmap_addr; /*It is already NULL terminated*/

        /*Else it is already mapped but it is not NULL terminated.
         It is safer to return NULL for now.*/
        ci_debug_printf(1, "ci_simple_file_to_const_string: '%s' failed, already mmaped as raw data!\n", body->filename);
        return NULL;
    }

    /* We need one more byte for string termination*/
    map_size = body->endpos + 1;
    if (ftruncate(body->fd, map_size) != 0)
        return NULL; /*failed to resize*/

    assert(body->mmap_addr == NULL);
    mmap_body_map(body, map_size, "ci_simple_file_to_const_string");
    /*Terminate buffer (already terminated by ftruncate?) */
    body->mmap_addr[map_size - 1] = '\0';

    return body->mmap_addr;
}

const char * ci_simple_file_to_const_raw_data(ci_simple_file_t *body, size_t *data_size)
{
    assert(body);
    if (!mmap_body_prerequisites(body, "ci_simple_file_to_const_raw_data"))
        return NULL;

    if (body->mmap_addr == NULL)
        mmap_body_map(body, (size_t)body->endpos, "ci_simple_file_to_const_raw_data");

    *data_size = body->endpos;
    return body->mmap_addr;
}

#define CI_MEMBUF_SF_FLAGS (CI_MEMBUF_CONST | CI_MEMBUF_RO | CI_MEMBUF_NULL_TERMINATED)
ci_membuf_t *ci_simple_file_to_membuf(ci_simple_file_t *body, unsigned int flags)
{
    assert((CI_MEMBUF_SF_FLAGS & flags) == flags);
    assert(flags & CI_MEMBUF_CONST);
    assert(body);
    void *addr;
    if (flags & CI_MEMBUF_NULL_TERMINATED)
        addr = (void *)ci_simple_file_to_const_string(body);
    else {
        size_t asize = 0;
        addr = (void *)ci_simple_file_to_const_raw_data(body, &asize);
    }

    if (!addr)
        return NULL;

    unsigned int memflags =  CI_MEMBUF_CONST | CI_MEMBUF_RO | CI_MEMBUF_HAS_EOF;
    if (flags & CI_MEMBUF_NULL_TERMINATED)
        memflags |= CI_MEMBUF_NULL_TERMINATED;
    return ci_membuf_from_content(body->mmap_addr, body->mmap_size, body->endpos, memflags);
}

/*******************************************************************/
/*ring memory buffer implementation                                */

struct ci_ring_buf *ci_ring_buf_new(size_t size)
{
    struct ci_ring_buf *buf = ci_object_pool_alloc(RING_BUF_POOL);
    if (!buf)
        return NULL;

    buf->buf = ci_buffer_alloc(size);
    if (!buf->buf) {
        ci_object_pool_free(buf);
        return NULL;
    }

    buf->end_buf = buf->buf+size-1;
    buf->read_pos = buf->buf;
    buf->write_pos = buf->buf;
    buf->full = 0;
    return buf;
}

void ci_ring_buf_destroy(struct ci_ring_buf *buf)
{
    assert(buf);
    ci_buffer_free(buf->buf);
    ci_object_pool_free(buf);
}

int ci_ring_buf_is_empty(struct ci_ring_buf *buf)
{
    assert(buf);
    return (buf->read_pos == buf->write_pos) && (buf->full == 0);
}

int ci_ring_buf_write_direct(struct ci_ring_buf *buf, char **wb, size_t *len)
{
    assert(buf);
    if (buf->read_pos == buf->write_pos && buf->full == 0) {
        *wb = buf->write_pos;
        *len = buf->end_buf - buf->write_pos + 1;
        return 0;
    } else if (buf->read_pos >= buf->write_pos) {
        *wb = buf->write_pos;
        *len = buf->read_pos - buf->write_pos;
        return 0;
    } else { /*buf->read_pos < buf->write_pos*/
        *wb = buf->write_pos;
        *len = buf->end_buf - buf->write_pos + 1;
        return 1;
    }
}

int ci_ring_buf_write_block(struct ci_ring_buf *buf, char **wb, int *len)
{
    size_t wsize;
    int ret = ci_ring_buf_write_direct(buf, wb, &wsize);
    *len = wsize;
    return ret;
}

int ci_ring_buf_read_direct(struct ci_ring_buf *buf, char **rb, size_t *len)
{
    assert(buf);
    if (buf->read_pos == buf->write_pos && buf->full == 0) {
        *rb = buf->read_pos;
        *len = 0;
        return 0;
    } else if (buf->read_pos >= buf->write_pos) {
        *rb = buf->read_pos;
        *len = buf->end_buf - buf->read_pos +1;
        return (buf->read_pos != buf->buf? 1:0);
    } else { /*buf->read_pos < buf->write_pos*/
        *rb = buf->read_pos;
        *len = buf->write_pos - buf->read_pos;
        return 0;
    }
}

int ci_ring_buf_read_block(struct ci_ring_buf *buf, char **rb, int *len)
{
    size_t rsize;
    int ret = ci_ring_buf_read_direct(buf, rb, &rsize);
    *len = rsize;
    return ret;
}

void ci_ring_buf_consume(struct ci_ring_buf *buf, size_t len)
{
    if (len <= 0)
        return;
    assert(buf);
    buf->read_pos += len;
    if (buf->read_pos > buf->end_buf)
        buf->read_pos = buf->buf;
    if (buf->full)
        buf->full = 0;
}

void ci_ring_buf_produce(struct ci_ring_buf *buf, size_t len)
{
    if (len <= 0)
        return;
    assert(buf);
    buf->write_pos += len;
    if (buf->write_pos > buf->end_buf)
        buf->write_pos = buf->buf;

    if (buf->write_pos == buf->read_pos)
        buf->full = 1;

}

int ci_ring_buf_write(struct ci_ring_buf *buf, const char *data, size_t size)
{
    char *wb;
    size_t wb_len;
    int ret, written;
    written = 0;
    do {
        ret = ci_ring_buf_write_direct(buf, &wb, &wb_len);
        if (wb_len) {
            wb_len = min(size, wb_len);
            memcpy(wb, data, wb_len);
            ci_ring_buf_produce(buf, wb_len);
            size -= wb_len;
            data += wb_len;
            written += wb_len;
        }
    } while ((ret != 0) && (size > 0));
    return written;
}


int ci_ring_buf_read(struct ci_ring_buf *buf, char *data, size_t size)
{
    char *rb;
    size_t rb_len;
    int ret, data_read;
    data_read = 0;
    do {
        ret = ci_ring_buf_read_direct(buf, &rb, &rb_len);
        if (rb_len) {
            rb_len = min(size, rb_len);
            memcpy(data, rb, rb_len);
            ci_ring_buf_consume(buf, rb_len);
            size -= rb_len;
            data += rb_len;
            data_read += rb_len;
        }
    } while ((ret != 0) && (size > 0));
    return data_read;
}
