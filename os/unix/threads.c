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
#include "ci_threads.h"


#ifndef USE_PTHREADS_RWLOCK
/*We can implement a better solution here using a mutex and a cond 
  object to simulate rwlocks (TODO)
*/
int ci_thread_rwlock_init(ci_thread_rwlock_t * rwlock)
{
     return pthread_mutex_init(rwlock, NULL);
}

int ci_thread_rwlock_destroy(ci_thread_rwlock_t * rwlock)
{
     return pthread_mutex_destroy(rwlock);
}

int ci_thread_rwlock_rdlock(ci_thread_rwlock_t * rwlock)
{
     return pthread_mutex_lock(rwlock);
}

int ci_thread_rwlock_wrlock(ci_thread_rwlock_t * rwlock)
{
     return pthread_mutex_lock(rwlock);
}

int ci_thread_rwlock_unlock(ci_thread_rwlock_t * rwlock)
{
     return pthread_mutex_unlock(rwlock);
}

#endif
