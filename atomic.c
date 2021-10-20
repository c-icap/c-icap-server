/*
 *  Copyright (C) 2004-2021 Christos Tsantilas
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
#include "atomic.h"
#include "ci_threads.h"
#include "debug.h"
#include "proc_mutex.h"

#if defined(__CI_INLINE_POSIX_ATOMICS)
__ci_implement_atomic_ops(,u64_non_inline, uint64_t);
__ci_implement_atomic_ops(,i64_non_inline, int64_t);
#if defined(CI_ATOMICS_USE_128BIT)
__ci_implement_atomic_ops(,u128_non_inline, unsigned __int128);
__ci_implement_atomic_ops(,i128_non_inline, __int128);
#endif

int ci_atomics_init()
{
    return 1;
}

#else

#define MTX_SIZE 5
static ci_thread_mutex_t thr_mtx[MTX_SIZE];
static ci_proc_mutex_t proc_mtx[MTX_SIZE];

#define get_mtx(array, pointer) (&array[((size_t)((void *)pointer - (void *)0)) % MTX_SIZE])

#define _implement_atomic_ops(name, type)                               \
    void ci_atomic_load_##name(type *counter, type *store) {            \
        ci_thread_mutex_t *mtx = get_mtx(thr_mtx, counter);             \
        ci_thread_mutex_lock(mtx);                                      \
        *counter = *store;                                              \
        ci_thread_mutex_unlock(mtx);                                    \
    }                                                                   \
    void ci_atomic_add_##name(type *counter, type add) {                \
        ci_thread_mutex_t *mtx = get_mtx(thr_mtx, counter);             \
        ci_thread_mutex_lock(mtx);                                      \
        *counter += add;                                                \
        ci_thread_mutex_unlock(mtx);                                   \
    }                                                                   \
    void ci_atomic_sub_##name(type *counter, type sub) {                \
        ci_thread_mutex_t *mtx = get_mtx(thr_mtx, counter);             \
        ci_thread_mutex_lock(mtx);                                     \
        *counter -= sub;                                                \
        ci_thread_mutex_unlock(mtx);                                    \
    }                                                                   \
    void ci_atomic_load_##name ## _gl(type *counter, type *store) {     \
        ci_proc_mutex_t *mtx = get_mtx(proc_mtx, counter);              \
        ci_proc_mutex_lock(mtx);                                        \
        *counter = *store;                                              \
        ci_proc_mutex_unlock(mtx);                                      \
    }                                                                   \
    void ci_atomic_add_##name ## _gl(type *counter, type add) {         \
        ci_proc_mutex_t *mtx = get_mtx(proc_mtx, counter);              \
        ci_proc_mutex_lock(mtx);                                        \
        *counter += add;                                                \
        ci_proc_mutex_unlock(mtx);                                      \
    }                                                                   \
    void ci_atomic_sub_##name ## _gl(type *counter, type sub) {         \
        ci_proc_mutex_t *mtx = get_mtx(proc_mtx, counter);              \
        ci_proc_mutex_lock(mtx);                                        \
        *counter -= sub;                                                \
        ci_proc_mutex_unlock(mtx);                                      \
    }

_implement_atomic_ops(u64_non_inline, uint64_t);
_implement_atomic_ops(i64_non_inline, int64_t);
#if defined(CI_ATOMICS_USE_128BIT)
_implement_atomic_ops(u128_non_inline, unsigned __int128);
_implement_atomic_ops(i128_non_inline, __int128);
#endif

int ci_atomics_init()
{
    int i;
    for (i = 0; i < MTX_SIZE; ++i ) {
        ci_thread_mutex_init(&(thr_mtx[i]));
    }
    /* File locks can not be used for threads.
       By default use posix semaphores, else sysv locks.
       TODO: implement interprocess pthread locking.
    */
    int ret = 0 , error = 0;
    for (i = 0; i < MTX_SIZE; ++i) {
        char name[128];
        snprintf(name, sizeof(name), "ci_atomic_%d", i);
#if defined(USE_POSIX_SEMAPHORES)
        ret = ci_proc_mutex_init2(&(proc_mtx[i]), name, "posix");
#elif defined(USE_SYSV_IPC_MUTEX)
        ret = ci_proc_mutex_init2(&(proc_mtx[i]), name, "sysv");
#endif
        if (!ret)
            error = 1;
    }
    return !error;
}

#endif
