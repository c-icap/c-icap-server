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


#include <stdlib.h>
#include "c-icap.h"
#include "debug.h"
#include "shared_mem.h"



void *ci_shared_mem_create(ci_shared_mem_id_t *id,int size){
     HANDLE hMapFile;
     LPVOID lpMapAddress;
     SECURITY_ATTRIBUTES saAttr; 
 
     saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
     saAttr.bInheritHandle = TRUE; 
     saAttr.lpSecurityDescriptor = NULL;
 
     hMapFile = CreateFileMapping(
     INVALID_HANDLE_VALUE,    // current file handle 
     NULL,                              // default security 
     PAGE_READWRITE,                    // read/write permission 
     0,                              // First 32 bit of size (Not used) 
     size,                           // Second 32 bit of size 
     NULL);            // name of mapping object 
 
    if (hMapFile == NULL) 
    { 
        ci_debug_printf(1,"Could not create file mapping object:(error:%d)\n",
                        GetLastError()); 
        return NULL;
    }


    lpMapAddress = MapViewOfFile(hMapFile, // handle to mapping object 
    FILE_MAP_ALL_ACCESS,               // read/write permission 
    0,                                 // max. object size 
    0,                                 // size of hFile 
    0);                                // map entire file 
 
    if (lpMapAddress == NULL) 
    { 
        ci_debug_printf(1,"Could not map view of file.(error:%d)\n",
                        GetLastError());
        CloseHandle(hMapFile);
        return NULL; 
    }

    *id=hMapFile;
     return lpMapAddress;
}


void *ci_shared_mem_attach(ci_shared_mem_id_t *id){
    LPVOID lpMapAddress;
    lpMapAddress = MapViewOfFile(
        *id, // handle to mapping object 
        FILE_MAP_ALL_ACCESS,               // read/write permission 
        0,                                 // max. object size 
        0,                                 // size of hFile 
        0);                                // map entire file 
 
    if (lpMapAddress == NULL) 
    { 
        ci_debug_printf(1,"Could not map view of file.(error:%d)\n",
                        GetLastError());
        CloseHandle(*id);
        return NULL; 
    }
    return lpMapAddress;
}

int ci_shared_mem_detach(ci_shared_mem_id_t *id,void *shmem){
    CloseHandle(*id);
    UnmapViewOfFile(shmem);
    return 1;
}


int ci_shared_mem_destroy(ci_shared_mem_id_t *id,void *shmem){
    CloseHandle(*id);
    UnmapViewOfFile(shmem);
    return 1;
}


