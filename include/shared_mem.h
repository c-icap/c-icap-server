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


#ifndef __C_ICAP_SHARED_MEM_H
#define __C_ICAP_SHARED_MEM_H

#include "c-icap.h"

#if defined (_WIN32)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ci_shared_mem_id ci_shared_mem_id_t;

typedef struct ci_shared_mem_scheme {
    void *(*shared_mem_create)(ci_shared_mem_id_t *id, const char *name, int size);
    void *(*shared_mem_attach)(ci_shared_mem_id_t *id);
    int (*shared_mem_detach)(ci_shared_mem_id_t *id);
    int (*shared_mem_destroy)(ci_shared_mem_id_t *id);
    int (*shared_mem_print_info)(ci_shared_mem_id_t *id, char *buf, size_t buf_size);
    const char *name;
} ci_shared_mem_scheme_t;


#define CI_SHARED_MEM_NAME_SIZE 64
struct ci_shared_mem_id {
    char name[CI_SHARED_MEM_NAME_SIZE];
    void *mem;
    size_t size;

#if defined (_WIN32)
    HANDLE id;
#else

    const ci_shared_mem_scheme_t *scheme;
    union {
#if defined (USE_POSIX_SHARED_MEM)
        struct posix {
            int fd;
        } posix;
#endif
#if defined (USE_SYSV_IPC)
        struct sysv {
            int id;
        } sysv;
#endif
        int id_;
    };
#endif
};

CI_DECLARE_FUNC(void) *ci_shared_mem_create(ci_shared_mem_id_t *id, const char *name, int size);
CI_DECLARE_FUNC(void) *ci_shared_mem_attach(ci_shared_mem_id_t *id);
CI_DECLARE_FUNC(int) ci_shared_mem_detach(ci_shared_mem_id_t *id);
CI_DECLARE_FUNC(int) ci_shared_mem_destroy(ci_shared_mem_id_t *id);


CI_DECLARE_FUNC(int) ci_shared_mem_set_scheme(const char *name);

#ifdef __cplusplus
}
#endif

#endif
