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


#ifndef _SHARED_MEM_H
#define _SHARED_MEM_H

#include "c-icap.h"

#ifdef USE_SYSV_IPC

#define ci_shared_mem_id_t int

#elif defined (USE_POSIX_MAPPED_FILES)

typedef struct ci_shared_mem_id {
     char *mem;
     int size;
} ci_shared_mem_id_t;


#elif defined (_WIN32)
#include <windows.h>
#define ci_shared_mem_id_t HANDLE

#endif

CI_DECLARE_FUNC(void) *ci_shared_mem_create(ci_shared_mem_id_t *id,int size);
CI_DECLARE_FUNC(void) *ci_shared_mem_attach(ci_shared_mem_id_t *id);
CI_DECLARE_FUNC(int)   ci_shared_mem_detach(ci_shared_mem_id_t *id,void *shmem);
CI_DECLARE_FUNC(int)   ci_shared_mem_destroy(ci_shared_mem_id_t *id,void *shmem);



#endif
