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


#ifndef __CI_THREADS_H
#define __CI_THREADS_H

#include "c-icap.h"

#ifndef _WIN32

#include <pthread.h>

#define  ci_thread_mutex_t   pthread_mutex_t
#define  ci_thread_cond_t    pthread_cond_t
#define  ci_thread_t         pthread_t

#define  ci_thread_mutex_init(pmutex)  pthread_mutex_init(pmutex,NULL)
#define ci_thread_mutex_destroy(pmutex) pthread_mutex_destroy(pmutex)
#define ci_thread_mutex_lock(pmutex) pthread_mutex_lock(pmutex)
#define ci_thread_mutex_unlock(pmutex) pthread_mutex_unlock(pmutex)

#define  ci_thread_cond_init(pcond)   pthread_cond_init(pcond,NULL)
#define ci_thread_cond_destroy(pcond) pthread_cond_destroy(pcond)
#define ci_thread_cond_wait(pcond,pmutex) pthread_cond_wait(pcond,pmutex)
#define  ci_thread_cond_broadcast(pcond) pthread_cond_broadcast(pcond)
#define ci_thread_cond_signal(pcond)   pthread_cond_signal(pcond)


#define ci_thread_create(pthread_id,pfunc,parg) pthread_create(pthread_id, NULL,pfunc,parg)
#define ci_thread_join(thread_id)  pthread_join(thread_id,NULL)

#else /*ifdef _WIN32*/

#include <windows.h>

#define  ci_thread_mutex_t   CRITICAL_SECTION
#define  ci_thread_cond_t    HANDLE
#define  ci_thread_t         DWORD

CI_DECLARE_FUNC(int)  ci_thread_mutex_init(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_destroy(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_lock(ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int) ci_thread_mutex_unlock(ci_thread_mutex_t *pmutex);

CI_DECLARE_FUNC(int)  ci_thread_cond_init(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_destroy(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_wait(ci_thread_cond_t *pcond,ci_thread_mutex_t *pmutex);
CI_DECLARE_FUNC(int)  ci_thread_cond_broadcast(ci_thread_cond_t *pcond);
CI_DECLARE_FUNC(int) ci_thread_cond_signal(ci_thread_cond_t *pcond);


CI_DECLARE_FUNC(int) ci_thread_create(ci_thread_t *thread_id, void *(*pfunc)(void *), void *parg);
CI_DECLARE_FUNC(int) ci_thread_join(ci_thread_t thread_id);


#endif

#endif
