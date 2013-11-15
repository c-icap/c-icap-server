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
#include "log.h"
#include "proc_threads_queues.h"
#include "shared_mem.h"
#include <assert.h>


static int old_requests = 0;

struct connections_queue *init_queue(int size)
{
     int ret;
     struct connections_queue *q;
     if ((q =
          (struct connections_queue *) malloc(sizeof(struct connections_queue)))
         == NULL)
          return NULL;


     ret = ci_thread_mutex_init(&(q->queue_mtx));
     if (ret == 0) ret = ci_thread_mutex_init(&(q->cond_mtx));
     if (ret == 0) ret = ci_thread_cond_init(&(q->queue_cond));

     if (ret == 0
         && (q->connections =
             (ci_connection_t *) malloc(size * sizeof(ci_connection_t))) !=
         NULL) {
          q->size = size;
          q->used = 0;
          return q;
     }
     //else memory allocation failed or mutex/cond init failed
     if (q->connections)
          free(q->connections);
     free(q);
     return NULL;
}

void destroy_queue(struct connections_queue *q)
{
     ci_thread_mutex_destroy(&(q->queue_mtx));
     ci_thread_cond_destroy(&(q->queue_cond));
     free(q->connections);
     free(q);
}

int put_to_queue(struct connections_queue *q, ci_connection_t * con)
{
     int ret;
     if (ci_thread_mutex_lock(&(q->queue_mtx)) != 0)
          return -1;
     if (q->used == q->size) {
          ci_thread_mutex_unlock(&(q->queue_mtx));
          ci_debug_printf(1, "put_to_queue_fatal error used=%d size=%d\n",
                          q->used, q->size);
          return 0;
     }
     ci_copy_connection(&(q->connections[q->used]), con);
     ret = ++q->used;
     ci_thread_mutex_unlock(&(q->queue_mtx));
     ci_thread_cond_signal(&(q->queue_cond));   //????
     return ret;
}

int get_from_queue(struct connections_queue *q, ci_connection_t * con)
{
     if (ci_thread_mutex_lock(&(q->queue_mtx)) != 0)
          return -1;
     if (q->used == 0) {
          ci_thread_mutex_unlock(&(q->queue_mtx));
          return 0;
     }
     q->used--;
     ci_copy_connection(con, &(q->connections[q->used]));
     ci_thread_mutex_unlock(&(q->queue_mtx));
     return 1;
}

int wait_for_queue(struct connections_queue *q)
{
     ci_debug_printf(7, "Waiting for a request....\n");
     if (ci_thread_mutex_lock(&(q->cond_mtx)) != 0)
          return -1;
     if (ci_thread_cond_wait(&(q->queue_cond), &(q->cond_mtx)) != 0) {
          ci_thread_mutex_unlock(&(q->cond_mtx));
          return -1;
     }
     if (ci_thread_mutex_unlock(&(q->cond_mtx)) != 0)
          return -1;
     return 1;
}


/***********************************************************************************/
/*                                                                                 */
/*  Children queue......                                                             */

int create_childs_queue(struct childs_queue *q, int size)
{
     int ret, i;
     struct stat_memblock *mem_block;
     q->stats_block_size = ci_stat_memblock_size();
     
     q->shared_mem_size = sizeof(child_shared_data_t) * size /*child shared data*/
                          + q->stats_block_size * size /*child stats area*/
                          + q->stats_block_size /*Server history  stats area*/
                          + sizeof(struct server_statistics); /*Server general statistics*/
     if ((q->childs =
          ci_shared_mem_create(&(q->shmid), q->shared_mem_size)) == NULL) {
          log_server(NULL, "can't get shared memory!");
          return 0;
     }

     q->size = size;
     q->stats_area = (void *)(q->childs) + sizeof(child_shared_data_t) * q->size;
     for (i = 0; i < q->size; i++) {
       mem_block = q->stats_area + i * q->stats_block_size;
       mem_block->sig = MEMBLOCK_SIG;
     }       
     q->stats_history = q->stats_area + q->size * q->stats_block_size;
     q->stats_history->sig = MEMBLOCK_SIG;

     q->srv_stats = (struct server_statistics *)(q->stats_area + q->size * q->stats_block_size + q->stats_block_size);
     ci_debug_printf(2, "Create shared mem, qsize=%d stat_block_size=%d childshared data:%d\n",
		     q->size,  q->stats_block_size, (int)sizeof(child_shared_data_t) * q->size);

     stat_memblock_fix(q->stats_history);
     ci_stat_memblock_reset(q->stats_history);

     for (i = 0; i < q->size; i++) {
          q->childs[i].pid = 0;
          q->childs[i].pipe = -1;
     }

     /* reset statistics */
     q->srv_stats->started_childs = 0;
     q->srv_stats->closed_childs = 0;
     q->srv_stats->crashed_childs = 0;

     if ((ret = ci_proc_mutex_init(&(q->queue_mtx))) == 0) {
         /*Release shared mem which is allocated */
          ci_shared_mem_destroy(&(q->shmid), q->childs);
          log_server(NULL, "can't create children queue semaphore!");
          return 0;
     }
     return 1;
}

int attach_childs_queue(struct childs_queue *q)
{
     child_shared_data_t *c;
     ci_proc_mutex_lock(&(q->queue_mtx));       //Not really needed .........

     if ((c =
          (child_shared_data_t *) ci_shared_mem_attach(&(q->shmid))) == NULL) {
          log_server(NULL, "can't attach shared memory!");
          ci_proc_mutex_unlock(&(q->queue_mtx));
          return 0;
     }

     q->childs = c;
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return 1;
}

int dettach_childs_queue(struct childs_queue *q)
{

     ci_proc_mutex_lock(&(q->queue_mtx));       //Not really needed .........
     if (ci_shared_mem_detach(&(q->shmid), q->childs) == 0) {
          log_server(NULL, "can't dettach shared memory!");
          ci_proc_mutex_unlock(&(q->queue_mtx));
          return 0;
     }

     q->childs = NULL;
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return 1;
}

int destroy_childs_queue(struct childs_queue *q)
{

     ci_proc_mutex_lock(&(q->queue_mtx));       //Not really needed .........

     if (!ci_shared_mem_destroy(&(q->shmid), q->childs)) {
          log_server(NULL, "can't destroy shared memory!");
          ci_proc_mutex_unlock(&(q->queue_mtx));
          return 0;
     }

     q->childs = NULL;
     ci_proc_mutex_unlock(&(q->queue_mtx));
     ci_proc_mutex_destroy(&(q->queue_mtx));
     return 1;
}

int childs_queue_is_empty(struct childs_queue *q)
{
     int i;
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid != 0)
               return 0;
     }
     return 1;
}


child_shared_data_t *get_child_data(struct childs_queue * q, process_pid_t pid)
{
     int i;
     if (!q->childs)
          return NULL;

     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == pid)
               return &(q->childs[i]);
     }
     return NULL;
}

child_shared_data_t *register_child(struct childs_queue * q,
                                    process_pid_t pid, int maxservers,
                                    ci_pipe_t pipe)
{
     int i;

     if (!q->childs)
          return NULL;
     ci_debug_printf(8, "Register in shared mem, qsize=%d stat_block_size=%d childshared data:%d\n",
		     q->size,  q->stats_block_size, (int) sizeof(child_shared_data_t) * q->size);
     ci_proc_mutex_lock(&(q->queue_mtx));
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == 0) {
               q->childs[i].pid = pid;
               q->childs[i].freeservers = maxservers;
               q->childs[i].usedservers = 0;
               q->childs[i].requests = 0;
               q->childs[i].connections = 0;
               q->childs[i].to_be_killed = 0;
               q->childs[i].father_said = 0;
               q->childs[i].idle = 1;
               q->childs[i].pipe = pipe;
	       q->childs[i].stats = (void *)(q->childs) + 
		                    sizeof(child_shared_data_t) * q->size + 
		                    i * (q->stats_block_size);
	       assert(q->childs[i].stats->sig == MEMBLOCK_SIG);
               q->childs[i].stats_size = q->stats_block_size;
               ci_proc_mutex_unlock(&(q->queue_mtx));
               return &(q->childs[i]);
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return NULL;
}

void announce_child(struct childs_queue *q, process_pid_t pid)
{
    if (q->childs && pid)
        q->srv_stats->started_childs++;
}

int remove_child(struct childs_queue *q, process_pid_t pid, int status)
{
     int i;
     struct stat_memblock *child_stats;
     if (!q->childs)
          return 0;

     ci_proc_mutex_lock(&(q->queue_mtx));
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == pid) {
               q->childs[i].pid = 0;
               old_requests += q->childs[i].requests;
               if (q->childs[i].pipe >= 0) {
                    close(q->childs[i].pipe);
                    q->childs[i].pipe = -1;
               }
	       child_stats = q->stats_area + i * (q->stats_block_size);
	       /*re-arange pointers in childs memblock*/
	       stat_memblock_reconstruct(child_stats);
	       ci_stat_memblock_merge(q->stats_history, child_stats);
               q->srv_stats->closed_childs++;
               if (status)
                   q->srv_stats->crashed_childs++;
               ci_proc_mutex_unlock(&(q->queue_mtx));
               return 1;
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return 0;
}

int find_a_child_to_be_killed(struct childs_queue *q)
{
     int i, which, freeservers;
     ci_proc_mutex_lock(&(q->queue_mtx));
     freeservers = q->childs[0].freeservers;
     which = 0;
     for (i = 1; i < q->size; i++) {
          if (q->childs[i].pid != 0 && freeservers > q->childs[i].freeservers) {
               freeservers = q->childs[i].freeservers;
               which = i;
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return which;
}

int find_a_child_nrequests(struct childs_queue *q, int max_requests)
{
     int i, which, requests;
     which = -1;
     requests = max_requests;
     ci_proc_mutex_lock(&(q->queue_mtx));
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == 0)
               continue;
          if (q->childs[i].to_be_killed) {      /*If a death of a child pending do not kill any other */
               ci_proc_mutex_unlock(&(q->queue_mtx));
               return -1;
          }
          if (requests < q->childs[i].requests) {
               requests = q->childs[i].requests;
               which = i;
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return which;
}

int find_an_idle_child(struct childs_queue *q)
{
     int i, which, requests = -1;
     which = -1;
     ci_proc_mutex_lock(&(q->queue_mtx));
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == 0)
               continue;
          if (q->childs[i].to_be_killed) {      /*A child going to die wait... */
               ci_proc_mutex_unlock(&(q->queue_mtx));
               return -1;
          }
          if (q->childs[i].usedservers == 0 && q->childs[i].idle == 1) {
               if (requests < q->childs[i].requests) {
                    requests = q->childs[i].requests;
                    which = i;
               }
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return which;
}

int childs_queue_stats(struct childs_queue *q, int *childs, int *freeservers,
                       int *used, int *maxrequests)
{
     int i;

     *childs = 0;
     *freeservers = 0;
     *used = 0;
     *maxrequests = 0;

     if (!q->childs)
          return 0;

     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid != 0 && q->childs[i].to_be_killed == 0) {
               (*childs)++;
               (*freeservers) += q->childs[i].freeservers;
               (*used) += q->childs[i].usedservers;
               (*maxrequests) += q->childs[i].requests;
          }
     }
     (*maxrequests) += old_requests;
     return 1;
}


void dump_queue_statistics(struct childs_queue *q)
{

    int i, k;
    int childs = 0;
    int freeservers = 0;
    int used = 0;
    int requests = 0;
    struct stat_memblock *child_stats, copy_stats;

     if (!q->childs)
          return;

     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid != 0 && q->childs[i].to_be_killed == 0) {
               childs++;
               freeservers += q->childs[i].freeservers;
               used += q->childs[i].usedservers;
               requests += q->childs[i].requests;
	       ci_debug_printf(1, "\nChild pid:%d\tFree Servers:%d\tUsed Servers:%d\tRequests:%d\n",
			       q->childs[i].pid, q->childs[i].freeservers,  
			       q->childs[i].usedservers, q->childs[i].requests
			       );

	       child_stats = q->stats_area + i * (q->stats_block_size);
	       copy_stats.counters64_size = child_stats->counters64_size;
	       copy_stats.counterskbs_size = child_stats->counterskbs_size;
	       copy_stats.counters64 = (void *)child_stats + sizeof(struct stat_memblock);
	       copy_stats.counterskbs = (void *)child_stats + sizeof(struct stat_memblock) 
		 + child_stats->counters64_size*sizeof(uint64_t);

	       for (k=0; k < copy_stats.counters64_size && k < STAT_INT64.entries_num; k++)
		   ci_debug_printf(1,"\t%s:%llu\n", STAT_INT64.entries[k].label,
				   (long long unsigned) copy_stats.counters64[k]);

	       for (k=0; k < copy_stats.counterskbs_size && k < STAT_KBS.entries_num; k++)
		 ci_debug_printf(1,"\t%s:%llu kbytes and %d bytes\n",
				 STAT_KBS.entries[k].label,
				 (long long unsigned) copy_stats.counterskbs[k].kb,
				 copy_stats.counterskbs[k].bytes);
          }
     }
     ci_debug_printf(1, "\nChildren:%d\tFree Servers:%d\tUsed Servers:%d\tRequests:%d\n",
		     childs, freeservers, used, requests);
     ci_debug_printf(1,"\nHistory\n");
     for (k=0; k < q->stats_history->counters64_size && k < STAT_INT64.entries_num; k++)
       ci_debug_printf(1,"\t%s:%llu\n", STAT_INT64.entries[k].label,
		       (long long unsigned) q->stats_history->counters64[k]);

     for (k=0; k < q->stats_history->counterskbs_size && k < STAT_KBS.entries_num; k++)
       ci_debug_printf(1,"\t%s:%llu kbytes  and %d bytes\n",STAT_KBS.entries[k].label, 
		       (long long unsigned) q->stats_history->counterskbs[k].kb,
		       q->stats_history->counterskbs[k].bytes);
     
}
