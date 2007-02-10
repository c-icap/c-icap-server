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

#include "ci_threads.h"


#ifndef HAVE_PTHREADS_RWLOCK
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
