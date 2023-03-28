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
#include "array.h"
#include "debug.h"
#include "stats.h"
#include <math.h>

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
    size_t mem_block_size;
    void *histos;
    size_t histos_size;
};
struct stat_area *STATS = NULL;

#define STEP 128

static struct stat_area * ci_stat_area_construct(void *mem_block, size_t size, void *histos, size_t histos_size, void (*release_mem)(void *));
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

ci_stat_type_t ci_stat_entry_type_by_id(int id)
{
    _CI_ASSERT(id <  STAT_STATS.entries_num);
    return STAT_STATS.entries[id].type;
}

void ci_stat_entry_release_lists()
{
    stat_entry_release_list(&STAT_STATS);
}

int ci_stat_attach_mem(void *mem_block, size_t size, void *histos_mem, size_t histos_size, void (*release_mem)(void *))
{
    if (STATS)
        return 1;

    STATS = ci_stat_area_construct(mem_block, size, histos_mem, histos_size, release_mem);
    return (STATS != NULL);
}

void ci_stat_allocate_mem()
{
    size_t mem_size = ci_stat_memblock_size();
    void *mem = malloc(mem_size);
    _CI_ASSERT(mem);
    size_t histo_size = ci_stat_histo_mem_size();
    void *histo_mem = histo_size > 0 ? malloc(histo_size) : NULL;
    _CI_ASSERT(ci_stat_histo_mem_initialize(histo_mem, histo_size));
    ci_stat_attach_mem(mem, mem_size, histo_mem, histo_size, free);
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

void ci_stat_uint64_dec(int ID, uint64_t count)
{
    if (!STATS || !STATS->mem_block)
        return;

    if (ID < 0 || ID > STATS->mem_block->stats_count)
        return;

    ci_atomic_sub_u64(&(STATS->mem_block->stats[ID].counter), (uint64_t)count);
}

void ci_stat_kbs_inc(int ID, uint64_t count)
{
    if (!STATS || !STATS->mem_block)
        return;

    if (ID < 0 || ID > STATS->mem_block->stats_count)
        return;

    ci_kbs_lock_and_update(&(STATS->mem_block->stats[ID].kbs), count);
}

void ci_stat_value_set(int ID, uint64_t value)
{
    if (!STATS || !STATS->mem_block)
        return;

    if (ID < 0 || ID > STATS->mem_block->stats_count)
        return;

    /*This is also can set kbs statistic type*/
    ci_atomic_store_u64(&(STATS->mem_block->stats[ID].counter), value);
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
            if (stats[i].count < 0)
                ci_atomic_sub_u64(&(STATS->mem_block->stats[id].counter), (uint64_t)(-stats[i].count));
            else
                ci_atomic_add_u64(&(STATS->mem_block->stats[id].counter), (uint64_t)stats[i].count);
            break;
        case CI_STAT_KBS_T:
            ci_kbs_lock_and_update(&(STATS->mem_block->stats[id].kbs), stats[i].count);
            break;
        case CI_STAT_TIME_US_T:
        case CI_STAT_TIME_MS_T:
        case CI_STAT_INT64_MEAN_T:
            STATS->mem_block->stats[id].counter = stats[i].value;
            break;
        default:
            /*Wrong type id, ignore for now*/
            break;
        }
    }
}

_CI_ATOMIC_TYPE uint64_t *ci_stat_uint64_ptr(int ID)
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

uint64_t ci_stat_uint64_get(int ID)
{
    uint64_t value;
    if (!STATS || !STATS->mem_block)
        return 0;

    if (ID >= 0 && ID < STATS->mem_block->stats_count)
        ci_atomic_load_u64(&STATS->mem_block->stats[ID].counter, &value);
    else
        value = 0;
    return value;
}

ci_kbs_t ci_stat_kbs_get(int ID)
{
    static ci_kbs_t zero = {.bytes = 0};
    ci_kbs_t value;
    if (!STATS || !STATS->mem_block) {
        return zero;
    }

    if (ID >= 0 && ID < STATS->mem_block->stats_count) {
        uint64_t uint_val;
        ci_atomic_load_u64(&STATS->mem_block->stats[ID].kbs.bytes, &uint_val);
        value.bytes = uint_val;
    } else
        value.bytes = 0;
    return value;
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
struct stat_area *ci_stat_area_construct(void *mem_block, size_t size, void *histo, size_t histo_size, void (*release_mem)(void *))
{
    struct stat_area  *area = NULL;
    if (size < ci_stat_memblock_size() )
        return NULL;

    if (histo_size < ci_stat_histo_mem_size())
        return NULL;

    area = malloc(sizeof(struct stat_area));
    if (!area)
        return NULL;

    area->mem_block = ci_stat_memblock_init(mem_block, size);
    area->mem_block_size = size;
    area->histos = histo;
    area->histos_size = histo_size;
    area->release_mem = release_mem;
    return area;
}

void ci_stat_area_destroy(struct stat_area  *area)
{
    if (area->release_mem) {
        area->release_mem(area->mem_block);
        if (area->histos)
            area->release_mem(area->histos);
    }
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
        case CI_STAT_INT64_MEAN_T:
            if (!history)
                to_block->stats[i].counter = (existing_instances * to_block->stats[i].counter + from_block->stats[i].counter) / (existing_instances + 1);
            break;
        default:
            /*print error?*/
            break;
        }
    }
}

typedef enum ci_histo_type {
    CI_HISTO_DEFAULT = 0,
    CI_HISTO_LINEAR = CI_HISTO_DEFAULT,
    CI_HISTO_LOG,
    CI_HISTO_ENUM,
    CI_HISTO_CUSTOM_BINS,
    CI_HISTO_END
} ci_histo_type_t;

typedef struct ci_stat_histo_spec {
    unsigned int id;
    char data_descr[64];
    int items;
    ci_histo_type_t histo_type;
    ci_stat_type_t data_type;
    uint64_t min;
    uint64_t max;
    size_t size;
    double step;
    unsigned int flags;
    //TODO: recheck if labels and custom_bins attached correctly to
    //      shared memory segments and used correctly.
    const char **labels;
    const uint64_t *custom_bins;
} ci_stat_histo_spec_t;

#define CI_HISTO_SIG 0xEAEA
typedef struct ci_stat_histogram {
    unsigned int sig;
    ci_stat_histo_spec_t header;
    _CI_ATOMIC_TYPE uint64_t other;
    _CI_ATOMIC_TYPE uint64_t bins[];
} ci_stat_histogram_t;

static ci_dyn_array_t *Histos = NULL;
static int HistoCount = 0;
static size_t HistoPos = 0;

size_t ci_stat_histo_mem_size()
{
    return HistoPos;
}

int ci_stat_histo_register(const char *label, const char *data_descr, int items, ci_histo_type_t type, uint64_t min, uint64_t max)
{
    if (!Histos)
        Histos = ci_dyn_array_new2(64, sizeof(ci_stat_histo_spec_t));
    unsigned int id = (unsigned int)HistoPos;
    if (items > (max - min))
        items = max - min;
    double step;
    if (type == CI_HISTO_LOG)
        step = ((double) (items - 1) / log((double) (max - min)));
    else {
        step = ((double) (items - 1) / (double) (max - min));
        _CI_ASSERT(step <= 1);
    }
    size_t size = sizeof(ci_stat_histogram_t) + items * sizeof(ci_stat_value_t);
    ci_stat_histo_spec_t histo = {id: id,
                                        data_descr: "\0",
                                        items: items,
                                        histo_type: type,
                                        data_type: CI_STAT_INT64_T,
                                        min: min,
                                        max: max,
                                        size: size,
                                        step: step,
                                        flags: 0,
                                        labels: NULL,
                                        custom_bins: NULL
    };
    snprintf(histo.data_descr, sizeof(histo.data_descr), "%s", data_descr);
    _CI_ASSERT(ci_dyn_array_add(Histos, label, &histo, sizeof(ci_stat_histo_spec_t)));
    HistoCount++;
    HistoPos += size;
    return id;
}

int ci_stat_histo_mem_initialize(void *histos, size_t size)
{
    if (size < ci_stat_histo_mem_size())
        return 0;
    if (!Histos)
        return 1; /*Nothing to do*/
    int i;
    for (i = 0; i < ci_dyn_array_size(Histos); i++) {
        const ci_stat_histo_spec_t *spec = ci_dyn_array_value(Histos, i);
        _CI_ASSERT(spec->id < (unsigned int)size);
        ci_stat_histogram_t *histo = (ci_stat_histogram_t *)(histos + spec->id);
        histo->sig = CI_HISTO_SIG;
        memcpy(&histo->header, spec, sizeof(ci_stat_histo_spec_t));
        memset(histo->bins, 0, spec->items * sizeof(ci_stat_value_t));
    }
    return 1;
}

static ci_stat_histogram_t *ci_stat_histo_get_histo(int id)
{
    if (!STATS || !STATS->histos)
        return NULL;
    if (id >= STATS->histos_size)
        return NULL; /* assert or at least warn? */
    ci_stat_histogram_t *histo = (ci_stat_histogram_t *)(STATS->histos + id);
    _CI_ASSERT(histo->sig == CI_HISTO_SIG);
    return histo;
}

void ci_stat_histo_set_flag(int id, unsigned flags)
{
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (histo)
        histo->header.flags |= flags;
}

void ci_stat_histo_clear_flag(int id, unsigned flags)
{
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (histo)
        histo->header.flags &= ~flags;
}

const char * ci_stat_histo_data_descr(int id)
{
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (histo)
        return histo->header.data_descr;
    return "-";
}

static inline double fix_bin(double value)
{
    // currently we are supporting only integer data types
    return trunc(value);
}

/*
  This is builds a simple c-icap histogram
*/
int ci_stat_histo_create(const char *label, const char *data_descr, int items, uint64_t min, uint64_t max)
{
    return ci_stat_histo_register(label, data_descr, items, CI_HISTO_DEFAULT, min, max);
}

static void ci_stat_histo_update_linear(ci_stat_histogram_t *histo, uint64_t value)
{
    if (value <= histo->header.min)
        ci_atomic_add_u64(&histo->bins[0], 1);
    else {
        value -= histo->header.min;
        const unsigned int bin = (unsigned int)floor((histo->header.step * (double)value) + 1.0);
        if (bin >= histo->header.items) {
            ci_atomic_add_u64(&histo->other, 1);
        } else
            ci_atomic_add_u64(&histo->bins[bin], 1);
    }
}

double ci_stat_histo_get_bin_value_linear(ci_stat_histogram_t *histo, unsigned pos)
{
    double value = histo->header.min + ((double)pos) / histo->header.step;
    value = fix_bin(value);
    if (value > (double)(histo->header.max))
        return (double) -1;
    return value;
}

char *ci_stat_histo_get_bin_label_linear(ci_stat_histogram_t *histo, unsigned pos, char *buf, size_t buf_size, double *bin)
{
    *bin = ci_stat_histo_get_bin_value_linear(histo, pos);
    *bin = fix_bin(*bin);
    if (*bin >= 0)
        snprintf(buf, buf_size, "%.0f", *bin);
    else
        snprintf(buf, buf_size, "Infinity");
    return buf;
}

/*Log based histograms*/
int ci_stat_histo_create_log(const char *label, const char *data_descr, int items, uint64_t min, uint64_t max)
{
    return ci_stat_histo_register(label, data_descr, items, CI_HISTO_LOG, min, max);
}

static void ci_stat_histo_update_log(ci_stat_histogram_t *histo, uint64_t value)
{
    if (value <= histo->header.min)
        ci_atomic_add_u64(&histo->bins[0], 1);
    else {
        value -= histo->header.min;
        const unsigned int bin = (unsigned int)floor((histo->header.step * log((double)value)) + 1.0);
        if (bin >= histo->header.items) {
            ci_atomic_add_u64(&histo->other, 1);
        } else
            ci_atomic_add_u64(&histo->bins[bin], 1);
    }
}

double ci_stat_histo_get_bin_value_log(ci_stat_histogram_t *histo, unsigned pos)
{
    double value = histo->header.min + exp((double)(pos) / histo->header.step);
    value = fix_bin(value);
    if (value > (double)(histo->header.max))
        return (double) -1;
    return value;
}

char *ci_stat_histo_get_bin_label_log(ci_stat_histogram_t *histo, unsigned pos, char *buf, size_t buf_size, double *bin)
{
    *bin = ci_stat_histo_get_bin_value_log(histo, pos);
    *bin = fix_bin(*bin);
    if (*bin >= 0)
        snprintf(buf, buf_size, "%.0f", *bin);
    else
        snprintf(buf, buf_size, "Infinity");
    return buf;
}

/*Enum values based histogram*/
int ci_stat_histo_create_enum(const char *label, const char *data_descr, const char **labels, int items)
{
    int id = ci_stat_histo_register(label, data_descr, items, CI_HISTO_ENUM, 0, items);
    if (id < 0)
        return id;
#if 0
    /* Faster ... */
    const ci_array_item_t *ai = ci_dyn_array_get_item(Histos, HistoCount - 1);
    _CI_ASSERT(ai);
    ci_stat_histo_spec_t *histo = ai->value;
#else
    /* ... but we do not care for speed. */
    ci_stat_histo_spec_t *histo = (ci_stat_histo_spec_t *)ci_dyn_array_search(Histos, label);
#endif
    _CI_ASSERT(histo);
    histo->labels = labels;
    return id;
}

static void ci_stat_histo_update_enum(ci_stat_histogram_t *histo, uint64_t value)
{
    if (value >= histo->header.max)
        ci_atomic_add_u64(&histo->other, 1);
    else
        ci_atomic_add_u64(&histo->bins[value], 1);
}

double ci_stat_histo_get_bin_value_enum(ci_stat_histogram_t *histo, unsigned pos)
{
    if (pos >= (double)(histo->header.max))
        return (double) -1;
    return (double)pos;
}

char *ci_stat_histo_get_bin_label_enum(ci_stat_histogram_t *histo, unsigned pos, char *buf, size_t buf_size, double *bin)
{
    if (pos < (double)(histo->header.max)) {
        const char *label = histo->header.labels[pos];
        if (label) {
            *bin = pos;
            snprintf(buf, buf_size, "%s", label);
            return buf;
        }
    }
    *bin = -1;
    snprintf(buf, buf_size, "Other");
    return buf;
}

/*Custom bins histograms*/
int ci_stat_histo_create_custom_bins(const char *label, const char *data_descr, const uint64_t *bins_array, int bins_number)
{
    const uint64_t min = bins_array[0]; /*Should be configured?*/
    const uint64_t max = bins_array[bins_number -1];
    int i;
    /* Check array order */
    for (i = 1; i < bins_number; ++i)
        _CI_ASSERT((bins_array[i] > bins_array[i -1]) && "custom array bins is not ordered");

    int id = ci_stat_histo_register(label, data_descr, bins_number, CI_HISTO_CUSTOM_BINS, min, max);
    if (id < 0)
        return id;
    ci_stat_histo_spec_t *histo = (ci_stat_histo_spec_t *)ci_dyn_array_search(Histos, label);
    histo->custom_bins = bins_array;
    return id;
}

static void ci_stat_histo_update_custom_bins(ci_stat_histogram_t *histo, uint64_t value)
{
    if (value >= histo->header.max)
        ci_atomic_add_u64(&histo->other, 1);
    else{
        int i;
        for(i = 0; i < histo->header.items; ++i) {
            if (histo->header.custom_bins[i] >= value)
                break;
        }
        /* _CI_ASSERT(i <=  histo->header.items); */
        ci_atomic_add_u64(&histo->bins[i], 1);
    }
}

double ci_stat_histo_get_bin_value_custom_bins(ci_stat_histogram_t *histo, unsigned pos)
{
    if (pos >= (double)(histo->header.max))
        return (double) -1;
    return (double)histo->header.custom_bins[pos];
}

char *ci_stat_histo_get_bin_label_custom_bins(ci_stat_histogram_t *histo, unsigned pos, char *buf, size_t buf_size, double *bin)
{
    *bin = ci_stat_histo_get_bin_value_custom_bins(histo, pos);
    if (*bin >= 0) {
        snprintf(buf, buf_size, "%.0f", *bin);
    } else
        snprintf(buf, buf_size, "Infinity");
    return buf;
}

struct _histo_operator {
    ci_histo_type_t type;
    void (*update)(ci_stat_histogram_t *histo, uint64_t value);
    double (*get_bin_value)(ci_stat_histogram_t *histo, unsigned pos);
    char *(*get_bin_label)(ci_stat_histogram_t *histo, unsigned pos, char *buf, size_t buf_size, double *bin);
} ci_histo_operators[] = {
    {CI_HISTO_LINEAR, ci_stat_histo_update_linear, ci_stat_histo_get_bin_value_linear, ci_stat_histo_get_bin_label_linear},
    {CI_HISTO_LOG, ci_stat_histo_update_log, ci_stat_histo_get_bin_value_log, ci_stat_histo_get_bin_label_log},
    {CI_HISTO_ENUM, ci_stat_histo_update_enum, ci_stat_histo_get_bin_value_enum, ci_stat_histo_get_bin_label_enum},
    {CI_HISTO_CUSTOM_BINS,ci_stat_histo_update_custom_bins, ci_stat_histo_get_bin_value_custom_bins, ci_stat_histo_get_bin_label_custom_bins}
};

void ci_stat_histo_update(int id, uint64_t value)
{
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (histo) {
        _CI_ASSERT(histo->header.histo_type >= CI_HISTO_DEFAULT && histo->header.histo_type < CI_HISTO_END);
        ci_histo_operators[histo->header.histo_type].update(histo, value);
    }
}

void ci_stat_histo_iterate(void *data, int (*fn)(void *data, const char *name, int id))
{
    int i;
    if (!Histos)
        return;
    for (i = 0; i < ci_dyn_array_size(Histos); i++) {
        const ci_array_item_t *ai = ci_dyn_array_get_item(Histos, i);
        const char *name = ai->name;
        const ci_stat_histo_spec_t *spec = ai->value;
        if (fn(data, name, spec->id))
            return; /*abort*/
    }
}

void ci_stat_histo_raw_bins_iterate(int id, void *data, void (*fn)(void *data, double bin_raw, uint64_t count))
{
    int i;
    double lastbin = -1;
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (!histo)
        return;
    for(i = 0; i < histo->header.items; i++) {
        double bin = ci_histo_operators[histo->header.histo_type].get_bin_value(histo, i);
        uint64_t count = histo->bins[i];
        if (i > 0 && lastbin == bin) {
            _CI_ASSERT(count == 0); /*Else we have to merge with the lastbin*/
            continue;
        }
        if (histo->header.histo_type == CI_HISTO_ENUM && histo->header.flags & CI_HISTO_IGNORE_COUNT_ZERO && count == 0)
            continue;
        fn(data, bin, count);
        lastbin = bin;
    }
    fn(data, (double)-1, histo->other);
}


void ci_stat_histo_bins_iterate(int id, void *data, void (*fn)(void *data, const char *bin_label, uint64_t count))
{
    char buf[128];
    int i;
    double lastbin = -1, raw_bin = -1;
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (!histo)
        return;
    for(i = 0; i < histo->header.items; i++) {
        const char *bin = ci_histo_operators[histo->header.histo_type].get_bin_label(histo, i, buf, sizeof(buf), &raw_bin);
        uint64_t count = histo->bins[i];
        if (i > 0 && raw_bin == lastbin) {
            _CI_ASSERT(count == 0); /*Else we have to merge with the lastbin*/
            continue;
        }
        if (histo->header.histo_type == CI_HISTO_ENUM && histo->header.flags & CI_HISTO_IGNORE_COUNT_ZERO && count == 0)
            continue;
        fn(data, bin, count);
        lastbin = raw_bin;
    }
    const char *other_bin = ci_histo_operators[histo->header.histo_type].get_bin_label(histo, (histo->header.max + 1), buf, sizeof(buf), &raw_bin);
    fn(data, other_bin, histo->other);
}

int ci_stat_histo_get_id(const char *name)
{
    if (!Histos)
        return -1;
    const ci_stat_histo_spec_t *spec = ci_dyn_array_search(Histos, name);
    if (spec)
        return spec->id;
    return -1;
}

CI_DECLARE_FUNC(int) ci_stat_histo_bins_number(int id)
{
    ci_stat_histogram_t *histo = ci_stat_histo_get_histo(id);
    if (!histo)
        return 0;
    return histo->header.items;
}
