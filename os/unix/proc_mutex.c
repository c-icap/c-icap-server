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
#include "proc_mutex.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h> 




#if defined(USE_SYSV_IPC_MUTEX)

#define  SEMKEY 888888L         /*A key but what key;The IPC_PRIVATE must used instead ..... */
#define  PERMS 0600
/*static int current_semkey=SEMKEY; */

static struct sembuf op_lock[2] = {
     {0, 0, 0},                 /*wait for sem to become 0 */
     {0, 1, SEM_UNDO}           /*then increment sem by 1  */
};

static struct sembuf op_unlock[1] = {
     {0, -1, (IPC_NOWAIT | SEM_UNDO)}   /*decrement sem by 1   */
};

#ifndef HAVE_UNION_SEMUN
union semun {
     int val;                   /* Value for SETVAL */
     struct semid_ds *buf;      /* Buffer for IPC_STAT, IPC_SET */
     unsigned short *array;     /* Array for GETALL, SETALL */
     struct seminfo *__buf;     /* Buffer for IPC_INFO
                                   (Linux specific) */
};
#endif

static int sysv_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
     union semun arg;
     if ((mutex->sysv.id = semget(IPC_PRIVATE, 1, IPC_CREAT | PERMS)) < 0) {
          ci_debug_printf(1, "Error creating mutex\n");
          return 0;
     }
     arg.val = 0;
     if ((semctl(mutex->sysv.id, 0, SETVAL, arg)) < 0) {
          ci_debug_printf(1, "Error setting default value for mutex, errno:%d\n",
                          errno);
          return 0;
     }
     strncpy(mutex->name, name, CI_PROC_MUTEX_NAME_SIZE);
     mutex->name[CI_PROC_MUTEX_NAME_SIZE - 1] = '\0';
     return 1;
}

static int sysv_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
     if (semctl(mutex->sysv.id, 0, IPC_RMID, 0) < 0) {
          ci_debug_printf(1, "Error removing mutex\n");
          return 0;
     }
     return 1;
}

static int sysv_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
     if (semop(mutex->sysv.id, (struct sembuf *) &op_lock, 2) < 0) {
          return 0;
     }
     return 1;
}

static int sysv_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
     if (semop(mutex->sysv.id, (struct sembuf *) &op_unlock, 1) < 0) {
          return 0;
     }
     return 1;
}

static int sysv_proc_mutex_print_info(ci_proc_mutex_t * mutex, char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size, "sysv:%s/%d", mutex->name, mutex->sysv.id);
}

static ci_proc_mutex_scheme_t sysv_mutex_scheme = {
    sysv_proc_mutex_init,
    sysv_proc_mutex_destroy,
    sysv_proc_mutex_lock,
    sysv_proc_mutex_unlock,
    sysv_proc_mutex_print_info,
    "sysv"
};

#endif

#if defined(USE_POSIX_SEMAPHORES)

#define CI_PROC_MUTEX_NAME_TMPL "/c-icap-sem."
static int posix_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
    int i = 0;
    mutex->posix.sem = SEM_FAILED;
    for(i = 0; i < 1024; ++i) {
        errno = 0;
        snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "%s%s.%d", CI_PROC_MUTEX_NAME_TMPL, name, i);
        if ((mutex->posix.sem = sem_open(mutex->name, O_CREAT|O_EXCL, S_IREAD|S_IWRITE|S_IRGRP, 1)) != SEM_FAILED) {
            return 1;
        }
        if (errno != EEXIST)
            break;
    }
    if (errno == EEXIST) {
        ci_debug_printf(1, "Error allocation posix proc mutex, to many c-icap mutexes are open!\n");
    } else {
        ci_debug_printf(1, "Error allocation posix proc mutex, errno: %d\n", errno);
    }
    return 0;
}

static int posix_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
     if (sem_unlink(mutex->name) < 0) {
          return 0;
     }
     return 1;
}

static int posix_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
     if (sem_wait(mutex->posix.sem)) {
         ci_debug_printf(1, "Failed to get lock of posix mutex\n");
         return 0;
     }
     return 1;
}

static int posix_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
     if (sem_post(mutex->posix.sem)) {
         ci_debug_printf(1, "Failed to unlock of posix mutex\n");
         return 0;
     }
     return 1;
}

static int posix_proc_mutex_print_info(ci_proc_mutex_t * mutex, char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size, "posix:%s", mutex->name);
}

static ci_proc_mutex_scheme_t posix_mutex_scheme = {
    posix_proc_mutex_init,
    posix_proc_mutex_destroy,
    posix_proc_mutex_lock,
    posix_proc_mutex_unlock,
    posix_proc_mutex_print_info,
    "posix"
};
#endif

#if defined (USE_POSIX_FILE_LOCK)

#define CI_MUTEX_FILE_TEMPLATE "/tmp/icap_lock"

/*NOTE: mkstemp does not exists for some platforms */
static int file_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
     strcpy(mutex->name, CI_MUTEX_FILE_TEMPLATE);
     snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "%s_%s.XXXXXX", CI_MUTEX_FILE_TEMPLATE, name);
     if ((mutex->file.fd = mkstemp(mutex->name)) < 0)
          return 0;

     return 1;
}

static int file_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
     close(mutex->file.fd);
     if (unlink(mutex->name) != 0)
          return 0;
     return 1;
}

static int file_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
     struct flock fl;
     fl.l_type = F_WRLCK;
     fl.l_whence = SEEK_SET;
     fl.l_start = 0;
     fl.l_len = 0;

     if (fcntl(mutex->file.fd, F_SETLKW, &fl) < 0)
          return 0;
     return 1;
}

static int file_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
     struct flock fl;
     fl.l_type = F_UNLCK;
     fl.l_whence = SEEK_SET;
     fl.l_start = 0;
     fl.l_len = 0;
     if (fcntl(mutex->file.fd, F_SETLK, &fl) < 0)
          return 0;
     return 1;
}

static int file_proc_mutex_print_info(ci_proc_mutex_t * mutex, char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size, "file:%s", mutex->name);
}

static ci_proc_mutex_scheme_t file_mutex_scheme = {
    file_proc_mutex_init,
    file_proc_mutex_destroy,
    file_proc_mutex_lock,
    file_proc_mutex_unlock,
    file_proc_mutex_print_info,
    "file"
};

#endif

#if defined(USE_POSIX_SEMAPHORES)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &posix_mutex_scheme;
#elif defined(USE_SYSV_IPC_MUTEX)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &sysv_mutex_scheme;
#elif defined(USE_POSIX_FILE_LOCK)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &file_mutex_scheme;
#else
const ci_proc_mutex_scheme_t *default_mutex_scheme = NULL;
#endif

const ci_proc_mutex_scheme_t *ci_proc_mutex_default_scheme()
{
    return default_mutex_scheme;
}

int ci_proc_mutex_set_scheme(const char *scheme)
{
#if defined(USE_SYSV_IPC_MUTEX)
    if (strcasecmp(scheme, "sysv") == 0)
        default_mutex_scheme = &sysv_mutex_scheme;
    else
#endif
#if defined(USE_POSIX_SEMAPHORES)
    if (strcasecmp(scheme, "posix") == 0)
        default_mutex_scheme = &posix_mutex_scheme;
    else
#endif
#if  defined(USE_POSIX_FILE_LOCK)
    if (strcasecmp(scheme, "file") == 0)
    default_mutex_scheme = &file_mutex_scheme;
    else
#endif
    {
        ci_debug_printf(1, "Unknown interprocess locking scheme: '%s'", scheme);
        return 0;
    }
    return 1;
}

int ci_proc_mutex_init(ci_proc_mutex_t *mutex, const char *name)
{
    if (default_mutex_scheme) {
        mutex->scheme = default_mutex_scheme;
        return default_mutex_scheme->proc_mutex_init(mutex, name);
    }
    return 0;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex)
{
    if (mutex->scheme)
        return mutex->scheme->proc_mutex_destroy(mutex);
    return 0;
}

int ci_proc_mutex_lock(ci_proc_mutex_t *mutex)
{
    if (mutex->scheme)
        return mutex->scheme->proc_mutex_lock(mutex);
    return 0;
}

int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex)
{
    if (mutex->scheme)
        return mutex->scheme->proc_mutex_unlock(mutex);
    return 0;
}
