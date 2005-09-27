/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef __BODY_H
#define __BODY_H

#include "c-icap.h"
#include <stdio.h>
#include "util.h"


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
CI_DECLARE_FUNC(int) ci_membuf_write(struct ci_membuf *body, char *buf,int len, int iseof);
CI_DECLARE_FUNC(int) ci_membuf_read(struct ci_membuf *body,char *buf,int len);


/*****************************************************************/
/* Cached file functions and structure                           */

#define CI_FILE_USELOCK  0x01
#define CI_FILE_HAS_EOF  0x02

typedef struct ci_cached_file{
     ci_off_t endpos;
     ci_off_t readpos;
     int      bufsize;
     int flags;
     ci_off_t unlocked;
     char *buf;
     int fd;
     char filename[CI_FILENAME_LEN];
} ci_cached_file_t;


CI_DECLARE_DATA extern int  CI_BODY_MAX_MEM;
CI_DECLARE_DATA extern char *CI_TMPDIR;

CI_DECLARE_FUNC(ci_cached_file_t) * ci_cached_file_new(int size);
CI_DECLARE_FUNC(void) ci_cached_file_destroy(ci_cached_file_t *);
CI_DECLARE_FUNC(int) ci_cached_file_write(ci_cached_file_t *body,
					  char *buf,int len, int iseof);
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
     int flags;
     ci_off_t unlocked;
     int fd;
     char filename[CI_FILENAME_LEN+1];
} ci_simple_file_t;


CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_new();
CI_DECLARE_FUNC(ci_simple_file_t) * ci_simple_file_named_new(char *tmp,char*filename);

CI_DECLARE_FUNC(void) ci_simple_file_release(ci_simple_file_t *);
CI_DECLARE_FUNC(void) ci_simple_file_destroy(ci_simple_file_t *body);
CI_DECLARE_FUNC(int) ci_simple_file_write(ci_simple_file_t *body,
					  char *buf,int len, int iseof);
CI_DECLARE_FUNC(int) ci_simple_file_read(ci_simple_file_t *body,char *buf,int len);


#define ci_simple_file_lock_all(body)            (body->flags|=CI_FILE_USELOCK,body->unlocked=0)
#define ci_simple_file_unlock(body, len)     (body->unlocked=len)
#define ci_simple_file_unlock_all(body)      (body->flags&=~CI_FILE_USELOCK,body->unlocked=0)
#define ci_simple_file_size(body)            (body->endpos)
#define ci_simple_file_haseof(body)        (body->flags&CI_FILE_HAS_EOF)

#endif
