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

#ifndef __C_ICAP_STATS_H
#define __C_ICAP_STATS_H

#include "c-icap.h"
#include "atomic.h"
#include "ci_threads.h"
#include "debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*Fucntions for use with modules and services */

/**
 \defgroup STAT c-icap library API for keeping statistics
 \ingroup API
 * Typical use of these API in a c-icap service can be:
 \code
 static int MyCounterId = -1;
 int myservice_init(ci_service_xdata_t *srv_xdata,struct ci_server_conf *server_conf)
{
    // MyCounter will be shown in c-icap info statistics page under the
    // statistic table "Service myservice".
    MyCounterId = ci_stat_entry_register("MyCounter", CI_STAT_INT64_T, "Service myservice");
}
...
 //Eg somewhere inside service io handler or inside preview handler:
   ci_stat_uint64_inc(MyCounterId, 1);
...
 \endcode
* Also check the \ref SERVER_STATS for functions to use to retrieve
* current c-icap server statistic values.
*/

/**
 * Statistic types
 \ingroup STAT
 * Express various statistic types. All statistics are kept per c-icap
 * child process and the children values sum (counters or accumulated
 * kilobytes) or the values mean (mean/average type statistics) are
 * shown.
 */
typedef enum ci_stat_type {
    CI_STAT_INT64_T, /*!< An unsigned integer counter */
    CI_STAT_KBS_T, /*!< Accumulated kilobytes */
    STAT_INT64_T = CI_STAT_INT64_T, STAT_KBS_T = CI_STAT_KBS_T, /*backward compatibility */
    CI_STAT_TIME_US_T, /*!< Mean/average time expressed in microseconds */
    CI_STAT_TIME_MS_T,  /*!< Mean/average time expressed in milliseconds */
    CI_STAT_INT64_MEAN_T, /*!< A mean unsigned integer value */
    CI_STAT_TYPE_END
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
 * Search for that statistic entry counter "label" in statistics group
 * "group" of type "type".
 \ingroup STAT
 \param label The entry name
 \param group The entry group name
 \param type The entry type
 \return The statistic entry ID or -1
*/
CI_DECLARE_FUNC(int) ci_stat_entry_find(const char *label, const char *group, ci_stat_type_t type);

/**
 * Return the statistic type of the given statistic entry
 \ingroup STAT
 \param id The statistic entry ID
 \return The statistic type
 */
CI_DECLARE_FUNC(ci_stat_type_t) ci_stat_entry_type_by_id(int id);

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
 * Sets the statistic entry 'ID' to the 'value'. It can be any statistic type.
 * For kbs statistic types the value means 'bytes'.
 \ingroup STAT
 */
CI_DECLARE_FUNC(void) ci_stat_value_set(int ID, uint64_t value);

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
CI_DECLARE_FUNC(_CI_ATOMIC_TYPE uint64_t *) ci_stat_uint64_ptr(int ID);

/**
 * Used to batch update statistics
 \ingroup STAT
 */
typedef struct ci_stat_item {
    /**
       \brief The type of statistic entry to update
     */
    ci_stat_type_t type;

    /**
       \brief The Id of statistic entry to update
    */
    int Id;

    union {
        /**
           \brief used to increase or decrease counters
        */
        int64_t count;

        /**
           \brief used to set unsigned types, like the CI_STAT_TIME_*
        */
        uint64_t value;
    };
} ci_stat_item_t;

/**
 * Updates multiple statistic entries in one step
 \param stats An array with statistic entries ids and their increment values
 \param count The number of items of stats array
 * example usage:
 \code
 int REQUESTS_ID = -1;
 int FAILURES_ID = -1;
 ...
 REQUESTS_ID = ci_stat_entry_register(...);
 FAILURES_ID = ci_stat_entry_register(...);
 ...
 ci_stat_item_t stats[3] = {
     {CI_STAT_INT64_T, REQUESTS_ID, count : 1},
     {CI_STAT_INT64_T, FAILURES_ID, count : 1},
 }
 ci_stat_update(stats, 2);
 \endcode
 \ingroup STAT
*/
CI_DECLARE_FUNC(void) ci_stat_update(const ci_stat_item_t *stats, int count);

/**
 * Return the current value of an integer (STAT_INT64_T, CI_STAT_TIME_*_T
 * or CI_STAT_INT64_MEAN_T) statistic entry.
 \ingroup STAT
 */
CI_DECLARE_FUNC(uint64_t) ci_stat_uint64_get(int ID);

/**
 \typedef ci_kbs_t
 * Represents (accumulated) kilobytes
 \ingroup STAT
 */
typedef struct kbs {
    _CI_ATOMIC_TYPE uint64_t bytes;
} kbs_t;
typedef struct kbs ci_kbs_t;

/**
 * Return the value of a statistic entry of type CI_STAT_KBS_T.
 \ingroup STAT
*/
CI_DECLARE_FUNC(ci_kbs_t) ci_stat_kbs_get(int ID);


/**
 \defgroup KBS    ci_kbs_t operations
 \ingroup STAT
 * functions for manipulating ci_kbs_t objects
*/

/**
 * Zeroes a ci_kbs_t object
 \ingroup KBS
*/
static inline ci_kbs_t ci_kbs_zero() {ci_kbs_t zero = {0}; return zero;}

static inline uint64_t ci_kbs_kilobytes(const ci_kbs_t *kbs)
{
    return (kbs->bytes >> 10);
}

static inline uint64_t ci_kbs_remainder_bytes(const ci_kbs_t *kbs)
{
    return (kbs->bytes & 0x3FF);
}

/**
 * Append/add "bytes" to the given "kbs" ci_kbs_t object counter
 \ingroup KBS
*/
static inline void ci_kbs_update(ci_kbs_t *kbs, uint64_t bytes)
{
    _CI_ASSERT(kbs);
    /* Warning: kbs.bytes is an _Atomic, do not use compound assignment
       to avoid atomic operation */
    kbs->bytes = kbs->bytes + bytes;
}

/**
 * Update atomically the given "kbs" object
 \ingroup KBS
*/
static inline void ci_kbs_lock_and_update(ci_kbs_t *kbs, uint64_t bytes)
{
    _CI_ASSERT(kbs);
    ci_atomic_add_u64(&(kbs->bytes), bytes);
}

/**
 * Appends "kbs"  to "add_to"
 \ingroup KBS
*/
static inline void ci_kbs_add_to(ci_kbs_t *add_to, const ci_kbs_t *kbs)
{
    _CI_ASSERT(kbs);
    _CI_ASSERT(add_to);
    /* Warning: kbs.bytes is an _Atomic, do not use compound assignment
       to avoid atomic operation */
    add_to->bytes = add_to->bytes + kbs->bytes;
}

/**
 * Subtract two ci_kbs_t objects
 \ingroup KBS
*/
static inline ci_kbs_t ci_kbs_sub(ci_kbs_t *kbs1, const ci_kbs_t *kbs2)
{
    _CI_ASSERT(kbs1);
    _CI_ASSERT(kbs2);
    ci_kbs_t res;
    res.bytes = kbs1->bytes - kbs2->bytes;
    return res;
}

/**
 * Adds two ci_kbs_t objects
 \ingroup KBS
*/
static inline ci_kbs_t ci_kbs_add(ci_kbs_t *kbs1, const ci_kbs_t *kbs2)
{
    _CI_ASSERT(kbs1);
    _CI_ASSERT(kbs2);
    ci_kbs_t res;
    res.bytes = kbs1->bytes + kbs2->bytes;
    return res;
}

/**
 * Similar to ci_stat_uint64_ptr but for ci_kbs_t statistic type
 \ingroup STAT
*/
CI_DECLARE_FUNC(ci_kbs_t *) ci_stat_kbs_ptr(int ID);

/*Low level structures and functions*/
typedef struct ci_stat_value {
    union {
        _CI_ATOMIC_TYPE uint64_t counter;
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
CI_DECLARE_FUNC(void) ci_stat_memblock_merge(ci_stat_memblock_t *to_block, const ci_stat_memblock_t *from_block, int history, int existing_instances);
CI_DECLARE_FUNC(void) ci_stat_memblock_reset(ci_stat_memblock_t *block);

CI_DECLARE_FUNC(ci_stat_memblock_t *) ci_stat_memblock_init(void *mem, size_t mem_size);

CI_DECLARE_FUNC(int) ci_stat_memblock_check(const ci_stat_memblock_t *block);

static inline uint64_t ci_stat_memblock_get_counter(const ci_stat_memblock_t *block, int id) {
    _CI_ASSERT(block);
    if (id < block->stats_count)
        return block->stats[id].counter;
    return 0;
}

static inline ci_kbs_t ci_stat_memblock_get_kbs(const ci_stat_memblock_t *block, int id) {
    _CI_ASSERT(block);
    if (id < block->stats_count)
        return block->stats[id].kbs;
    const ci_kbs_t zero = {0};
    return zero;
}

#define STAT_INT64_INC(memblock, id, count) ci_atomic_add_u64(&(memblock->stats[id].counter), count)
#define STAT_INT64_DEC(memblock, id, count) ci_atomic_sub_u64(&(memblock->stats[id].counter), count)
#define STAT_KBS_INC(memblock, id, count) ci_kbs_lock_and_update(&(memblock->stats[id].kbs), count)
/* do not use compound assignment to avoid atomic operation: */
#define STAT_INT64_INC_NL(memblock, id, count) (memblock->stats[id].counter = memblock->stats[id].counter + count)
#define STAT_INT64_DEC_NL(memblock, id, count) (memblock->stats[id].counter = memblock->stats[id].counter - count)
#define STAT_KBS_INC_NL(memblock, id, count) ci_kbs_update(&(memblock->stats[id].kbs), count)

#define STAT_INT64_NL(memblock, id) (memblock->stats[id].counter)
#define STAT_KBS_NL(memblock, id) (memblock->stats[id].kbs)

static inline void ci_stat_membock_uint64_inc(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    _CI_ASSERT(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_INC(mem_block, ID, count);
}

static inline void ci_stat_membock_uint64_dec(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    _CI_ASSERT(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_DEC(mem_block, ID, count);
}

static inline void ci_stat_memblock_kbs_inc(ci_stat_memblock_t *mem_block, int ID, uint64_t count)
{
    _CI_ASSERT(mem_block);
    if (ID < 0 || ID > mem_block->stats_count)
        return;
    STAT_INT64_INC(mem_block, ID, count);
}

#ifdef __cplusplus
}
#endif

#endif
