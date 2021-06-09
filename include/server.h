/*
 *  Copyright (C) 2004-2021 Christos Tsantilas
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


#ifndef __C_ICAP_SERVER_H
#define __C_ICAP_SERVER_H

#include "stats.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*c-icap server statistics functions*/
/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * current child.
 \ingroup SERVER
 */
static inline ci_kbs_t ci_server_stat_kbs_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    assert(block);
    return  ci_stat_memblock_get_kbs(block, id);
}

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * current child.
 \ingroup SERVER
 */
static inline uint64_t ci_server_stat_uint64_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    assert(block);
    return  ci_stat_memblock_get_counter(block, id);
}

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * running children.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_running(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * running children.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_running(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * c-icap server. This is include history data.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_global(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * c-icap server. This is include history data.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_global(int id);

CI_DECLARE_FUNC(int) ci_server_shared_memblob_register(const char *name, size_t size);
CI_DECLARE_FUNC(void *) ci_server_shared_memblob(int ID);
CI_DECLARE_FUNC(void *) ci_server_shared_memblob_byname(const char *name);


#ifdef __cplusplus
}
#endif

#endif
