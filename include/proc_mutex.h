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


#ifndef __PROC_MUTEX_H
#define __PROC_MUTEX_H

#include "c-icap.h"
#if defined (USE_SYSV_IPC_MUTEX)
#include <sys/ipc.h>
#include <sys/sem.h>
#elif defined (USE_POSIX_SEMAPHORES)
#include <semaphore.h>
#elif defined (USE_POSIX_FILE_LOCK)
#include <fcntl.h>
#elif defined (_WIN32)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if defined (USE_SYSV_IPC_MUTEX)

#define ci_proc_mutex_t int

#elif defined (USE_POSIX_SEMAPHORES)

#define CI_PROC_MUTEX_NAME_SIZE 25
#define CI_PROC_MUTEX_NAME_TMPL "c-icap.mutex."
typedef struct ci_proc_mutex {
    char name[CI_PROC_MUTEX_NAME_SIZE];
    sem_t *sem;
} ci_proc_mutex_t;

#elif defined (USE_POSIX_FILE_LOCK)

#define FILE_LOCK_SIZE 25
#define FILE_LOCK_TEMPLATE "/tmp/icap_lock.XXXXXX"

typedef struct ci_proc_mutex{
     char filename[FILE_LOCK_SIZE];
     int fd;
} ci_proc_mutex_t;

#elif defined (_WIN32)

#define ci_proc_mutex_t HANDLE

#endif

CI_DECLARE_FUNC(int) ci_proc_mutex_init(ci_proc_mutex_t *mutex);
CI_DECLARE_FUNC(int) ci_proc_mutex_lock(ci_proc_mutex_t *mutex);
CI_DECLARE_FUNC(int) ci_proc_mutex_unlock(ci_proc_mutex_t *mutex);
CI_DECLARE_FUNC(int) ci_proc_mutex_destroy(ci_proc_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif
