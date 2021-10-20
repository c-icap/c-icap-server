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
#include "stats.h"
#include <assert.h>

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

struct stat_entry_list STAT_STATS = {NULL, 0, 0};
struct stat_groups_list STAT_GROUPS = {NULL, 0, 0};;

struct stat_area {
    ci_thread_mutex_t mtx;
    void (*release_mem)(void *);
    struct stat_memblock *mem_block;
};
struct stat_area *STATS = NULL;

#define STEP 128

static struct stat_area * ci_stat_area_construct(void *mem_block, int size, void (*release_mem)(void *));
static void ci_stat_area_destroy(struct stat_area  *area);
#if 0
static void ci_stat_area_reset(struct stat_area *area);
#endif

int ci_stat_memblock_size(void)
{
    return _CI_ALIGN(sizeof(struct stat_memblock)) + STAT_STATS.entries_num * sizeof(ci_stat_value_t);
}

int stat_entry_by_name(struct stat_entry_list *list, const char *label);

int stat_entry_add(struct stat_entry_list *list,const char *label, int type, int gid)
{
    struct stat_entry *l;
    int indx;

    if (!list)
        return -1;

    indx = stat_entry_by_name(list, label);
    if (indx >= 0 )
        return indx;

    if (list->size == list->entries_num) {

        if (list->size == 0) {
            list->entries = malloc(STEP*sizeof(struct stat_entry));
            if (!list->entries)
                return -1;
        } else {
            l = realloc(list->entries, (list->size+STEP)*sizeof(struct stat_entry));
            if (!l)
                return -1;
            list->entries = l;
        }
        list->size += STEP;
    }
    list->entries[list->entries_num].label = strdup(label);
    list->entries[list->entries_num].type = type;
    list->entries[list->entries_num].gid = gid;
    indx = list->entries_num;
    list->entries_num++;
    return indx;
}

void stat_entry_release_list(struct stat_entry_list *list)
{
    int i;
    if (!list->entries)
        return;
    for (i = 0; i < list->entries_num; i++)
        free(list->entries[i].label);
    free(list->entries);
    list->entries = NULL;
    list->size = 0;
    list->entries_num = 0;
}

int stat_entry_by_name(struct stat_entry_list *list, const char *label)
{
    int i;
    if (!list->entries)
        return -1;

    for (i = 0; i < list->entries_num; i++)
        if (strcmp(label, list->entries[i].label) == 0) return i;

    return -1;
}

int stat_group_add(const char *group)
{
    char **group_list;
    int gid = 0;

    for (gid = 0; gid < STAT_GROUPS.entries_num; gid++) {
        if (strcmp(STAT_GROUPS.groups[gid], group) == 0)
            return gid;
    }

    if (STAT_GROUPS.size == 0) {
        STAT_GROUPS.groups = malloc(STEP * sizeof(char *));
        if (!STAT_GROUPS.groups)
            return -1;
        STAT_GROUPS.size = STEP;
    } else if (STAT_GROUPS.size == STAT_GROUPS.entries_num) {
        group_list = realloc(STAT_GROUPS.groups, (STAT_GROUPS.size+STEP)*sizeof(char *));
        if (!group_list)
            return -1;
        STAT_GROUPS.groups = group_list;
        STAT_GROUPS.size += STEP;
    }
    STAT_GROUPS.groups[STAT_GROUPS.entries_num] = strdup(group);
    gid = STAT_GROUPS.entries_num;
    STAT_GROUPS.entries_num++;
    return gid;
}

int ci_stat_entry_register(const char *label, ci_stat_type_t type, const char *group)
{
    int gid;

    gid = stat_group_add(group);
    if (gid < 0)
        return -1;

    if (type == CI_STAT_INT64_T || type == CI_STAT_KBS_T)
        return stat_entry_add(&STAT_STATS, label, type, gid);

    return -1;
}

void ci_stat_entry_release_lists()
{
    stat_entry_release_list(&STAT_STATS);
}

int ci_stat_attach_mem(void *mem_block, int size, void (*release_mem)(void *))
{
    if (STATS)
        return 1;

    STATS = ci_stat_area_construct(mem_block, size, release_mem);
    return (STATS != NULL);
}

void ci_stat_release()
{
    if (!STATS)
        return;
    ci_stat_area_destroy(STATS);
    STATS = NULL;
}

static inline void do_update_uint64(int ID, int count)
{
    if (ID >= 0 && ID < STATS->mem_block->stats_count)
        STATS->mem_block->stats[ID].counter += count;
}

void ci_stat_uint64_inc(int ID, int count)
{
    if (!STATS || !STATS->mem_block)
        return;
    ci_thread_mutex_lock(&STATS->mtx);
    do_update_uint64(ID, count);
    ci_thread_mutex_unlock(&STATS->mtx);
}

static inline void do_update_kbs(int ID, int count)
{
    if (ID >= 0 && ID < STATS->mem_block->stats_count) {
        STATS->mem_block->stats[ID].kbs.bytes += count;
        STATS->mem_block->stats[ID].kbs.kb += (STATS->mem_block->stats[ID].kbs.bytes >> 10);
        STATS->mem_block->stats[ID].kbs.bytes &= 0x3FF;
    }
}

void ci_stat_kbs_inc(int ID, int count)
{
    if (!STATS || !STATS->mem_block)
        return;

    ci_thread_mutex_lock(&STATS->mtx);
    do_update_kbs(ID, count);
    ci_thread_mutex_unlock(&STATS->mtx);
}

void ci_stat_update(const ci_stat_item_t *stats, int count)
{
    int i;
    if (!STATS || !STATS->mem_block)
        return;
    ci_thread_mutex_lock(&STATS->mtx);
    for (i = 0; i < count; ++i) {
        switch (stats[i].type) {
        case CI_STAT_INT64_T:
            do_update_uint64(stats[i].Id, stats[i].count);
            break;
        case CI_STAT_KBS_T:
            do_update_kbs(stats[i].Id, stats[i].count);
            break;
        default:
            /*Wrong type id, ignore for now*/
            break;
        }
    }
    ci_thread_mutex_unlock(&STATS->mtx);
}

uint64_t *ci_stat_uint64_ptr(int ID)
{
    if (!STATS || !STATS->mem_block)
        return NULL;

    if (ID >= 0 && ID < STATS->mem_block->stats_count)
        return &(STATS->mem_block->stats[ID].counter);

    return NULL;
}

void ci_stat_groups_iterate(void *data, int (*group_call)(void *data, const char *name, int groupId))
{
    int ret = 0;
    int gid;
    for (gid = 0; gid < STAT_GROUPS.entries_num && !ret; gid++) {
        ret = group_call(data, STAT_GROUPS.groups[gid], gid);
    }
}

static ci_stat_value_t stat_value_zero = {.kbs = {0, 0}};

void ci_stat_statistics_iterate(void *data, int groupId, int (*stat_call)(void *data, const char *label, int ID, int gId, const ci_stat_t *stat))
{
    int ret = 0;
    int sid;
    for (sid = 0; sid < STAT_STATS.entries_num && !ret; sid++) {
        if (groupId < 0 || groupId == STAT_STATS.entries[sid].gid) {
            ci_stat_t stat = {
                .type = STAT_STATS.entries[sid].type,
                .value = (STATS && STATS->mem_block ? STATS->mem_block->stats[sid] : stat_value_zero)
            };
            ret = stat_call(data, STAT_STATS.entries[sid].label, sid, STAT_STATS.entries[sid].gid, &stat);
        }
    }
}

/***********************************************
   Low level functions
*/
struct stat_area *ci_stat_area_construct(void *mem_block, int size, void (*release_mem)(void *))
{
    struct stat_area  *area = NULL;
    if (size < ci_stat_memblock_size() )
        return NULL;

    area = malloc(sizeof(struct stat_area));
    if (!area)
        return NULL;

    ci_thread_mutex_init(&(area->mtx));
    area->mem_block = ci_stat_memblock_init(mem_block, size);
    area->release_mem = release_mem;
    return area;
}

#if 0
void ci_stat_area_reset(struct stat_area *area)
{
    ci_thread_mutex_lock(&(area->mtx));
    ci_stat_memblock_reset(area->mem_block);
    ci_thread_mutex_unlock(&(area->mtx));
}
#endif


void ci_stat_area_destroy(struct stat_area  *area)
{
    ci_thread_mutex_destroy(&(area->mtx));
    if (area->release_mem)
        area->release_mem(area->mem_block);
    free(area);
}

/*Does not realy needed*/
void ci_stat_area_uint64_inc(struct stat_area *area,int ID, int count)
{
    if (!area->mem_block)
        return;
    if (ID < 0 || ID >= area->mem_block->stats_count)
        return;
    ci_thread_mutex_lock(&area->mtx);
    area->mem_block->stats[ID].counter += count;
    ci_thread_mutex_unlock(&area->mtx);
}

/*Does not realy needed*/
void ci_stat_area_kbs_inc(struct stat_area *area,int ID, int count)
{
    if (!area->mem_block)
        return;

    if (ID < 0 || ID >= area->mem_block->stats_count)
        return;

    ci_thread_mutex_lock(&area->mtx);
    area->mem_block->stats[ID].kbs.bytes += count;
    area->mem_block->stats[ID].kbs.kb += (area->mem_block->stats[ID].kbs.bytes >> 10);
    area->mem_block->stats[ID].kbs.bytes &= 0x3FF;
    ci_thread_mutex_unlock(&area->mtx);
}

/*Make a memblock area from continues memory block*/
struct stat_memblock * ci_stat_memblock_init(void *mem, size_t mem_size)
{
    struct stat_memblock *mem_block = mem;

    if (mem_size < ci_stat_memblock_size())
        return NULL;

    mem_block->sig = MEMBLOCK_SIG;
    mem_block->stats_count =  STAT_STATS.entries_num;
    ci_stat_memblock_reset(mem_block);
    return mem_block;
}

int ci_stat_memblock_check(const struct stat_memblock *block)
{
    return (block->sig == MEMBLOCK_SIG) && (block->stats_count <= STAT_STATS.entries_num);
}

void ci_stat_memblock_reset(struct stat_memblock *block)
{
    memset(block->stats, 0, block->stats_count * sizeof(ci_stat_value_t));
}

void ci_stat_memblock_merge(struct stat_memblock *dest_block, struct stat_memblock *stats)
{
    int i;
    if (!dest_block || !stats)
        return;

    /* After a reconfigure we may have more counters. */
    assert(dest_block->stats_count >= stats->stats_count);
    assert(dest_block->stats_count == STAT_STATS.entries_num);
    assert(dest_block->sig == MEMBLOCK_SIG);
    assert(stats->sig == MEMBLOCK_SIG);

    for (i = 0; i < stats->stats_count; i++) {
        switch (STAT_STATS.entries[i].type) {
        case CI_STAT_INT64_T:
            dest_block->stats[i].counter += stats->stats[i].counter;
            break;
        case CI_STAT_KBS_T:
            dest_block->stats[i].kbs.kb += stats->stats[i].kbs.kb;
            dest_block->stats[i].kbs.bytes += stats->stats[i].kbs.bytes;
            dest_block->stats[i].kbs.kb += (stats->stats[i].kbs.bytes >> 10);
            dest_block->stats[i].kbs.bytes &= 0x3FF;
            break;
        default:
            /*print error?*/
            break;
        }
    }
}

