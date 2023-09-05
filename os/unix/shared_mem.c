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
#include "debug.h"
#include "shared_mem.h"
#include "util.h"

#include <assert.h>
#if defined(USE_SYSV_IPC)
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#if defined(USE_POSIX_MAPPED_FILES)
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <errno.h>

#define  PERMS 0600

#if defined(USE_SYSV_IPC)

void *sysv_shared_mem_create(ci_shared_mem_id_t * id, const char *name, int size)
{
    assert(id);
    if ((id->sysv.id = shmget(IPC_PRIVATE, size, PERMS | IPC_CREAT)) < 0) {
        char err[128];
        ci_debug_printf(1, "Error creating sysv shared mem '%s': %s\n", name, ci_strerror(errno, err, sizeof(err)));
        return NULL;
    }

    if ((id->mem = shmat(id->sysv.id, NULL, 0)) == (void *) -1) {
        char err[128];
        ci_debug_printf(1, "Error attaching/creating sysv shared mem '%s': %s\n", name, ci_strerror(errno, err, sizeof(err)));
        return NULL;
    }

    id->size = size;
    snprintf(id->name, CI_SHARED_MEM_NAME_SIZE, "%s", name);
    return id->mem;
}


void *sysv_shared_mem_attach(ci_shared_mem_id_t * id)
{
    assert(id);
    if ((id->mem = shmat(id->sysv.id, NULL, 0)) == (void *) -1) {
        char err[128];
        ci_debug_printf(1, "Error creating sysv shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
        return NULL;
    }
    return id->mem;
}

int sysv_shared_mem_detach(ci_shared_mem_id_t * id)
{
    assert(id);
    if (shmdt(id->mem) < 0) {
        char err[128];
        ci_debug_printf(1, "Error detaching sysv shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
        return 0;
    }
    return 1;
}


int sysv_shared_mem_destroy(ci_shared_mem_id_t * id)
{
    assert(id);
    if (shmdt(id->mem) < 0) {
        char err[128];
        ci_debug_printf(1, "Error detaching/destroying sysv shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
        return 0;
    }

    if (shmctl(id->sysv.id, IPC_RMID, NULL) < 0) {
        char err[128];
        ci_debug_printf(1, "Error removing sysv shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
        return 0;
    }

    return 1;
}

int sysv_shared_mem_print_info(ci_shared_mem_id_t *id, char *buf, size_t buf_size)
{
    assert(id);
    return snprintf(buf, buf_size, "sysv:%s/%d %ld kbs", id->name, id->sysv.id, (long)(id->size/1024));
}

const ci_shared_mem_scheme_t sysv_scheme = {
    sysv_shared_mem_create,
    sysv_shared_mem_attach,
    sysv_shared_mem_detach,
    sysv_shared_mem_destroy,
    sysv_shared_mem_print_info,
    "sysv"
};

#endif


#if defined(USE_POSIX_MAPPED_FILES)

void *mmap_shared_mem_create(ci_shared_mem_id_t * id, const char *name, int size)
{
    assert(id);
    if ((id->mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        char err[128];
        ci_debug_printf(1, "Error creating mmap shared mem '%s': %s\n", name, ci_strerror(errno, err, sizeof(err)));
        return NULL;
    }
    id->size = size;
    snprintf(id->name, CI_SHARED_MEM_NAME_SIZE, "%s", name);
    return id->mem;
}


void *mmap_shared_mem_attach(ci_shared_mem_id_t * id)
{
    assert(id);
    return (void *) (id->mem);
}


int mmap_shared_mem_detach(ci_shared_mem_id_t * id)
{
    return 1;
}


int mmap_shared_mem_destroy(ci_shared_mem_id_t * id)
{
    assert(id);
    if (munmap(id->mem, id->size) != 0) {
        char err[128];
        ci_debug_printf(1, "Error destroying mmap shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
    }
    return 1;
}

int mmap_shared_mem_print_info(ci_shared_mem_id_t *id, char *buf, size_t buf_size)
{
    assert(id);
    return snprintf(buf, buf_size, "mmap:%s/%p %ld kbs", id->name, id->mem, (long)(id->size/1024));
}

const ci_shared_mem_scheme_t mmap_scheme = {
    mmap_shared_mem_create,
    mmap_shared_mem_attach,
    mmap_shared_mem_detach,
    mmap_shared_mem_destroy,
    mmap_shared_mem_print_info,
    "mmap"
};

#endif

#if defined (USE_POSIX_SHARED_MEM)

#define CI_SHARED_MEM_NAME_TMPL "/c-icap-shared"

void *posix_shared_mem_create(ci_shared_mem_id_t * id, const char *name, int size)
{
    int i, ret;
    assert(id);
    id->size = size;
    for (i = 0; i < 1024; ++i) {
        errno = 0;
        snprintf(id->name, CI_SHARED_MEM_NAME_SIZE, "%s-%s.%d", CI_SHARED_MEM_NAME_TMPL, name, i);
        id->posix.fd = shm_open(id->name, O_RDWR | O_CREAT | O_EXCL, S_IREAD | S_IWRITE);
        if (id->posix.fd < 0) {
            char err[128];
            if (errno == EEXIST)
                continue;
            ci_debug_printf(2, "Posix shared mem '%s': Failed to create posix posix shared mem: %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
            return NULL;
        }
        ret = ftruncate(id->posix.fd, id->size);
        if (ret < 0) {
            char err[128];
            ci_debug_printf(2, "Posix shared mem '%s': Failed to truncate posix file: %s! Ignoring\n", id->name, ci_strerror(errno, err, sizeof(err)));
        }
        id->mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, id->posix.fd, 0);
        if (id->mem == MAP_FAILED) {
            char err[128];
            ci_debug_printf(1, "Error mmap posix shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
            return NULL;
        }
        return id->mem;
    }
    ci_debug_printf(1, "Error creating posix shared mem %s. Check for stalled c-icap posix shared memory\n", id->name);
    return NULL;
}


void *posix_shared_mem_attach(ci_shared_mem_id_t * id)
{
    assert(id);
    return (id->mem);
}

int posix_shared_mem_detach(ci_shared_mem_id_t * id)
{
    assert(id);
    munmap(id->mem, id->size);
    /* close(id->posix.fd); Is this needed?*/
    id->mem = NULL;
    return 1;
}

int posix_shared_mem_destroy(ci_shared_mem_id_t * id)
{
    assert(id);
    if (munmap(id->mem, id->size) != 0) {
        char err[128];
        ci_debug_printf(1, "Error  unmaping/destroying posix shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
    }
    close(id->posix.fd);
    id->mem = NULL;
    if (shm_unlink(id->name) != 0) {
        char err[128];
        ci_debug_printf(1, "Error destroying posix shared mem '%s': %s\n", id->name, ci_strerror(errno, err, sizeof(err)));
    }
    return 1;
}

int posix_shared_mem_print_info(ci_shared_mem_id_t *id, char *buf, size_t buf_size)
{
    assert(id);
    return snprintf(buf, buf_size, "posix:%s %ld kbs", id->name, (long)(id->size/1024));
}

const ci_shared_mem_scheme_t posix_scheme = {
    posix_shared_mem_create,
    posix_shared_mem_attach,
    posix_shared_mem_detach,
    posix_shared_mem_destroy,
    posix_shared_mem_print_info,
    "posix"
};

#endif

#if defined(USE_POSIX_SHARED_MEM)
const ci_shared_mem_scheme_t *default_scheme = &posix_scheme;
#elif defined(USE_POSIX_MAPPED_FILES)
const ci_shared_mem_scheme_t *default_scheme = &mmap_scheme;
#elif defined(USE_SYSV_IPC)
const ci_shared_mem_scheme_t *default_scheme = &sysv_scheme;
#else
const ci_shared_mem_scheme_t *default_scheme = NULL;
#endif

int ci_shared_mem_set_scheme(const char *name)
{
#if defined(USE_POSIX_SHARED_MEM)
    if (strcasecmp(name, "posix") == 0)
        default_scheme = &posix_scheme;
    else
#endif
#if defined(USE_POSIX_MAPPED_FILES)
        if (strcasecmp(name, "mmap") == 0)
            default_scheme = &mmap_scheme;
        else
#endif
#if defined(USE_SYSV_IPC)
            if (strcasecmp(name, "sysv") == 0)
                default_scheme = &sysv_scheme;
            else
#endif
            {
                ci_debug_printf(1, "Shared mem scheme '%s' does not supported by c-icap\n", name);
                return 0;
            }

    return 1;
}

void *ci_shared_mem_create(ci_shared_mem_id_t * id, const char *name, int size)
{
    if (!default_scheme)
        return NULL;

    assert(id);
    id->scheme = default_scheme;
    return default_scheme->shared_mem_create(id, name, size);
}


void *ci_shared_mem_attach(ci_shared_mem_id_t * id)
{
    assert(id && id->scheme);
    return id->scheme->shared_mem_attach(id);
}


int ci_shared_mem_detach(ci_shared_mem_id_t * id)
{
    assert(id && id->scheme);
    return id->scheme->shared_mem_detach(id);
}


int ci_shared_mem_destroy(ci_shared_mem_id_t * id)
{
    assert(id && id->scheme);
    return id->scheme->shared_mem_destroy(id);
}

int ci_shared_mem_print_info(ci_shared_mem_id_t *id, char *buf, size_t buf_size)
{
    assert(id && id->scheme);
    if (id->scheme->shared_mem_print_info)
        return id->scheme->shared_mem_print_info(id, buf, buf_size);
    return snprintf(buf, buf_size, "shared_mem:%s", id->name);
}
