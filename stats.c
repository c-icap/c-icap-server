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

struct stat_entry_list STAT_INT64 = {NULL, 0, 0};
struct stat_entry_list STAT_KBS = {NULL, 0, 0};
struct stat_groups_list STAT_GROUPS = {NULL, 0, 0};;

struct stat_area *STATS = NULL;

#define STEP 128

int ci_stat_memblock_size(void)
{
   return sizeof(struct stat_memblock)+STAT_INT64.entries_num*sizeof(uint64_t)+STAT_KBS.entries_num*sizeof(kbs_t);
}

int stat_entry_by_name(struct stat_entry_list *list, const char *label);

int stat_entry_add(struct stat_entry_list *list,const char *label, int type, int gid)
{
   struct stat_entry *l;
   int indx;

   if (!list)
       return -1;

   indx = stat_entry_by_name(list, label);
   if (indx >=0 )
       return indx;

   if (list->size == list->entries_num) {
 
       if (list->size==0) {
           list->entries = malloc(STEP*sizeof(struct stat_entry));
       }
       else {
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
   for (i=0;i<list->entries_num;i++)
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

    for (i=0; i<list->entries_num; i++)
        if (strcmp(label, list->entries[i].label) == 0) return i;

    return -1;
}

int stat_group_add(char *group)
{
   char **group_list;
   int gid =0;
    
   for (gid = 0; gid < STAT_GROUPS.entries_num; gid++) {
       if (strcmp(STAT_GROUPS.groups[gid], group) == 0) 
           return gid;
   }

   if (STAT_GROUPS.size == 0) {
       STAT_GROUPS.groups = malloc(STEP * sizeof(char *));
       STAT_GROUPS.size = STEP;
   }
   else if (STAT_GROUPS.size == STAT_GROUPS.entries_num) {
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

int ci_stat_entry_register(char *label, int type, char *group) 
{
   int gid;

   gid = stat_group_add(group);
   if (gid < 0)
       return -1;

   if (type == STAT_INT64_T) {
       return stat_entry_add(&STAT_INT64, label, type, gid);
   } 
   else if(type == STAT_KBS_T) {
       return stat_entry_add(&STAT_KBS, label, type, gid); 
   }
   return -1;
}

void ci_stat_entry_release_lists()
{
    stat_entry_release_list(&STAT_INT64);
    stat_entry_release_list(&STAT_KBS);
}

void ci_stat_attach_mem(void *mem_block,int size,void (*release_mem)(void *))
{
    if (STATS)
       return;

    STATS = ci_stat_area_construct(mem_block, size, release_mem);
}

void ci_stat_release()
{
    if (!STATS)
        return;
    ci_stat_area_destroy(STATS);
    STATS = NULL;
}

void ci_stat_uint64_inc(int ID, int count)
{
   if (!STATS || !STATS->mem_block)
       return;
    if (ID<0 || ID>=STATS->mem_block->counters64_size)
        return;
    ci_thread_mutex_lock(&STATS->mtx);
    STATS->mem_block->counters64[ID] += count;
    ci_thread_mutex_unlock(&STATS->mtx);
}

void ci_stat_kbs_inc(int ID, int count)
{
   if (!STATS->mem_block)
       return;

   if (ID<0 || ID>=STATS->mem_block->counterskbs_size)
        return;

    ci_thread_mutex_lock(&STATS->mtx);
    STATS->mem_block->counterskbs[ID].bytes += count;
    STATS->mem_block->counterskbs[ID].kb += (STATS->mem_block->counterskbs[ID].bytes >> 10);
    STATS->mem_block->counterskbs[ID].bytes &= 0x3FF;
    ci_thread_mutex_unlock(&STATS->mtx);
}


/***********************************************
   Low level functions
*/
struct stat_area *ci_stat_area_construct(void *mem_block, int size, void (*release_mem)(void *))
{
     struct stat_area  *area = malloc(sizeof(struct stat_area));
     if (size < ci_stat_memblock_size() )
         return NULL;
     
     assert(((struct stat_memblock *)mem_block)->sig == MEMBLOCK_SIG);

     ci_thread_mutex_init(&(area->mtx));
     area->mem_block = mem_block;
     area->release_mem = release_mem;
     area->mem_block->counters64 = mem_block + sizeof(struct stat_memblock);
     area->mem_block->counterskbs = mem_block + sizeof(struct stat_memblock) + STAT_INT64.entries_num*sizeof(uint64_t);
     area->mem_block->counters64_size =  STAT_INT64.entries_num;
     area->mem_block->counterskbs_size = STAT_KBS.entries_num;
     ci_stat_area_reset(area);
     return area;
}

void ci_stat_area_reset(struct stat_area *area)
{
     int i;

     ci_thread_mutex_lock(&(area->mtx));
     for (i=0; i<area->mem_block->counters64_size; i++)
          area->mem_block->counters64[i] = 0;
     for (i=0; i<area->mem_block->counterskbs_size; i++) {
          area->mem_block->counterskbs[i].kb = 0;
          area->mem_block->counterskbs[i].bytes = 0;
     }
     ci_thread_mutex_unlock(&(area->mtx));
}
 

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
    if (ID<0 || ID>=area->mem_block->counters64_size)
        return;
    ci_thread_mutex_lock(&area->mtx);
    area->mem_block->counters64[ID] += count;
    ci_thread_mutex_unlock(&area->mtx);
}

/*Does not realy needed*/
void ci_stat_area_kbs_inc(struct stat_area *area,int ID, int count)
{
   if (!area->mem_block)
       return;

   if (ID<0 || ID>=area->mem_block->counterskbs_size)
        return;

    ci_thread_mutex_lock(&area->mtx);
    area->mem_block->counterskbs[ID].bytes += count;
    area->mem_block->counterskbs[ID].kb += (area->mem_block->counterskbs[ID].bytes >> 10);
    area->mem_block->counterskbs[ID].bytes &= 0x3FF;
    ci_thread_mutex_unlock(&area->mtx);
}

/*Make a memblock area from continues memory block*/
void stat_memblock_fix(struct stat_memblock *mem_block)
{
     assert(mem_block->sig == MEMBLOCK_SIG);
     mem_block->counters64_size =  STAT_INT64.entries_num;
     mem_block->counterskbs_size = STAT_KBS.entries_num;
     mem_block->counters64 = (void *)mem_block + sizeof(struct stat_memblock);
     mem_block->counterskbs = (void *)mem_block + sizeof(struct stat_memblock)
                              + mem_block->counters64_size*sizeof(uint64_t);
}

/*Reconstruct a memblock which is located to a continues memory block*/
void stat_memblock_reconstruct(struct stat_memblock *mem_block)
{
     assert(mem_block->sig == MEMBLOCK_SIG);
     mem_block->counters64 = (void *)mem_block + sizeof(struct stat_memblock);
     mem_block->counterskbs = (void *)mem_block + sizeof(struct stat_memblock)
                              + mem_block->counters64_size*sizeof(uint64_t);
}

void ci_stat_memblock_reset(struct stat_memblock *block)
{
     int i;
     for (i=0; i<block->counters64_size; i++)
          block->counters64[i] = 0;
     for (i=0; i<block->counterskbs_size; i++) {
          block->counterskbs[i].kb = 0;
          block->counterskbs[i].bytes = 0;
     }  
}

void ci_stat_memblock_merge(struct stat_memblock *dest_block, struct stat_memblock *mem_block)
{
     int i;
     if (!dest_block || !mem_block)
         return;

     for (i=0; i<dest_block->counters64_size && i<mem_block->counters64_size; i++)
          dest_block->counters64[i] += mem_block->counters64[i];

     for (i=0; i<dest_block->counterskbs_size && i<mem_block->counterskbs_size; i++) {
          dest_block->counterskbs[i].kb += mem_block->counterskbs[i].kb;
          dest_block->counterskbs[i].bytes += mem_block->counterskbs[i].bytes;
          dest_block->counterskbs[i].kb += (dest_block->counterskbs[i].bytes >> 10);
          dest_block->counterskbs[i].bytes &= 0x3FF;
     }
}



void ci_stat_area_merge(struct stat_area *dest, struct stat_area *src)
{
     if (!dest->mem_block || !src->mem_block)
         return;

     ci_stat_memblock_merge(dest->mem_block, src->mem_block);
}

