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
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

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

#if defined(USE_SYSV_IPC_MUTEX)

struct sysv_data{
    int id;
} sysv_data;

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
    struct sysv_data *sysv = (struct sysv_data *)calloc(1, sizeof(sysv_data));
    assert(sysv);
    mutex->data = sysv;
    if ((sysv->id = semget(IPC_PRIVATE, 1, IPC_CREAT | PERMS)) < 0) {
        ci_debug_printf(1, "Error creating mutex\n");
        return 0;
    }
    arg.val = 0;
    if ((semctl(sysv->id, 0, SETVAL, arg)) < 0) {
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
    struct sysv_data *sysv = (struct sysv_data *)mutex->data;
    assert(sysv);
    if (semctl(sysv->id, 0, IPC_RMID, 0) < 0) {
        ci_debug_printf(1, "Error removing mutex\n");
        return 0;
    }
    free(mutex->data);
    mutex->data = NULL;
    return 1;
}

static int sysv_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
    struct sysv_data *sysv = (struct sysv_data *)mutex->data;
    assert(sysv);
    if (semop(sysv->id, (struct sembuf *) &op_lock, 2) < 0) {
        return 0;
    }
    return 1;
}

static int sysv_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
    struct sysv_data *sysv = (struct sysv_data *)mutex->data;
    assert(sysv);
    if (semop(sysv->id, (struct sembuf *) &op_unlock, 1) < 0) {
        return 0;
    }
    return 1;
}

static int sysv_proc_mutex_print_info(ci_proc_mutex_t * mutex, char *buf, size_t buf_size)
{
    struct sysv_data *sysv = (struct sysv_data *)mutex->data;
    assert(sysv);
    return snprintf(buf, buf_size, "sysv:%s/%d", mutex->name, sysv->id);
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

struct posix_data{
    sem_t *sem;
} posix_data;

#define CI_PROC_MUTEX_NAME_TMPL "/c-icap-sem."
static int posix_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
    int i = 0;
    struct posix_data *posix = (struct posix_data *)calloc(1, sizeof(struct posix_data));
    assert(posix);
    mutex->data = posix;
    posix->sem = SEM_FAILED;
    for (i = 0; i < 1024; ++i) {
        errno = 0;
        snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "%s%s.%d", CI_PROC_MUTEX_NAME_TMPL, name, i);
        if ((posix->sem = sem_open(mutex->name, O_CREAT|O_EXCL, S_IREAD|S_IWRITE|S_IRGRP, 1)) != SEM_FAILED) {
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
    free(mutex->data);
    mutex->data = NULL;
    return 1;
}

static int posix_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
    struct posix_data *posix = (struct posix_data *)mutex->data;
    assert(posix);
    if (sem_wait(posix->sem)) {
        ci_debug_printf(1, "Failed to get lock of posix mutex\n");
        return 0;
    }
    return 1;
}

static int posix_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
    struct posix_data *posix = (struct posix_data *)mutex->data;
    assert(posix);
    if (sem_post(posix->sem)) {
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

struct file_data {
    int fd; /*To lock processes*/
    pthread_mutex_t mtx; /*To lock process threads*/
} file_data;

#define CI_MUTEX_FILE_TEMPLATE "/tmp/icap_lock"
#if defined(__CYGWIN__)
#define CI_MUTEX_FILE_TEMPLATE_2 "\\tmp\\icap_lock"
#endif

/*NOTE: mkstemp does not exists for some platforms */
static int file_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
    int fd;
    snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "%s_%s.XXXXXX", CI_MUTEX_FILE_TEMPLATE, name);
    if ((fd = mkstemp(mutex->name)) < 0) {
#if defined(__CYGWIN__)
        /*Maybe runs under cmd*/
        snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "%s_%s.XXXXXX", CI_MUTEX_FILE_TEMPLATE_2, name);
        if ((fd = mkstemp(mutex->name)) < 0)
#endif
        return 0;
    }
    struct file_data *file = (struct file_data *)calloc(1, sizeof(struct file_data));
    assert(file);
    file->fd = fd;
    pthread_mutex_init(&file->mtx, NULL);
    mutex->data = file;
    return 1;
}

static int file_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
    struct file_data *file = (struct file_data *)mutex->data;
    assert(file);
    close(file->fd);
    if (unlink(mutex->name) != 0)
        return 0;
    pthread_mutex_destroy(&file->mtx);
    free(mutex->data);
    mutex->data = NULL;
    return 1;
}

static int file_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    struct file_data *file = (struct file_data *)mutex->data;
    assert(file);
    pthread_mutex_lock(&file->mtx);
    if (fcntl(file->fd, F_SETLKW, &fl) < 0) {
        pthread_mutex_unlock(&file->mtx);
        return 0;
    }
    return 1;
}

static int file_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    struct file_data *file = (struct file_data *)mutex->data;
    assert(file);
    if (fcntl(file->fd, F_SETLK, &fl) < 0)
        return 0;
    pthread_mutex_unlock(&file->mtx);
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

#if defined(HAVE_PTHREADS_PROCESS_SHARED)
#include "shared_mem.h"

enum mutex_state {MTX_STATE_UNUSED = 0, MTX_STATE_OK, MTX_STATE_UNRECOVERABLE};
struct mutex_item {
    pthread_mutex_t mtx;
    enum mutex_state state;
};
static struct mutex_item *PThreadSharedMemPtr = NULL;
static ci_shared_mem_id_t PThreadSharedMemId;
static int PThreadSharedMemRegistered = 0;
static const int PThreadMaxMutexes = 1024;

struct pthread_data {
    int mtx_id;
} pthread_data;

static void pthread_proc_mutexes_init_child()
{
    PThreadSharedMemPtr = ci_shared_mem_attach(&PThreadSharedMemId);
}

static void pmutex_init(pthread_mutex_t *mtx)
{
    pthread_mutexattr_t mtx_attr;
    pthread_mutexattr_init(&mtx_attr);
    pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mtx_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(mtx, &mtx_attr);
    pthread_mutexattr_destroy(&mtx_attr);
}

static int pthread_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
    if (PThreadSharedMemPtr == NULL) {
        PThreadSharedMemPtr = (struct mutex_item *)ci_shared_mem_create(&PThreadSharedMemId, "cicap_pthread_proc_mutexes", PThreadMaxMutexes * sizeof(struct mutex_item));
        memset(PThreadSharedMemPtr, 0, PThreadMaxMutexes * sizeof(struct mutex_item));
        pthread_atfork(NULL, NULL, pthread_proc_mutexes_init_child);
    }
    int mtx_id;
    for (mtx_id = 0; mtx_id < PThreadMaxMutexes && PThreadSharedMemPtr[mtx_id].state !=  MTX_STATE_UNUSED; mtx_id++);
    if (mtx_id >= PThreadMaxMutexes)
        return 0;

    struct pthread_data *pthread = (struct pthread_data *)malloc(sizeof(struct pthread_data));
    assert(pthread);
    pthread->mtx_id = mtx_id;
    PThreadSharedMemRegistered++;
    pmutex_init(&PThreadSharedMemPtr[pthread->mtx_id].mtx);
    mutex->data = pthread;
    return 1;
}

static int pthread_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
    struct pthread_data *pthread = (struct pthread_data *)mutex->data;
    assert(pthread);
    assert(pthread->mtx_id >= 0);
    assert(PThreadSharedMemPtr);
    pthread_mutex_destroy(&(PThreadSharedMemPtr[pthread->mtx_id].mtx));
    PThreadSharedMemPtr[pthread->mtx_id].state = MTX_STATE_UNUSED;
    PThreadSharedMemRegistered--;
    free(mutex->data);
    mutex->data = NULL;
    if (PThreadSharedMemRegistered <= 0) {
        ci_shared_mem_destroy(&PThreadSharedMemId);
        PThreadSharedMemPtr = NULL;
    }
    return 1;
}

static int pthread_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
    struct pthread_data *pthread = (struct pthread_data *)mutex->data;
    assert(pthread);
    assert(pthread->mtx_id >= 0);
    assert(PThreadSharedMemPtr);
    do {
        int ret = pthread_mutex_lock(&(PThreadSharedMemPtr[pthread->mtx_id].mtx));
        if (ret != 0) {
            if (ret == EOWNERDEAD) {
                ci_debug_printf(1, "Shared pthread owner crashed, trying to recover\n");
                pthread_mutex_consistent(&(PThreadSharedMemPtr[pthread->mtx_id].mtx));
            } else if (ret == ENOTRECOVERABLE) {
                PThreadSharedMemPtr[pthread->mtx_id].state = MTX_STATE_UNRECOVERABLE;
                ci_debug_printf(1, "Shared pthread can not be recovered, wait for monitor to rebuild it\n");
                ci_usleep(10000); /*wait for 10ms*/
                continue;
            } else {
                ci_debug_printf(1, "Shared pthread locking error: %d\n", ret);
                return 0;
            }
        }
    } while(0);
    return 1;
}

static int pthread_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
    struct pthread_data *pthread = (struct pthread_data *)mutex->data;
    assert(pthread);
    assert(pthread->mtx_id >= 0);
    assert(PThreadSharedMemPtr);
    pthread_mutex_unlock(&(PThreadSharedMemPtr[pthread->mtx_id].mtx));
    return 1;
}

static int pthread_proc_mutex_print_info(ci_proc_mutex_t * mutex, char *buf, size_t buf_size)
{
    char shmname[128];
    ci_shared_mem_print_info(&PThreadSharedMemId, shmname, sizeof(shmname));
    return snprintf(buf, buf_size, "pthread_mutex:%s@[%s]", mutex->name, shmname);
}

static void pthread_mutex_recover_after_crash()
{
    if (!PThreadSharedMemPtr)
        return;
    int i;
    for (i = 0; i < PThreadMaxMutexes; i++) {
        int reinit = 0;
        if (PThreadSharedMemPtr[i].state ==  MTX_STATE_UNUSED) {
            continue;
        }
        if (PThreadSharedMemPtr[i].state ==  MTX_STATE_UNRECOVERABLE)
            reinit = 1;
        else {
            int ret = pthread_mutex_trylock(&(PThreadSharedMemPtr[i].mtx));
            if (ret == 0)
                pthread_mutex_unlock(&(PThreadSharedMemPtr[i].mtx));
            if (ret == EOWNERDEAD) {
                ci_debug_printf(1, "pthread_mutex_recover_after_crash: mutex %d needs recovery\n", i);
                pthread_mutex_consistent(&(PThreadSharedMemPtr[i].mtx));
                pthread_mutex_unlock(&(PThreadSharedMemPtr[i].mtx));
            } else if (ret != EBUSY)
                reinit = 1;
        }
        if (reinit) {
            ci_debug_printf(1, "pthread_mutex_recover_after_crash: Re-build mutex %d\n", i);
            pthread_mutex_destroy(&(PThreadSharedMemPtr[i].mtx));
            pmutex_init(&PThreadSharedMemPtr[i].mtx);
        }
    }
}

static ci_proc_mutex_scheme_t pthread_mutex_scheme = {
    pthread_proc_mutex_init,
    pthread_proc_mutex_destroy,
    pthread_proc_mutex_lock,
    pthread_proc_mutex_unlock,
    pthread_proc_mutex_print_info,
    "pthread"
};
#endif

#if defined(HAVE_PTHREADS_PROCESS_SHARED)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &pthread_mutex_scheme;
#elif defined(USE_POSIX_FILE_LOCK)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &file_mutex_scheme;
#elif defined(USE_SYSV_IPC_MUTEX)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &sysv_mutex_scheme;
#elif defined(USE_POSIX_SEMAPHORES)
const ci_proc_mutex_scheme_t *default_mutex_scheme = &posix_mutex_scheme;
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
#if defined(HAVE_PTHREADS_PROCESS_SHARED)
                if (strcasecmp(scheme, "pthread") == 0)
                    default_mutex_scheme = &pthread_mutex_scheme;
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

CI_DECLARE_FUNC(int) ci_proc_mutex_init2(ci_proc_mutex_t *mutex, const char *name, const char *scheme)
{
    const ci_proc_mutex_scheme_t *use_scheme = NULL;
#if defined(USE_SYSV_IPC_MUTEX)
    if (strcasecmp(scheme, "sysv") == 0)
        use_scheme = &sysv_mutex_scheme;
    else
#endif
#if defined(USE_POSIX_SEMAPHORES)
        if (strcasecmp(scheme, "posix") == 0)
            use_scheme = &posix_mutex_scheme;
        else
#endif
#if  defined(USE_POSIX_FILE_LOCK)
            if (strcasecmp(scheme, "file") == 0)
                use_scheme = &file_mutex_scheme;
            else
#endif

                use_scheme = NULL;

    if (use_scheme) {
        mutex->scheme = use_scheme;
        return use_scheme->proc_mutex_init(mutex, name);
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

int ci_proc_mutex_print_info(ci_proc_mutex_t *mutex, char *buf, size_t size)
{
    if (mutex->scheme && mutex->scheme->proc_mutex_print_info)
        return mutex->scheme->proc_mutex_print_info(mutex, buf, size);
    return snprintf(buf, size, "mutex:%s", mutex->name);
}

void ci_proc_mutex_recover_after_crash()
{
#if defined(HAVE_PTHREADS_PROCESS_SHARED)
    pthread_mutex_recover_after_crash();
#endif
}
