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


#ifndef __PROC_MUTEX_H
#define __PROC_MUTEX_H

#if defined (HAVE_SYSV_IPC)

#include <sys/ipc.h>
#include <sys/sem.h>

#define ci_proc_mutex_t int

#elif defined (HAVE_POSIX_SEMAPHORES)

#include <semaphore.h>

#define ci_proc_mutex_t sem_t

#elif defined (HAVE_POSIX_FILE_LOCK)

#include <fcntl.h>

#define FILE_LOCK_SIZE 25
#define FILE_LOCK_TEMPLATE "/tmp/icap_lock.XXXXXX"

typedef struct ci_proc_mutex{
     char filename[FILE_LOCK_SIZE];
     int fd;
} ci_proc_mutex_t;


#elif defined (_WIN32)
#include <windows.h>
#define ci_proc_mutex_t HANDLE

#endif

int ci_proc_mutex_init(ci_proc_mutex_t *mutex);
int ci_proc_mutex_lock(ci_proc_mutex_t *mutex);
int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex);
int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex);


#endif
