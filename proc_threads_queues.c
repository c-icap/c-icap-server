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
#include "array.h"
#include "debug.h"
#include "log.h"
#include "proc_threads_queues.h"
#include "server.h"
#include "shared_mem.h"
#include <assert.h>

static ci_dyn_array_t *MemBlobs = NULL;
static int MemBlobsCount = 0;
static int64_t old_requests = 0;

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

struct childs_queue *create_childs_queue(int size)
{
    int ret, i;
    struct childs_queue *q = malloc(sizeof(struct childs_queue));
    if (!q) {
        log_server(NULL, "Error allocation memory for children-queue data\n");
        return NULL;
    }
    q->stats_block_size = ci_stat_memblock_size();

    q->shared_mem_size = sizeof(child_shared_data_t) * size /*child shared data*/
                         + q->stats_block_size * size /*child stats area*/
                         + q->stats_block_size /*Server history  stats area*/
                         + sizeof(struct server_statistics) /*Server general statistics*/
                         + MemBlobsCount * sizeof(ci_server_shared_blob_t); /*Register blobs size*/
    if ((q->childs =
                ci_shared_mem_create(&(q->shmid), "kids-queue", q->shared_mem_size)) == NULL) {
        log_server(NULL, "can't get shared memory!");
        free(q);
        return NULL;
    }

    q->size = size;
    /*
      The memory area with statistics memory blocks where share statistics
      for c-icap server are stored is located after the q->childs array of
      child_shared_data_t objects.
     */
    q->stats_area = (void *)(q->childs) + sizeof(child_shared_data_t) * q->size;
    /*
      Children statistics are located to the the following position:
        (q->stats_area + i * q->stats_block_size)
      Where 'i' is a number between 0 and (KidsNumber -1)
      KidsNumber is the number of children equal to q->size.

      To access each child statistics area you need something like the
      following:
      for (i = 0; i < q->size; i++) {
          ci_stat_memblock_t *mb = q->stats_area + i * q->stats_block_size;
      }

      The memory blocks for children statistics will be initialized by each
      child uses the corresponding memory block.
    */

    /*
      The memory for server history statistics, where the statistics from
      exited children are accumulated, is located at:
          (q->stats_area + q->size * q->stats_block_size)
    */
    q->stats_history = ci_stat_memblock_init((void *)(q->stats_area + q->size * q->stats_block_size), q->stats_block_size);
    if (!q->stats_history) {
        ci_shared_mem_destroy(&(q->shmid));
        log_server(NULL, "Failed to initialize statistics area (statistics history area)");
        free(q);
        return NULL;
    }

    q->srv_stats = (struct server_statistics *)(q->stats_area + q->size * q->stats_block_size + q->stats_block_size);
    ci_debug_printf(2, "Create shared mem, qsize=%d stat_block_size=%d children shared data of size:%d\n",
                    q->size,  q->stats_block_size, (int)sizeof(child_shared_data_t) * q->size);

    for (i = 0; i < q->size; i++) {
        q->childs[i].pid = 0;
        q->childs[i].pipe = -1;
    }

    /* reset statistics */
    q->srv_stats->started_childs = 0;
    q->srv_stats->closed_childs = 0;
    q->srv_stats->crashed_childs = 0;
    q->srv_stats->blob_count = MemBlobsCount;

    if ((ret = ci_proc_mutex_init(&(q->queue_mtx), "children-queue")) == 0) {
        /*Release shared mem which is allocated */
        ci_shared_mem_destroy(&(q->shmid));
        log_server(NULL, "can't create children queue semaphore!");
        free(q);
        return NULL;
    }
    return q;
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
    if (ci_shared_mem_detach(&(q->shmid)) == 0) {
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

    if (!ci_shared_mem_destroy(&(q->shmid))) {
        log_server(NULL, "can't destroy shared memory!");
        ci_proc_mutex_unlock(&(q->queue_mtx));
        return 0;
    }

    q->childs = NULL;
    ci_proc_mutex_unlock(&(q->queue_mtx));
    ci_proc_mutex_destroy(&(q->queue_mtx));
    free(q);
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
            q->childs[i].servers = maxservers;
            q->childs[i].usedservers = 0;
            q->childs[i].requests = 0;
            q->childs[i].to_be_killed = 0;
            q->childs[i].father_said = 0;
            q->childs[i].idle = 1;
            q->childs[i].pipe = pipe;
            q->childs[i].stats = (void *)(q->childs) +
                                 sizeof(child_shared_data_t) * q->size +
                                 i * (q->stats_block_size);
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
    ci_stat_memblock_t *child_stats;
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
            assert(ci_stat_memblock_check(child_stats));
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

/*
int find_a_child_to_be_killed(struct childs_queue *q)
{
    int i, which, lessUsedServers;
    ci_proc_mutex_lock(&(q->queue_mtx));
    lessUsedServers = q->childs[0].usedservers;
    which = 0;
    for (i = 1; i < q->size; i++) {
        if (q->childs[i].pid != 0 && lessUsedServers > q->childs[i].usedservers) {
            lessUsedServers = q->childs[i].usedservers;
            which = i;
        }
    }
    ci_proc_mutex_unlock(&(q->queue_mtx));
    return which;
}
*/

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
                       int *used, int64_t *maxrequests)
{
    int i;
    int max_servers = 0;

    *childs = 0;
    *freeservers = 0;
    *used = 0;
    *maxrequests = 0;

    if (!q->childs)
        return 0;

    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid != 0 && q->childs[i].to_be_killed == 0) {
            (*childs)++;
            max_servers += q->childs[i].servers;
            (*used) += q->childs[i].usedservers;
            (*maxrequests) += q->childs[i].requests;
        }
    }
    (*maxrequests) += old_requests;
    (*freeservers) = max_servers - (*used);
    return 1;
}

static int print_statistics(void *data, const char *label, int id, int gId, const ci_stat_t *astat)
{
    ci_kbs_t kbs;
    assert(astat);
    assert(label);
    assert(data);
    ci_stat_memblock_t *stats = (ci_stat_memblock_t *)data;
    assert(ci_stat_memblock_check(stats));
    switch (astat->type) {
    case CI_STAT_INT64_T:
        ci_debug_printf(1,"\t%s:%" PRIu64 "\n",
                        label,
                        ci_stat_memblock_get_counter(stats, id));
        break;
    case CI_STAT_KBS_T:
        kbs = ci_stat_memblock_get_kbs(stats, id);
        ci_debug_printf(1,"\t%s:%" PRIu64 "kbytes and %" PRIu64 " bytes\n",
                        label,
                        ci_kbs_kilobytes(&kbs),
                        ci_kbs_remainder_bytes(&kbs));
        break;
    default:
        break;
    }
    return 0;
}

void dump_queue_statistics(struct childs_queue *q)
{

    int i;
    int childs = 0;
    int maxservers = 0;
    int used = 0;
    int requests = 0;
    ci_stat_memblock_t *child_stats;

    if (!q->childs)
        return;

    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid != 0 && q->childs[i].to_be_killed == 0) {
            childs++;
            maxservers += q->childs[i].servers;
            used += q->childs[i].usedservers;
            requests += q->childs[i].requests;
            ci_debug_printf(1, "\nChild pid:%d\tFree Servers:%d\tUsed Servers:%d\tRequests:% " PRIi64 "\n",
                            q->childs[i].pid, (q->childs[i].servers - q->childs[i].usedservers),
                            q->childs[i].usedservers, q->childs[i].requests
                           );

            child_stats = q->stats_area + i * (q->stats_block_size);
            ci_stat_statistics_iterate(child_stats, -1, print_statistics);
        }
    }
    ci_debug_printf(1, "\nChildren:%d\tFree Servers:%d\tUsed Servers:%d\tRequests:%d\n",
                    childs, maxservers - used, used, requests);
    ci_debug_printf(1,"\nHistory\n");
    ci_stat_statistics_iterate(q->stats_history, -1, print_statistics);
}

extern struct childs_queue *childs_queue;
ci_kbs_t ci_server_stat_kbs_get_running(int id)
{
    const ci_stat_memblock_t *block;
    ci_kbs_t value = {0}, local;
    int i;
    assert(childs_queue);
    for (i = 0; i < childs_queue->size; i++) {
        if (childs_queue->childs[i].pid == 0 || childs_queue->childs[i].to_be_killed != 0)
            continue;
        block = (const ci_stat_memblock_t *) (childs_queue->stats_area + i * (childs_queue->stats_block_size));
        local = ci_stat_memblock_get_kbs(block, id);
        ci_kbs_add_to(&value, &local);
    }

    return value;
}

uint64_t ci_server_stat_uint64_get_running(int id)
{
    const ci_stat_memblock_t *block;
    uint64_t value = 0;
    int i;
    assert(childs_queue);
    for (i = 0; i < childs_queue->size; i++) {
        if (childs_queue->childs[i].pid == 0 || childs_queue->childs[i].to_be_killed != 0)
            continue;
        block = (const ci_stat_memblock_t *) (childs_queue->stats_area + i * (childs_queue->stats_block_size));
        value +=  ci_stat_memblock_get_counter(block, id);
    }

    return value;
}

ci_kbs_t ci_server_stat_kbs_get_global(int id)
{
    ci_kbs_t value = ci_server_stat_kbs_get_running(id);
    ci_kbs_t hist = ci_stat_memblock_get_kbs(childs_queue->stats_history, id);
    ci_kbs_add_to(&value, &hist);
    return value;
}

uint64_t ci_server_stat_uint64_get_global(int id)
{
    uint64_t value = ci_server_stat_uint64_get_running(id);
    value +=  ci_stat_memblock_get_counter(childs_queue->stats_history, id);
    return value;
}

struct shared_blob_data{
    int id;
    int items;
};

int ci_server_shared_memblob_register(const char *name, size_t size)
{
    if (childs_queue) {
        /*Shared mem created, we can not add request more shared memory
          any more*/
        return -1;
    }

    /*
      No need for locking, it runs on single thread mode before kids and
      threads are started.
    */

    if (!MemBlobs) {
         MemBlobs = ci_dyn_array_new2(32, sizeof(int));
    }
    int blobs_number =  size / sizeof(ci_server_shared_blob_t) + ((size % sizeof(ci_server_shared_blob_t)) != 0 ? 1 : 0);
    int id = MemBlobsCount;
    struct shared_blob_data data = {id, blobs_number};
    assert(ci_dyn_array_add(MemBlobs, name, &data, sizeof(struct shared_blob_data)));
    MemBlobsCount += blobs_number;
    return id;
}

void *ci_server_shared_memblob(int id)
{
    if (id < 0 || id >= childs_queue->srv_stats->blob_count)
        return NULL;
    return (void *)(childs_queue->srv_stats->blobs[id].c8);
}

void *ci_server_shared_memblob_byname(const char *name)
{
    if (!MemBlobs)
        return NULL;

    const int *id = ci_dyn_array_search(MemBlobs, name);
    if (!id)
        return NULL;

    return ci_server_shared_memblob(*id);
}
