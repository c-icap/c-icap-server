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


#include "c-icap.h"
#include <errno.h>
#include "debug.h"
#include "proc_mutex.h"




#if defined(HAVE_SYSV_IPC)

#define  SEMKEY 888888L  /*A key but what key;The IPC_PRIVATE must used instead .....*/
#define  PERMS 0600 
static int current_semkey=SEMKEY; 

static struct sembuf op_lock[2]={
     0,0,0,             /*wait for sem to become 0 */
     0,1,SEM_UNDO      /*then increment sem by 1  */
};

static struct sembuf op_unlock[1]={
     0,-1, (IPC_NOWAIT|SEM_UNDO)      /*decrement sem by 1   */
};


int ci_proc_mutex_init(ci_proc_mutex_t *mutex){
     if((*mutex=semget(IPC_PRIVATE,1,IPC_CREAT|PERMS))<0 ){
	  debug_printf(1,"Error creating mutex");
	  return 0;
     }
     return 1;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex){
     if(semctl(*mutex,0,IPC_RMID,0)<0){
	  debug_printf(1,"Error removing mutex");
	  return 0;
     }
     return 1;
}

int ci_proc_mutex_lock(ci_proc_mutex_t *mutex){
     if(semop(*mutex, (struct sembuf *)&op_lock,2)<0){
	  return 0;
     }
     return 1;
}

int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex){
     if(semop(*mutex,(struct sembuf *)&op_unlock,1)<0){
	  return 0;
     }
     return 1;
}

#elif defined (HAVE_POSIX_SEMAPHORES)

int ci_proc_mutex_init(ci_proc_mutex_t *mutex){
  if(sem_init(mutex,1,1)<0){
    debug_printf(1,"An error ocured (errno:%d, ENOSYS:%d)\n",errno,ENOSYS);
    return 0;
  }
  return 1;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex){
  if(sem_destroy(mutex)<0){
    return 0;
  }
  return 1;
}

int ci_proc_mutex_lock(ci_proc_mutex_t *mutex){
  sem_wait(mutex);
  return 1;
}

int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex){
  sem_post(mutex);
     return 1;
}

#elif defined (HAVE_POSIX_FILE_LOCK)

/*NOTE: mkstemp does not exists for some platforms */
 
int ci_proc_mutex_init(ci_proc_mutex_t *mutex){
     strcpy(mutex->filename,FILE_LOCK_TEMPLATE);
     if((mutex->fd=mkstemp(mutex->filename))<0)
	  return 0;
     
     return 1;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex){
     close(mutex->fd);
     if(unlink(mutex->filename)!=0)
	  return 0;
     return 1;
}

int ci_proc_mutex_lock(ci_proc_mutex_t *mutex){
     struct flock fl;
     fl.l_type=F_WRLCK;
     fl.l_whence=SEEK_SET;
     fl.l_start=0;
     fl.l_len=0;

     if(fcntl(mutex->fd,F_SETLKW,&fl)<0)
	  return 0;
     return 1;
}

int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex){
     struct flock fl;
     fl.l_type=F_UNLCK;
     fl.l_whence=SEEK_SET;
     fl.l_start=0;
     fl.l_len=0;
     if(fcntl(mutex->fd,F_SETLK,&fl)<0)
	  return 0;
     return 1;
}


#endif
