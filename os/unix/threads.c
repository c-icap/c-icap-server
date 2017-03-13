/*
 *  Copyright (C) 2004-2012 Christos Tsantilas
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
#include "ci_threads.h"

typedef struct mutex_itm mutex_itm_t;
enum MTX_TYPE { MTX_MUTEX
#ifdef USE_PTHREADS_RWLOCK
                , MTX_RWLOCK
#endif
              };
struct mutex_itm {
    union {
        pthread_mutex_t *mutex;
#ifdef USE_PTHREADS_RWLOCK
        pthread_rwlock_t *rwlock;
#endif
    } mtx;
    int type;
    mutex_itm_t *next;
};

pthread_mutex_t mutexes_lock = PTHREAD_MUTEX_INITIALIZER;
static mutex_itm_t *mutexes = NULL;
static mutex_itm_t *last = NULL;

static int init_child_mutexes_scheduled = 0;

static void init_child_mutexes()
{
    mutex_itm_t *m;
    pthread_mutex_init(&mutexes_lock, NULL);
    for (m = mutexes; m != NULL; m = m->next) {
        switch (m->type) {
        case MTX_MUTEX:
            pthread_mutex_init(m->mtx.mutex, NULL);
            break;
#ifdef USE_PTHREADS_RWLOCK
        case MTX_RWLOCK:
            pthread_rwlock_init(m->mtx.rwlock, NULL);
            break;
#endif
        default: /*????????*/
            break;
        }
    }
}

static mutex_itm_t *add_mutex(void *pmutex, int type)
{
    mutex_itm_t *m = malloc(sizeof(mutex_itm_t));
    if (!m)
        return NULL;

    switch (type) {
    case MTX_MUTEX:
        m->mtx.mutex = (pthread_mutex_t *)pmutex;
        break;
#ifdef USE_PTHREADS_RWLOCK
    case MTX_RWLOCK:
        m->mtx.rwlock = (pthread_rwlock_t *)pmutex;
        break;
#endif
    default:
        free(m);
        return NULL;
    }

    m->type = type;
    m->next = NULL;
    pthread_mutex_lock(&mutexes_lock);
    if (mutexes == NULL) {
        mutexes = m;
        last = m;
    } else {
        last->next = m;
        last = last->next;
    }
    if (!init_child_mutexes_scheduled) {
        pthread_atfork(NULL, NULL, init_child_mutexes);
        init_child_mutexes_scheduled = 1;
    }
    pthread_mutex_unlock(&mutexes_lock);
    return m;
}

static void del_mutex(void *pmutex)
{
    mutex_itm_t *m,*p = NULL;

    pthread_mutex_lock(&mutexes_lock);
    for (m = mutexes; m != NULL; m = m->next) {
        if (m->mtx.mutex == pmutex) {
            if (p == NULL) /*first item*/
                mutexes = mutexes->next;
            else
                p->next = m->next;
            free(m);
            pthread_mutex_unlock(&mutexes_lock);
            return;
        }
        p = m;
    }
    pthread_mutex_unlock(&mutexes_lock);
}


int ci_thread_mutex_init(ci_thread_mutex_t *pmutex)
{
    int ret;
    ret = pthread_mutex_init(pmutex,NULL);
    if (ret != 0)
        return ret;
    add_mutex(pmutex, MTX_MUTEX); //assert result!=NULL?
    return ret;
}

int ci_thread_mutex_destroy(ci_thread_mutex_t *pmutex)
{
    del_mutex(pmutex);
    return pthread_mutex_destroy(pmutex);
}


#ifdef USE_PTHREADS_RWLOCK
int ci_thread_rwlock_init(ci_thread_rwlock_t *rwlock)
{
    int ret;
    ret = pthread_rwlock_init(rwlock,NULL);
    if (ret != 0)
        return ret;
    add_mutex(rwlock, MTX_RWLOCK); //assert result!=NULL?
    return ret;
}

int ci_thread_rwlock_destroy(ci_thread_rwlock_t *rwlock)
{
    del_mutex(rwlock);
    return pthread_rwlock_destroy(rwlock);
}

#else
/*We can implement a better solution here using a mutex and a cond
  object to simulate rwlocks (TODO)
*/
int ci_thread_rwlock_init(ci_thread_rwlock_t * rwlock)
{
    return ci_thread_mutex_init(rwlock, NULL);
}

int ci_thread_rwlock_destroy(ci_thread_rwlock_t * rwlock)
{
    return ci_thread_mutex_destroy(rwlock);
}

int ci_thread_rwlock_rdlock(ci_thread_rwlock_t * rwlock)
{
    return ci_thread_mutex_lock(rwlock);
}

int ci_thread_rwlock_wrlock(ci_thread_rwlock_t * rwlock)
{
    return ci_thread_mutex_lock(rwlock);
}

int ci_thread_rwlock_unlock(ci_thread_rwlock_t * rwlock)
{
    return ci_thread_mutex_unlock(rwlock);
}

#endif

int ci_thread_cond_init(ci_thread_cond_t *pcond)
{
    return pthread_cond_init(pcond,NULL);
}

int ci_thread_cond_destroy(ci_thread_cond_t *pcond)
{
    return pthread_cond_destroy(pcond);
}

int ci_thread_create(ci_thread_t *pthread_id, void *(*pfunc)(void *), void *parg)
{
    return pthread_create(pthread_id, NULL, pfunc, parg);
}

int ci_thread_join(ci_thread_t thread_id)
{
    return pthread_join(thread_id,NULL);
}
