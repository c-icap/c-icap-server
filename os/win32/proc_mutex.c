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
#include <tchar.h>

int ci_proc_mutex_init(ci_proc_mutex_t *mutex){
     if((*mutex=CreateMutex(NULL,FALSE,NULL))==NULL){
	  ci_debug_printf(1,"Error creating mutex:%d\n",GetLastError());
	  return 0;
     }
     return 1;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t *mutex){
     CloseHandle(*mutex);
     return 1;
}

int ci_proc_mutex_lock(ci_proc_mutex_t *mutex){
     WaitForSingleObject(*mutex,INFINITE);
     return 1;
}
 
int ci_proc_mutex_unlock(ci_proc_mutex_t *mutex){
     ReleaseMutex(*mutex);
     return 1;
}

