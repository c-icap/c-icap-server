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

#ifndef __C_ICAP_STATS_H
#define __C_ICAP_STATS_H

#include "c-icap.h"
#include "atomic.h"
#include "ci_threads.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*Fucntions for use with modules and services */

/**
 \defgroup STAT c-icap API for keeping statistics for services and modules
 \ingroup API
*/

/**
 * Statistic types
 \ingroup STAT
 */
typedef enum ci_stat_type {
    CI_STAT_INT64_T, CI_STAT_KBS_T,
    STAT_INT64_T = CI_STAT_INT64_T, STAT_KBS_T = CI_STAT_KBS_T /*backward compatibility */
} ci_stat_type_t;

/**
 * Registers a statistic entry counter. The counter can count kilobytes
 * (CI_STAT_KBS_T type) or simple counter (CI_STAT_INT64_T type)
 \ingroup STAT
 \param label A name for this entry
 \param type The type of the entry.  The counter can count kilobytes
 *           (CI_STAT_KBS_T type) or simple counter (CI_STAT_INT64_T type)
 \param group The group under which this entry appeared in info page.
 \return An ID which can be used to update counter
*/
CI_DECLARE_FUNC(int) ci_stat_entry_register(const char *label, ci_stat_type_t type, const char *group);

/**
 * Increases by 'count' the counter 'ID', which must be of type CI_STAT_INT64_T
 \ingroup STAT
 */
CI_DECLARE_FUNC(void) ci_stat_uint64_inc(int ID, uint64_t count);

/**
 * Decreases by 'count' the counter 'ID', which must be of type CI_STAT_INT64_T
 \ingroup STAT
 */
CI_DECLARE_FUNC(void) ci_stat_uint64_dec(int ID, uint64_t count);

/**
 * Increases by 'count' bytes the counter 'ID', which must be of type
 * CI_STAT_KBS_T.
 \ingroup STAT
 */
CI_DECLARE_FUNC(void) ci_stat_kbs_inc(int ID, uint64_t count);

/**
 * Return the memory address where the CI_STAT_INT64_T counter is stored
 * The user can use this address to update the counter directly. In this
 * case the user is responsible to correctly lock the counter (eg using
 * ci_thread_mutex) before using it.
 * This function can work only after the statistics memory is initialised,
 * after the running child is forked. It can not be used in init and
 * post_init services and modules handlers.
 \ingroup STAT
 */
CI_DECLARE_FUNC(uint64_t *) ci_stat_uint64_ptr(int ID);

/**
 * Used to batch update statistics
 \ingroup STAT
 */
typedef struct ci_stat_item {
    ci_stat_type_t type;
    int Id;
    uint64_t count;
} ci_stat_item_t;

/**
 * Updates multiple statistic entries in one step
 \param stats An array with statistic entries ids and their increment values
 \param count The number of items of stats array
 \ingroup STAT
*/
CI_DECLARE_FUNC(void) ci_stat_update(const ci_stat_item_t *stats, int count);

typedef struct kbs {
    uint64_t bytes;
} kbs_t;
typedef struct kbs ci_kbs_t;

static inline ci_kbs_t ci_kbs_zero() {ci_kbs_t zero = {0}; return zero;}

static inline uint64_t ci_kbs_kilobytes(const ci_kbs_t *kbs)
{
    return (kbs->bytes >> 10);
}

static inline uint64_t ci_kbs_remainder_bytes(const ci_kbs_t *kbs)
{
    return (kbs->bytes & 0x3FF);
}

static inline void ci_kbs_update(ci_kbs_t *kbs, uint64_t bytes)
{
    assert(kbs);
    kbs->bytes += bytes;
}

static inline void ci_kbs_lock_and_update(ci_kbs_t *kbs, uint64_t bytes)
{
    assert(kbs);
    ci_atomic_add_u64(&(kbs->bytes), bytes);
}

static inline void ci_kbs_add_to(ci_kbs_t *add_to, const ci_kbs_t *kbs)
{
    assert(kbs);
    assert(add_to);
    add_to->bytes += kbs->bytes;
}

/**
 * Similar to ci_stat_uint64_ptr but for ci_kbs_t statistic type
 \ingroup STAT
*/
CI_DECLARE_FUNC(ci_kbs_t *) ci_stat_kbs_ptr(int ID);

/*Low level structures and functions*/
typedef struct ci_stat_value {
    union {
        uint64_t counter;
        ci_kbs_t kbs;
    };
} ci_stat_value_t;

typedef struct ci_stat {
    ci_stat_type_t type;
    ci_stat_value_t value;
} ci_stat_t;

#define MEMBLOCK_SIG 0xFAFA
typedef struct ci_stat_memblock {
    unsigned int sig;
    int stats_count;
    ci_stat_value_t stats[];
} ci_stat_memblock_t;

CI_DECLARE_FUNC(int) ci_stat_memblock_size(void);

CI_DECLARE_FUNC(ci_stat_memblock_t *) ci_stat_memblock_get(void);

CI_DECLARE_FUNC(void) ci_stat_entry_release_lists();

CI_DECLARE_FUNC(int) ci_stat_attach_mem(void *mem, int size,void (*release_mem)(void *));

CI_DECLARE_FUNC(void) ci_stat_allocate_mem();

CI_DECLARE_FUNC(void) ci_stat_release();

CI_DECLARE_FUNC(int) ci_stat_mastergroup_register(const char *group);

enum {
    CI_STAT_GROUP_NONE = -1,
    CI_STAT_GROUP_MASTER = -2
};

CI_DECLARE_FUNC(int) ci_stat_group_register(const char *group, const char *master_group);

CI_DECLARE_FUNC(int) ci_stat_group_add(const char *group);

CI_DECLARE_FUNC(int) ci_stat_group_find(const char *group);

CI_DECLARE_FUNC(void) ci_stat_groups_iterate(void *data, int (*group_call)(void *data, const char *name, int groupId, int masterGroupId));
CI_DECLARE_FUNC(void) ci_stat_statistics_iterate(void *data, int groupID, int (*stat_call)(void *data, const char *label, int ID, int gId, const ci_stat_t *stat));

/*Stats memblocks low level functions*/
CI_DECLARE_FUNC(void) ci_stat_memblock_merge(ci_stat_memblock_t *to_block, const ci_stat_memblock_t *from_block);
CI_DECLARE_FUNC(void) ci_stat_memblock_reset(ci_stat_memblock_t *block);

CI_DECLARE_FUNC(ci_stat_memblock_t *) ci_stat_memblock_init(void *mem, size_t mem_size);

CI_DECLARE_FUNC(int) ci_stat_memblock_check(const ci_stat_memblock_t *block);

static inline uint64_t ci_stat_memblock_get_counter(const ci_stat_memblock_t *block, int id) {
    assert(block);
    if (id < block->stats_count)
        return block->stats[id].counter;
    return 0;
}

static inline ci_kbs_t ci_stat_memblock_get_kbs(const ci_stat_memblock_t *block, int id) {
    assert(block);
    if (id < block->stats_count)
        return block->stats[id].kbs;
    const ci_kbs_t zero = {0};
    return zero;
}

#define STAT_INT64_INC(memblock, id, count) ci_atomic_add_u64(&(memblock->stats[id].counter), count)
#define STAT_INT64_DEC(memblock, id, count) ci_atomic_sub_u64(&(memblock->stats[id].counter), count)
#define STAT_KBS_INC(memblock, id, count) ci_kbs_lock_and_update(&(memblock->stats[id].kbs), count)
#define STAT_INT64_INC_NL(memblock, id, count) memblock->stats[id].counter += count
#define STAT_INT64_DEC_NL(memblock, id, count) memblock->stats[id].counter -= count
#define STAT_KBS_INC_NL(memblock, id, count) ci_kbs_update(&(memblock->stats[id].kbs), count)

static inline void ci_stat_membock_uint64_inc(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    assert(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_INC(mem_block, ID, count);
}

static inline void ci_stat_membock_uint64_dec(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    assert(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_DEC(mem_block, ID, count);
}

static inline void ci_stat_memblock_kbs_inc(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    assert(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_INC(mem_block, ID, count);
}

#ifdef __cplusplus
}
#endif

#endif
