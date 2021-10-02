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
#include "debug.h"
#include "stats.h"

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

struct stat_group {
    char *name;
    int master_group_id;
};

struct stat_groups_list {
    struct stat_group *groups;
    int size;
    int entries_num;
};

struct stat_entry_list STAT_STATS = {NULL, 0, 0};
struct stat_groups_list STAT_GROUPS = {NULL, 0, 0};

struct stat_area {
    void (*release_mem)(void *);
    ci_stat_memblock_t *mem_block;
};
struct stat_area *STATS = NULL;

#define STEP 128

static struct stat_area * ci_stat_area_construct(void *mem_block, int size, void (*release_mem)(void *));
static void ci_stat_area_destroy(struct stat_area  *area);

int ci_stat_memblock_size(void)
{
    return _CI_ALIGN(sizeof(ci_stat_memblock_t)) + STAT_STATS.entries_num * sizeof(ci_stat_value_t);
}

ci_stat_memblock_t * ci_stat_memblock_get()
{
    return STATS ? STATS->mem_block : NULL;
}

int stat_entry_by_name(struct stat_entry_list *list, const char *label, int gid);

int stat_entry_add(struct stat_entry_list *list,const char *label, int type, int gid)
{
    struct stat_entry *l;
    int indx;

    if (!list)
        return -1;

    indx = stat_entry_by_name(list, label, gid);
    if (indx >= 0 )
        return (list->entries[indx].type == type) ? indx : -1;

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

int stat_entry_by_name(struct stat_entry_list *list, const char *label, int gid)
{
    int i;
    if (!list->entries)
        return -1;

    for (i = 0; i < list->entries_num; i++)
        if (strcmp(label, list->entries[i].label) == 0 && list->entries[i].gid == gid) return i;

    return -1;
}

int ci_stat_group_find(const char *group)
{
    int gid;
    for (gid = 0; gid < STAT_GROUPS.entries_num; gid++) {
        _CI_ASSERT(STAT_GROUPS.groups[gid].name);
        if (strcmp(STAT_GROUPS.groups[gid].name, group) == 0)
            return gid;
    }
    return -1;
}

static int group_add(const char *group, int mgid)
{
    int gid = ci_stat_group_find(group);
    if (gid >= 0) {
        if (mgid == CI_STAT_GROUP_MASTER && STAT_GROUPS.groups[gid].master_group_id != CI_STAT_GROUP_MASTER)
            return -1; /*The existing one is not a master group but we need to register a master one*/
        return gid;
    }

    if (STAT_GROUPS.size == STAT_GROUPS.entries_num) {
        struct stat_group *group_list;
        /* Also covers the case the STAT_GROUPS.size is 0 */
        group_list = (struct stat_group *)realloc(STAT_GROUPS.groups, (STAT_GROUPS.size + STEP) * sizeof(char *));
        if (!group_list)
            return -1;
        STAT_GROUPS.groups = group_list;
        STAT_GROUPS.size += STEP;
    }
    STAT_GROUPS.groups[STAT_GROUPS.entries_num].name = strdup(group);
    STAT_GROUPS.groups[STAT_GROUPS.entries_num].master_group_id = mgid;
    gid = STAT_GROUPS.entries_num;
    STAT_GROUPS.entries_num++;
    return gid;
}

int ci_stat_group_register(const char *group, const char *master_group)
{
    int mgid = CI_STAT_GROUP_NONE;

    if (!group)
        return -1;

    if (STATS) {
        /*The statistics area is built and finalized,
          we can not add new statistic items*/
        return -1;
    }

    if (master_group) {
        mgid = ci_stat_group_find(master_group);
        if (mgid < 0)
            return -1;

        if (STAT_GROUPS.groups[mgid].master_group_id != CI_STAT_GROUP_MASTER)
            return -1; /*The master group is not a master group*/
    }
    return group_add(group, mgid);
}

CI_DECLARE_FUNC(int) ci_stat_mastergroup_register(const char *group)
{
    return group_add(group, CI_STAT_GROUP_MASTER);
}

int ci_stat_group_add(const char *group)
{
    return ci_stat_group_register(group, NULL);
}

int ci_stat_entry_register(const char *label, ci_stat_type_t type, const char *group)
{
    int gid;

    if (STATS) {
        /*The statistics area is built and finalized,
          we can not add new statistic items*/
        return -1;
    }

    gid = ci_stat_group_add(group);
    if (gid < 0)
        return -1;

    if (type >= CI_STAT_INT64_T && type < CI_STAT_TYPE_END)
        return stat_entry_add(&STAT_STATS, label, type, gid);

    return -1;
}

int ci_stat_entry_find(const char *label, const char *group, ci_stat_type_t type)
{
    int i;
    int gid = ci_stat_group_find(group);
    if (gid < 0)
        return -1;
    for (i = 0; i < STAT_STATS.entries_num; i++)
        if (strcmp(label, STAT_STATS.entries[i].label) == 0 && STAT_STATS.entries[i].gid == gid && STAT_STATS.entries[i].type == type) return i;
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

void ci_stat_allocate_mem()
{
    size_t mem_size = ci_stat_memblock_size();
    void *mem = malloc(mem_size);
    _CI_ASSERT(mem);
    ci_stat_attach_mem(mem, mem_size, free);
}

void ci_stat_release()
{
    if (!STATS)
        return;
    ci_stat_area_destroy(STATS);
    STATS = NULL;
}

void ci_stat_uint64_inc(int ID, uint64_t count)
{
    if (!STATS || !STATS->mem_block)
        return;

    if (ID < 0 || ID > STATS->mem_block->stats_count)
        return;

    ci_atomic_add_u64(&(STATS->mem_block->stats[ID].counter), (uint64_t)count);
}

void ci_stat_kbs_inc(int ID, uint64_t count)
{
    if (!STATS || !STATS->mem_block)
        return;

    if (ID < 0 || ID > STATS->mem_block->stats_count)
        return;

    ci_kbs_lock_and_update(&(STATS->mem_block->stats[ID].kbs), count);
}

void ci_stat_update(const ci_stat_item_t *stats, int num)
{
    int i;
    if (!STATS || !STATS->mem_block)
        return;
    for (i = 0; i < num; ++i) {
        int id = stats[i].Id;
        if ( id < 0 || id > STATS->mem_block->stats_count)
            continue; /*May print a warning?*/
        switch (stats[i].type) {
        case CI_STAT_INT64_T:
            ci_atomic_add_u64(&(STATS->mem_block->stats[id].counter), (uint64_t)stats[i].count);
            break;
        case CI_STAT_KBS_T:
            ci_kbs_lock_and_update(&(STATS->mem_block->stats[id].kbs), stats[i].count);
            break;
        case CI_STAT_TIME_US_T:
        case CI_STAT_TIME_MS_T:
            /*Only can set a 32bit integer?*/
            STATS->mem_block->stats[id].counter = num;
            break;
        default:
            /*Wrong type id, ignore for now*/
            break;
        }
    }
}

uint64_t *ci_stat_uint64_ptr(int ID)
{
    if (!STATS || !STATS->mem_block)
        return NULL;

    if (ID >= 0 && ID < STATS->mem_block->stats_count)
        return &(STATS->mem_block->stats[ID].counter);

    return NULL;
}

ci_kbs_t * ci_stat_kbs_ptr(int ID)
{
    if (!STATS || !STATS->mem_block)
        return NULL;

    if (ID >= 0 && ID < STATS->mem_block->stats_count)
        return &(STATS->mem_block->stats[ID].kbs);

    return NULL;
}

void ci_stat_groups_iterate(void *data, int (*group_call)(void *data, const char *name, int groupId, int masterGroupId))
{
    int ret = 0;
    int gid;
    for (gid = 0; gid < STAT_GROUPS.entries_num && !ret; gid++) {
        ret = group_call(data, STAT_GROUPS.groups[gid].name, gid, STAT_GROUPS.groups[gid].master_group_id);
    }
}

static ci_stat_value_t stat_value_zero = {.kbs = {0}};

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

    area->mem_block = ci_stat_memblock_init(mem_block, size);
    area->release_mem = release_mem;
    return area;
}

void ci_stat_area_destroy(struct stat_area  *area)
{
    if (area->release_mem)
        area->release_mem(area->mem_block);
    free(area);
}

/*Make a memblock area from continues memory block*/
ci_stat_memblock_t * ci_stat_memblock_init(void *mem, size_t mem_size)
{
    ci_stat_memblock_t *mem_block = mem;

    if (mem_size < ci_stat_memblock_size())
        return NULL;

    mem_block->sig = MEMBLOCK_SIG;
    mem_block->stats_count =  STAT_STATS.entries_num;
    ci_stat_memblock_reset(mem_block);
    return mem_block;
}

int ci_stat_memblock_check(const ci_stat_memblock_t *block)
{
    return (block->sig == MEMBLOCK_SIG) && (block->stats_count <= STAT_STATS.entries_num);
}

void ci_stat_memblock_reset(ci_stat_memblock_t *block)
{
    memset(block->stats, 0, block->stats_count * sizeof(ci_stat_value_t));
}

void ci_stat_memblock_merge(ci_stat_memblock_t *to_block, const ci_stat_memblock_t *from_block, int history, int existing_instances)
{
    int i;
    if (!to_block || !from_block)
        return;

    /* After a reconfigure we may have more counters. */
    _CI_ASSERT(to_block->stats_count >= from_block->stats_count);
    _CI_ASSERT(to_block->stats_count == STAT_STATS.entries_num);
    _CI_ASSERT(to_block->sig == MEMBLOCK_SIG);
    _CI_ASSERT(from_block->sig == MEMBLOCK_SIG);

    for (i = 0; i < from_block->stats_count; i++) {
        switch (STAT_STATS.entries[i].type) {
        case CI_STAT_INT64_T:
            to_block->stats[i].counter += from_block->stats[i].counter;
            break;
        case CI_STAT_KBS_T:
            ci_kbs_add_to(&to_block->stats[i].kbs, &from_block->stats[i].kbs);
            break;
        case CI_STAT_TIME_US_T:
        case CI_STAT_TIME_MS_T:
            if (!history)
                to_block->stats[i].counter = (existing_instances * to_block->stats[i].counter + from_block->stats[i].counter) / (existing_instances + 1);
            break;
        default:
            /*print error?*/
            break;
        }
    }
}

