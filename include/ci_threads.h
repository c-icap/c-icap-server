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


#ifndef __CI_THREADS_H
#define __CI_THREADS_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32

#include <pthread.h>

#define  ci_thread_mutex_t   pthread_mutex_t
#define  ci_thread_cond_t    pthread_cond_t
#define  ci_thread_t         pthread_t

CI_DECLARE_FUNC(int) ci_thread_mutex_init(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_destroy(ci_thread_mutex_t *pmutex);
#define ci_thread_mutex_lock(pmutex) pthread_mutex_lock(pmutex)
#define ci_thread_mutex_unlock(pmutex) pthread_mutex_unlock(pmutex)
#define ci_thread_self  pthread_self

#ifdef USE_PTHREADS_RWLOCK
#define ci_thread_rwlock_t pthread_rwlock_t
CI_DECLARE_FUNC(int) ci_thread_rwlock_init(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_destroy(ci_thread_rwlock_t *);
#define ci_thread_rwlock_rdlock(rwlock) pthread_rwlock_rdlock(rwlock)
#define ci_thread_rwlock_wrlock(rwlock) pthread_rwlock_wrlock(rwlock)
#define ci_thread_rwlock_unlock(rwlock) pthread_rwlock_unlock(rwlock)
#else
#define ci_thread_rwlock_t pthread_mutex_t
CI_DECLARE_FUNC(int) ci_thread_rwlock_init(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_destroy(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_rdlock(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_wrlock(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_unlock(ci_thread_rwlock_t *);
#endif

CI_DECLARE_FUNC(int) ci_thread_cond_init(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_destroy(ci_thread_cond_t *pcond);
#define ci_thread_cond_wait(pcond,pmutex) pthread_cond_wait(pcond,pmutex)
#define  ci_thread_cond_broadcast(pcond) pthread_cond_broadcast(pcond)
#define ci_thread_cond_signal(pcond)   pthread_cond_signal(pcond)

CI_DECLARE_FUNC(int) ci_thread_create(ci_thread_t *pthread_id, void *(*pfunc)(void *), void *parg);
CI_DECLARE_FUNC(int) ci_thread_join(ci_thread_t thread_id);

#else /*ifdef _WIN32*/

#include <windows.h>

#define  ci_thread_mutex_t   CRITICAL_SECTION
#define  ci_thread_rwlock_t  CRITICAL_SECTION
#define  ci_thread_cond_t    HANDLE
#define  ci_thread_t         DWORD

CI_DECLARE_FUNC(int)  ci_thread_mutex_init(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_destroy(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_lock(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_unlock(ci_thread_mutex_t *pmutex);

CI_DECLARE_FUNC(int) ci_thread_rwlock_init(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_destroy(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_rdlock(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_wrlock(ci_thread_rwlock_t *);
CI_DECLARE_FUNC(int) ci_thread_rwlock_unlock(ci_thread_rwlock_t *);

CI_DECLARE_FUNC(int)  ci_thread_cond_init(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_destroy(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_wait(ci_thread_cond_t *pcond,ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int)  ci_thread_cond_broadcast(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_signal(ci_thread_cond_t *pcond);


CI_DECLARE_FUNC(int) ci_thread_create(ci_thread_t *thread_id, void *(*pfunc)(void *), void *parg);
CI_DECLARE_FUNC(int) ci_thread_join(ci_thread_t thread_id);

#endif

#ifdef __cplusplus
}
#endif

#endif /*__CI_THREADS_H */
