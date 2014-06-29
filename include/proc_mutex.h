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
#endif
#if defined (USE_POSIX_SEMAPHORES)
#include <semaphore.h>
#endif
#if defined (USE_POSIX_FILE_LOCK)
#include <fcntl.h>
#endif
#if defined (_WIN32)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ci_proc_mutex ci_proc_mutex_t;

typedef struct ci_proc_mutex_scheme {
    int (*proc_mutex_init)(ci_proc_mutex_t *mutex, const char *name);
    int (*proc_mutex_destroy)(ci_proc_mutex_t *mutex);
    int (*proc_mutex_lock)(ci_proc_mutex_t *mutex);
    int (*proc_mutex_unlock)(ci_proc_mutex_t *mutex);
    int (*proc_mutex_print_info)(ci_proc_mutex_t *mutex, char *buf, size_t buf_size);
    const char *name;
} ci_proc_mutex_scheme_t;

#define CI_PROC_MUTEX_NAME_SIZE 64

struct ci_proc_mutex {
    char name[CI_PROC_MUTEX_NAME_SIZE];

#if defined(_WIN32)

    HANDLE id;

#else

    const ci_proc_mutex_scheme_t *scheme;
    union {
#if defined(USE_SYSV_IPC_MUTEX)
        struct {
            int id;
        } sysv;
#endif
#if defined(USE_POSIX_SEMAPHORES)
        struct {
            sem_t *sem;
        } posix;
#endif
#if defined(USE_POSIX_FILE_LOCK)
        struct {
            int fd;
        } file;
#endif
    };

#endif
};

CI_DECLARE_FUNC(int) ci_proc_mutex_init(ci_proc_mutex_t *mutex, const char *name);
CI_DECLARE_FUNC(int) ci_proc_mutex_lock(ci_proc_mutex_t *mutex);
CI_DECLARE_FUNC(int) ci_proc_mutex_unlock(ci_proc_mutex_t *mutex);
CI_DECLARE_FUNC(int) ci_proc_mutex_destroy(ci_proc_mutex_t *mutex);

CI_DECLARE_FUNC(int) ci_proc_mutex_set_scheme(const char *scheme);
CI_DECLARE_FUNC(const ci_proc_mutex_scheme_t *) ci_proc_mutex_default_scheme();


#ifdef __cplusplus
}
#endif

#endif
