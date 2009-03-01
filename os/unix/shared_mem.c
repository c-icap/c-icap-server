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
#include "shared_mem.h"

#ifdef USE_SYSV_IPC
#include <sys/ipc.h>
#include <sys/shm.h>
#elif defined USE_POSIX_MAPPED_FILES
#include <sys/mman.h>
#endif




#define  PERMS 0600

#ifdef USE_SYSV_IPC

void *ci_shared_mem_create(ci_shared_mem_id_t * id, int size)
{
     void *c;
     if ((*id = shmget(IPC_PRIVATE, size, PERMS | IPC_CREAT)) < 0) {
          return NULL;
     }

     if ((c = shmat(*id, NULL, 0)) == (void *) -1) {
          return NULL;
     }
     return c;
}


void *ci_shared_mem_attach(ci_shared_mem_id_t * id)
{
     void *c;
     if ((c = shmat(*id, NULL, 0)) == (void *) -1) {
          return NULL;
     }
     return c;
}

int ci_shared_mem_detach(ci_shared_mem_id_t * id, void *shmem)
{
     if (shmdt(shmem) < 0) {
          return 0;
     }
     return 1;
}


int ci_shared_mem_destroy(ci_shared_mem_id_t * id, void *shmem)
{
     if (shmdt(shmem) < 0)
          return 0;

     if (shmctl(*id, IPC_RMID, NULL) < 0)
          return 0;

     return 1;
}

#elif defined USE_POSIX_MAPPED_FILES

void *ci_shared_mem_create(ci_shared_mem_id_t * id, int size)
{

     if ((id->mem =
          mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,
               0)) < 0)
          return NULL;

     id->size = size;

     return id->mem;
}


void *ci_shared_mem_attach(ci_shared_mem_id_t * id)
{
     return (void *) (id->mem);
}


int ci_shared_mem_detach(ci_shared_mem_id_t * id, void *shmem)
{

     return 1;
}


int ci_shared_mem_destroy(ci_shared_mem_id_t * id, void *shmem)
{
     munmap(id->mem, id->size);
     return 1;
}


#endif
