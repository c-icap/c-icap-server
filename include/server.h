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

/**
 \defgroup SERVER c-icap server API
 \ingroup API
 * Functions to interact with c-icap server
 */

/*c-icap server statistics functions*/
/**
 *
 \defgroup SERVER_STATS c-icap server statistics API
 \ingroup SERVER
 * Functions used to retrieve c-icap server statistic values.
 * The c-icap server statistics are stored per child process,
 * and the ci_stat_* library functions operate on local child
 * statistic variables.
 * The ci_server_stat_* functions collect statistic values from
 * running children  to compute the valid c-icap server statistic
 * values.
 */

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * current child.
 * This is similar to the ci_stat_kbs_get function, but asserts if
 * the c-icap server statistics subsystem is not configured.
 \ingroup SERVER_STATS
 */
static inline ci_kbs_t ci_server_stat_kbs_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    _CI_ASSERT(block);
    return  ci_stat_memblock_get_kbs(block, id);
}

/**
 * Retrieves the value of an integer statistic entry (of the type
 * STAT_INT64_T,CI_STAT_TIME_*_T or CI_STAT_INT64_MEAN_T) for the
 * current child.
 * This is similar to the ci_stat_uint64_get function, but asserts
 * if c-icap statistics subsystem is not configured.
 \ingroup SERVER_STATS
 */
static inline uint64_t ci_server_stat_uint64_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    _CI_ASSERT(block);
    return  ci_stat_memblock_get_counter(block, id);
}

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * running children.
 \ingroup SERVER_STATS
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_running(int id);

/**
 * Retrieves the value of an integer statistic entry for the
 * running children.
 \ingroup SERVER_STATS
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_running(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * c-icap server. The returned value includes history statistic data.
 \ingroup SERVER_STATS
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_global(int id);

/**
 * Retrieves the value of an integer statistic entry for the
 * c-icap server. The returned value includes history statistic data.
 \ingroup SERVER_STATS
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_global(int id);

/*c-icap server statistics functions*/
/**
 *
 \defgroup SERVER_MEMBLOB c-icap server shared memblobs
 \ingroup SERVER
 * Shared by c-icap children processes memory blocks for use by
 * services or modules.
 */

/**
 * Request a shared memory block from the c-icap server. It must run
 * before the c-icap children are started, eg in services/modules initialize
 * handlers, else will fail.
 \ingroup SERVER_MEMBLOB
 \param name A name to identify this memblob
 \param size The desired size of the memblob
 \return An identifier to use to access the memory block
*/
CI_DECLARE_FUNC(int) ci_server_shared_memblob_register(const char *name, size_t size);

/**
 *
 \returns a pointer to the memory block
 \param ID the memblob ID, returned by a ci_server_shared_memblob_register call
 \ingroup SERVER_MEMBLOB
*/
CI_DECLARE_FUNC(void *) ci_server_shared_memblob(int ID);

/**
 *
 \returns a pointer to the memory block
 \param name The name used to register/request the memory block.
 \ingroup SERVER_MEMBLOB
 */
CI_DECLARE_FUNC(void *) ci_server_shared_memblob_byname(const char *name);


#ifdef __cplusplus
}
#endif

#endif
