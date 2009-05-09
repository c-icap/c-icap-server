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
#include "simple_api.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define STARTLEN 8192           /*8*1024*1024 */
#define INCSTEP  4096

#define min(x,y) ((x)>(y)?(y):(x))
#define max(x,y) ((x)>(y)?(x):(y))

struct ci_membuf *ci_membuf_new()
{
    return ci_membuf_new_sized(STARTLEN);
}

struct ci_membuf *ci_membuf_new_sized(int size)
{
     struct ci_membuf *b;
     b = malloc(sizeof(struct ci_membuf));
     if (!b)
          return NULL;

     b->len = 0;
     b->endpos = 0;
     b->readpos = 0;
     b->hasalldata = 0;
     b->buf = malloc(size * sizeof(char));
     if (b->buf == NULL) {
          free(b);
          return NULL;
     }
     b->bufsize = size;
     return b;
}


void ci_membuf_free(struct ci_membuf *b)
{
     if (!b)
          return;
     if (b->buf)
          free(b->buf);
     free(b);
}


int ci_membuf_write(struct ci_membuf *b, char *data, int len, int iseof)
{
     int remains, newsize;
     char *newbuf;
     if (iseof) {
          b->hasalldata = 1;
/*	  ci_debug_printf(10,"Buffer size=%d, Data size=%d\n ",
		       ((struct membuf *)b)->bufsize,((struct membuf *)b)->endpos);
*/
     }

     remains = b->bufsize - b->endpos;
     while (remains < len) {
          newsize = b->bufsize + INCSTEP;
          newbuf = realloc(b->buf, newsize);
          if (newbuf == NULL) {
               if (remains)
                    memcpy(b->buf + b->endpos, data, remains);
               b->endpos = b->bufsize;
               return remains;
          }
          b->buf = newbuf;
          b->bufsize = newsize;
          remains = b->bufsize - b->endpos;
     }                          /*while remains<len */
     if (len) {
          memcpy(b->buf + b->endpos, data, len);
          b->endpos += len;
     }
     return len;
}

int ci_membuf_read(struct ci_membuf *b, char *data, int len)
{
     int remains, copybytes;
     remains = b->endpos - b->readpos;
     if (remains == 0 && b->hasalldata)
          return CI_EOF;
     copybytes = (len <= remains ? len : remains);
     if (copybytes) {
          memcpy(data, b->buf + b->readpos, copybytes);
          b->readpos += copybytes;
     }

     return copybytes;
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

     newbuf = realloc(body->buf, new_size);
     if (newbuf) {
          body->buf = newbuf;
          body->bufsize = new_size;
     }
     return 1;
}

ci_cached_file_t *ci_cached_file_new(int size)
{
     ci_cached_file_t *body;
     if (!(body = malloc(sizeof(ci_cached_file_t))))
          return NULL;

     if (size == 0)
          size = CI_BODY_MAX_MEM;

     if (size > 0 && size <= CI_BODY_MAX_MEM) {
          body->buf = malloc(size * sizeof(char));
     }
     else
          body->buf = NULL;

     if (body->buf == NULL) {
          body->bufsize = 0;
          if ((body->fd =
               ci_mktemp_file(CI_TMPDIR, tmp_template, body->filename)) < 0) {
               ci_debug_printf(1,
                               "Can not open temporary filename in directory:%s\n",
                               CI_TMPDIR);
               free(body);
               return NULL;
          }
     }
     else {
          body->bufsize = size;
          body->fd = -1;
     }
     body->endpos = 0;
     body->readpos = 0;
     body->flags = 0;
     body->unlocked = 0;
     return body;
}


#ifdef _WIN32
#define F_PERM S_IREAD|S_IWRITE
#else
#define F_PERM S_IREAD|S_IWRITE|S_IRGRP|S_IROTH
#endif

void ci_cached_file_reset(ci_cached_file_t * body, int new_size)
{

     if (body->fd > 0) {
          close(body->fd);
          unlink(body->filename);       /*Comment out for debuging reasons */
     }

     body->endpos = 0;
     body->readpos = 0;
     body->flags = 0;
     body->unlocked = 0;
     body->fd = -1;

     if (!resize_buffer(body, new_size)) {
          /*free memory and open a file. */
     }
}



void ci_cached_file_destroy(ci_cached_file_t * body)
{
     if (!body)
          return;
     if (body->buf)
          free(body->buf);

     if (body->fd >= 0) {
          close(body->fd);
          unlink(body->filename);       /*Comment out for debuging reasons */
     }

     free(body);
}


void ci_cached_file_release(ci_cached_file_t * body)
{
     if (!body)
          return;
     if (body->buf)
          free(body->buf);

     if (body->fd >= 0) {
          close(body->fd);
     }
     free(body);
}



int ci_cached_file_write(ci_cached_file_t * body, char *buf, int len, int iseof)
{
     int remains;

     if (iseof) {
          body->flags |= CI_FILE_HAS_EOF;
          ci_debug_printf(10, "Buffer size=%d, Data size=%" PRINTF_OFF_T "\n ",
                          ((ci_cached_file_t *) body)->bufsize,
                          ((ci_cached_file_t *) body)->endpos);
     }

     if(len == 0)  /*If no data to write just return 0;*/
	 return 0;

     if (body->fd > 0) {        /*A file was open so write the data at the end of file....... */
          lseek(body->fd, 0, SEEK_END);
          if (write(body->fd, buf, len) < 0) {
               ci_debug_printf(1, "Can not write to file!!! (errno=%d)\n",
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
                               "I can not create the temporary file name:%s!!!!!!\n",
                               body->filename);
               return -1;
          }
          write(body->fd, body->buf, body->endpos);
          write(body->fd, buf, len);
          body->endpos += len;
          return len;
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
     
     if(len == 0)  /*If no data to read just return 0*/
	 return 0;


     if (body->fd > 0) {
          if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0)
               remains = body->unlocked - body->readpos;
          else
               remains = len;

/*	  assert(remains>=0);*/

          bytes = (remains > len ? len : remains);      /*Number of bytes that we are going to read from file..... */

          lseek(body->fd, body->readpos, SEEK_SET);
          if ((bytes = read(body->fd, buf, bytes)) > 0)
               body->readpos += bytes;
          return bytes;
     }

     if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0)
          remains = body->unlocked - body->readpos;
     else
          remains = body->endpos - body->readpos;

/*     assert(remains>=0);     */

     bytes = (len <= remains ? len : remains);
     if (bytes > 0) {
          memcpy(buf, body->buf + body->readpos, bytes);
          body->readpos += bytes;
     }
     else {                     /*?????????????????????????????? */
          bytes = 0;
          ci_debug_printf(10, "Readed 0, %" PRINTF_OFF_T " %" PRINTF_OFF_T "\n",
                          body->readpos, body->endpos);
     }
     return bytes;
}


/********************************************************************************/
/*ci_simple_file function implementation                                        */

ci_simple_file_t *ci_simple_file_new(ci_off_t maxsize)
{
     ci_simple_file_t *body;

     if (!(body = malloc(sizeof(ci_simple_file_t))))
          return NULL;

     if ((body->fd =
          ci_mktemp_file(CI_TMPDIR, tmp_template, body->filename)) < 0) {
          ci_debug_printf(1,
                          "ci_simple_file_new: Can not open temporary filename in directory:%s\n",
                          CI_TMPDIR);
          free(body);
          return NULL;
     }
     body->endpos = 0;
     body->readpos = 0;
     body->flags = 0;
     body->unlocked = 0;        /*Not use look */
     body->max_store_size = (maxsize>0?maxsize:0);
     body->bytes_in = 0;
     body->bytes_out = 0;

     return body;
}



ci_simple_file_t *ci_simple_file_named_new(char *dir, char *filename,ci_off_t maxsize)
{
     ci_simple_file_t *body;

     if (!(body = malloc(sizeof(ci_cached_file_t))))
          return NULL;

     if (filename) {
          snprintf(body->filename, CI_FILENAME_LEN, "%s/%s", dir, filename);
          if ((body->fd =
               open(body->filename, O_CREAT | O_RDWR | O_EXCL, F_PERM)) < 0) {
               ci_debug_printf(1, "Can not open temporary filename: %s\n",
                               body->filename);
               free(body);
               return NULL;
          }
     }
     else if ((body->fd =
               ci_mktemp_file(dir, tmp_template, body->filename)) < 0) {
          ci_debug_printf(1,
                          "Can not open temporary filename in directory: %s\n",
                          dir);
          free(body);
          return NULL;
     }
     body->endpos = 0;
     body->readpos = 0;
     body->flags = 0;
     body->unlocked = 0;
     body->max_store_size = (maxsize>0?maxsize:0);
     body->bytes_in = 0;
     body->bytes_out = 0;

     return body;
}


void ci_simple_file_destroy(ci_simple_file_t * body)
{
     if (!body)
          return;

     if (body->fd >= 0) {
          close(body->fd);
          unlink(body->filename);       /*Comment out for debuging reasons */
     }

     free(body);
}


void ci_simple_file_release(ci_simple_file_t * body)
{
     if (!body)
          return;

     if (body->fd >= 0) {
          close(body->fd);
     }

     free(body);
}


int ci_simple_file_write(ci_simple_file_t * body, char *buf, int len, int iseof)
{
     int ret;
     int wsize = 0;

     if(len <= 0) {
	 if (iseof)
	     body->flags |= CI_FILE_HAS_EOF;
	 return 0;
     }

     if (body->endpos < body->readpos) {
         wsize = min(body->readpos-body->endpos-1, len);
     }
     else if(body->max_store_size && body->endpos >= body->max_store_size) {
       /*If we are going to entre ring mode. If we are using locking we can not enter ring mode.*/
         if (body->readpos!=0 && (body->flags & CI_FILE_USELOCK)==0) {
             body->endpos = 0;
	     if (!(body->flags & CI_FILE_RING_MODE)) {
		 body->flags |= CI_FILE_RING_MODE;
		 ci_debug_printf(9, "Entering Ring mode!\n");
	     }	     
             wsize = min(body->readpos-body->endpos-1, len);
         } 
         else {
	     if ((body->flags & CI_FILE_USELOCK) != 0)
		 ci_debug_printf(1, "File locked and no space on file for writing data, (Is this a bug?)!\n");
             return 0;
	 }
     }
     else {
	if (body->max_store_size)
	    wsize = min(body->max_store_size - body->endpos, len);
	else
	    wsize = len;
     }

     lseek(body->fd, body->endpos, SEEK_SET);
     if ((ret = write(body->fd, buf, wsize)) < 0) {
     }
     body->endpos += ret;
     body->bytes_in += ret;

     if (iseof && ret == len) {
          body->flags |= CI_FILE_HAS_EOF;
          ci_debug_printf(9, "Body data size=%" PRINTF_OFF_T "\n ",
                          body->bytes_in);
     }

     return ret;
}



int ci_simple_file_read(ci_simple_file_t * body, char *buf, int len)
{
     int remains, bytes;
 
     if (len <= 0)
         return 0;

     if ((body->readpos == body->endpos)) {
	 if((body->flags & CI_FILE_HAS_EOF)) {
	     ci_debug_printf(9, "Has EOF and no data to read, send EOF\n");
	     return CI_EOF;
	 }
	 else {
	     return 0;
	 }
     }

     if(body->max_store_size && body->readpos == body->max_store_size) {
       body->readpos = 0;
     }

     if ((body->flags & CI_FILE_USELOCK) && body->unlocked >= 0) {
          remains = body->unlocked - body->readpos;
     }
     else if(body->endpos > body->readpos) {
          remains = body->endpos - body->readpos;
     }
     else {
	 if(body->max_store_size) {
	     remains = body->max_store_size - body->readpos;
	 }
	 else {
	     ci_debug_printf(9, "Error? anyway send EOF\n");
	     return CI_EOF;
	 }
     }

     bytes = (remains > len ? len : remains);   /*Number of bytes that we are going to read from file..... */
     lseek(body->fd, body->readpos, SEEK_SET);
     if ((bytes = read(body->fd, buf, bytes)) > 0) {
          body->readpos += bytes;
	  body->bytes_out += bytes;
     }
     return bytes;

}



/*******************************************************************/
/*ring memory buffer implementation                                */

struct ci_ring_buf *ci_ring_buf_new(int size)
{
  struct ci_ring_buf *buf = malloc(sizeof(struct ci_ring_buf));
  buf->buf = malloc(size);
  buf->end_buf=buf->buf+size-1; 
  buf->read_pos = buf->buf;
  buf->write_pos = buf->buf;
  buf->full = 0;
  return buf;
}

void ci_ring_buf_destroy(struct ci_ring_buf *buf)
{
    free(buf->buf);
    free(buf);
}

int ci_ring_buf_is_empty(struct ci_ring_buf *buf)
{
  return (buf->read_pos==buf->write_pos) && (buf->full==0);
}

int ci_ring_buf_write_block(struct ci_ring_buf *buf, char **wb, int *len)
{
   if(buf->read_pos == buf->write_pos && buf->full == 0) {
       *wb = buf->write_pos;
       *len = buf->end_buf - buf->write_pos + 1;
       return 0;
   }
   else if(buf->read_pos >= buf->write_pos) {
       *wb = buf->write_pos; 
       *len = buf->read_pos - buf->write_pos;
       return 0;
   }
   else { /*buf->read_pos < buf->write_pos*/
       *wb = buf->write_pos;
       *len = buf->end_buf - buf->write_pos + 1;
       return 1;
   }
}

int ci_ring_buf_read_block(struct ci_ring_buf *buf, char **rb, int *len)
{
   if (buf->read_pos == buf->write_pos && buf->full == 0) {
       *rb = buf->read_pos;
       *len = 0;
       return 0;
   }
   else if(buf->read_pos >= buf->write_pos) {
       *rb = buf->read_pos;
       *len = buf->end_buf - buf->read_pos +1;
       return (buf->read_pos!=buf->buf? 1:0);
   }
   else { /*buf->read_pos < buf->write_pos*/
       *rb = buf->read_pos;
       *len = buf->write_pos - buf->read_pos;
       return 0;
   }
}

void ci_ring_buf_consume(struct ci_ring_buf *buf, int len)
{
  if(len <= 0)
    return;
  buf->read_pos += len;
  if(buf->read_pos > buf->end_buf)
    buf->read_pos = buf->buf;
  if(buf->full)
    buf->full = 0;
}

void ci_ring_buf_produce(struct ci_ring_buf *buf, int len)
{
  if(len <= 0)
    return;
  buf->write_pos += len;
  if (buf->write_pos > buf->end_buf)
    buf->write_pos = buf->buf;
  
  if(buf->write_pos == buf->read_pos)
    buf->full = 1;

}

int ci_ring_buf_write(struct ci_ring_buf *buf, char *data,int size)
{
  char *wb;
  int wb_len, ret, written;
  written = 0;
  do {
    ret = ci_ring_buf_write_block(buf, &wb, &wb_len);
    if (wb_len) {
	wb_len = min(size, wb_len);
	memcpy(wb, data, wb_len);
	ci_ring_buf_produce(buf, wb_len);
	size -= wb_len;
	data += wb_len;
	written += wb_len;
    }
  } while ((ret!=0) && (size>0));
  return written;
}


int ci_ring_buf_read(struct ci_ring_buf *buf, char *data,int size)
{
  char *rb;
  int rb_len, ret, data_read;
  data_read = 0;
  do {
    ret = ci_ring_buf_read_block(buf, &rb, &rb_len);
    if (rb_len) {
	rb_len = min(size, rb_len);
	memcpy(data, rb, rb_len);
	ci_ring_buf_consume(buf, rb_len);
	size -= rb_len;
	data += rb_len;
	data_read += rb_len;
    }
  } while ((ret!=0) && (size>0 ));
  return data_read;
}
