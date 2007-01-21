/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "c-icap.h"
#include "debug.h"
#include "log.h"
#include "proc_threads_queues.h"
#include "shared_mem.h"



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
     (ret == 0 && (ret = ci_thread_mutex_init(&(q->cond_mtx))));
     (ret == 0 && (ret = ci_thread_cond_init(&(q->queue_cond))));

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
/*  Childs queue......                                                             */


int create_childs_queue(struct childs_queue *q, int size)
{
     int ret, i;

     if ((q->childs =
          ci_shared_mem_create(&(q->shmid),
                               sizeof(child_shared_data_t) * size)) == NULL) {
          log_server(NULL, "can't get shared memory!");
          return 0;
     }

     q->size = size;

     for (i = 0; i < q->size; i++) {
          q->childs[i].pid = 0;
          q->childs[i].pipe = -1;
     }


     if ((ret = ci_proc_mutex_init(&(q->queue_mtx))) == 0) {
          log_server(NULL, "can't create childs queue semaphore!");
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
               ci_proc_mutex_unlock(&(q->queue_mtx));
               return &(q->childs[i]);
          }
     }
     ci_proc_mutex_unlock(&(q->queue_mtx));
     return NULL;
}


int remove_child(struct childs_queue *q, process_pid_t pid)
{
     int i;

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


int find_an_idle_child(struct childs_queue *q)
{
     int i, which, requests = -1;
     which = -1;
     ci_proc_mutex_lock(&(q->queue_mtx));
     for (i = 0; i < q->size; i++) {
          if (q->childs[i].pid == 0)
               continue;
          if (q->childs[i].to_be_killed) {
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
