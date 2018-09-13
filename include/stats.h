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

#ifndef __STATS_H
#define __STATS_H

#include "c-icap.h"
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
CI_DECLARE_FUNC(void) ci_stat_uint64_inc(int ID, int count);

/**
 * Increases by 'count' bytes the counter 'ID', which must be of type
 * CI_STAT_KBS_T.
 \ingroup STAT
 */
CI_DECLARE_FUNC(void) ci_stat_kbs_inc(int ID, int count);

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
    int count;
} ci_stat_item_t;

/**
 * Updates multiple statistic entries in one step
 \param stats An array with statistic entries ids and their increment values
 \param count The number of items of stats array
 \ingroup STAT
*/
CI_DECLARE_FUNC(void) ci_stat_update(const ci_stat_item_t *stats, int count);

/*Low level structures and functions*/
typedef struct kbs {
    uint64_t kb;
    unsigned int bytes;
} kbs_t;

#define MEMBLOCK_SIG 0xFAFA
struct stat_memblock {
    unsigned int sig;
    int counters64_size;
    int counterskbs_size;
    uint64_t *counters64;
    kbs_t *counterskbs;
};

struct stat_area {
    ci_thread_mutex_t mtx;
    void (*release_mem)(void *);
    struct stat_memblock *mem_block;
};


struct stat_entry {
    char *label;
    int type;
    int gid;
};

struct stat_entry_list {
    struct stat_entry *entries;
    int size;
    int entries_num;
};

struct stat_groups_list {
    char **groups;
    int size;
    int entries_num;
};

CI_DECLARE_DATA extern struct stat_entry_list STAT_INT64;
CI_DECLARE_DATA extern struct stat_entry_list STAT_KBS;
CI_DECLARE_DATA extern struct stat_groups_list STAT_GROUPS;

CI_DECLARE_DATA extern struct stat_area *STATS;

CI_DECLARE_FUNC(int) ci_stat_memblock_size(void);

CI_DECLARE_FUNC(void) ci_stat_entry_release_lists();

CI_DECLARE_FUNC(void) ci_stat_attach_mem(void *mem_block,
        int size,void (*release_mem)(void *));
CI_DECLARE_FUNC(void) ci_stat_release();

CI_DECLARE_FUNC(struct stat_area *) ci_stat_area_construct(void *mem_block, int size, void (*release_mem)(void *));
CI_DECLARE_FUNC(void) ci_stat_area_destroy(struct stat_area  *area);
CI_DECLARE_FUNC(void) ci_stat_area_reset(struct stat_area *area);
CI_DECLARE_FUNC(void) ci_stat_area_merge(struct stat_area *dest, struct stat_area *src);

/*Stats memblocks low level functions*/
CI_DECLARE_FUNC(void) ci_stat_memblock_merge(struct stat_memblock *dest_block, struct stat_memblock *mem_block);
CI_DECLARE_FUNC(void) ci_stat_memblock_reset(struct stat_memblock *block);

/*DO NOT USE the folllowings are only for internal c-icap server use!*/
CI_DECLARE_FUNC(void) stat_memblock_fix(struct stat_memblock *mem_block);
CI_DECLARE_FUNC(void) stat_memblock_reconstruct(struct stat_memblock *mem_block);

/*Private defines and functions*/
#define STATS_LOCK() ci_thread_mutex_lock(&STATS->mtx)
#define STATS_UNLOCK() ci_thread_mutex_unlock(&STATS->mtx)
#define STATS_INT64_INC(ID, count) (STATS->mem_block->counters64[ID] += count)
#define STATS_KBS_INC(ID, count) (STATS->mem_block->counterskbs[ID].bytes += count, STATS->mem_block->counterskbs[ID].kb += (STATS->mem_block->counterskbs[ID].bytes >> 10), STATS->mem_block->counterskbs[ID].bytes &= 0x3FF)

#ifdef __cplusplus
}
#endif

#endif
