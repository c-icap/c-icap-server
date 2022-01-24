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


#include "c-icap.h"
#include <errno.h>
#include "debug.h"
#include "proc_mutex.h"
#include <tchar.h>

int ci_proc_mutex_init(ci_proc_mutex_t * mutex, const char *name)
{
    snprintf(mutex->name, CI_PROC_MUTEX_NAME_SIZE, "Local\%s", name);
    /*TODO: use named mutex*/
    if ((mutex->id = CreateMutex(NULL, FALSE, NULL)) == NULL) {
        ci_debug_printf(1, "Error creating mutex:%d\n", GetLastError());
        return 0;
    }
    return 1;
}

int ci_proc_mutex_destroy(ci_proc_mutex_t * mutex)
{
    CloseHandle(mutex->id);
    return 1;
}

int ci_proc_mutex_lock(ci_proc_mutex_t * mutex)
{
    WaitForSingleObject(mutex->id, INFINITE);
    return 1;
}

int ci_proc_mutex_unlock(ci_proc_mutex_t * mutex)
{
    ReleaseMutex(mutex->id);
    return 1;
}

void ci_proc_mutex_recover_after_crash()
{
    /*Nothing to do*/
}
