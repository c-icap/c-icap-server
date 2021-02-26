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


#ifndef __C_ICAP_PROC_THREADS_QUEUES_H
#define __C_ICAP_PROC_THREADS_QUEUES_H

#include "net_io.h"
#include "ci_threads.h"
#include "proc_mutex.h"
#include "shared_mem.h"
#include "stats.h"

#ifdef __cplusplus
extern "C"
{
#endif

enum KILL_MODE {NO_KILL = 0,GRACEFULLY,IMMEDIATELY};

#ifdef _WIN32
#define process_pid_t HANDLE
#define ci_pipe_t     HANDLE
#else
#define process_pid_t int
#define ci_pipe_t   int
#endif


struct connections_queue {
    ci_connection_t *connections;
    int used;
    int size;
    ci_thread_mutex_t queue_mtx;
    ci_thread_mutex_t cond_mtx;
    ci_thread_cond_t queue_cond;
};


typedef struct child_shared_data {
    int freeservers;
    int usedservers;
    int requests;
    int connections;
    process_pid_t pid;
    int idle;
    int to_be_killed;
    int father_said;
    ci_pipe_t pipe;
    void *stats;
    int stats_size;
} child_shared_data_t;


typedef struct ci_server_shared_blob {
    union {
        char *ptr;
        struct c64bit {
            uint64_t c1;
            uint64_t c2;
            uint64_t c3;
            uint64_t c4;
        } c64;
        unsigned char c8[32];
    };
} ci_server_shared_blob_t;

struct server_statistics {
    unsigned int started_childs;
    unsigned int closed_childs;
    unsigned int crashed_childs;
    int blob_count;
    ci_server_shared_blob_t blobs[];
};

struct childs_queue {
    child_shared_data_t *childs;
    int size;
    int shared_mem_size;
    int stats_block_size;
    void  *stats_area;
    ci_stat_memblock_t *stats_history;
    ci_shared_mem_id_t shmid;
    ci_proc_mutex_t queue_mtx;
    struct server_statistics *srv_stats;
};



struct connections_queue *init_queue(int size);
void destroy_queue(struct connections_queue *q);
int put_to_queue(struct connections_queue *q,ci_connection_t *con);
int get_from_queue(struct connections_queue *q, ci_connection_t *con);
int wait_for_queue(struct connections_queue *q);
#define connections_pending(q) (q->used)


struct childs_queue *create_childs_queue(int size);
int destroy_childs_queue(struct childs_queue *q);
void announce_child(struct childs_queue *q, process_pid_t pid);
int attach_childs_queue(struct childs_queue *q);
int dettach_childs_queue(struct childs_queue *q);
int childs_queue_is_empty(struct childs_queue *q);
child_shared_data_t *get_child_data(struct childs_queue *q, process_pid_t pid);
child_shared_data_t *register_child(struct childs_queue *q,
                                    process_pid_t pid,
                                    int maxservers,
                                    ci_pipe_t pipe
                                   );

int remove_child(struct childs_queue *q, process_pid_t pid, int status);
int find_a_child_to_be_killed(struct childs_queue *q);
int find_a_child_nrequests(struct childs_queue *q,int max_requests);
int find_an_idle_child(struct childs_queue *q);
int childs_queue_stats(struct childs_queue *q, int *childs,
                       int *freeservers, int *used, int *maxrequests);
void dump_queue_statistics(struct childs_queue *q);

/*c-icap server statistics functions*/
/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * current child.
 \ingroup SERVER
 */
static inline ci_kbs_t ci_server_stat_kbs_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    assert(block);
    return  ci_stat_memblock_get_kbs(block, id);
}

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * current child.
 \ingroup SERVER
 */
static inline uint64_t ci_server_stat_uint64_get(int id)
{
    ci_stat_memblock_t *block = ci_stat_memblock_get();
    assert(block);
    return  ci_stat_memblock_get_counter(block, id);
}

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * running children.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_running(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * running children.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_running(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_KBS_T for the
 * c-icap server. This is include history data.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(ci_kbs_t) ci_server_stat_kbs_get_global(int id);

/**
 * Retrieves the value of a counter of type CI_STAT_INT64_T for the
 * c-icap server. This is include history data.
 \ingroup SERVER
 */
CI_DECLARE_FUNC(uint64_t) ci_server_stat_uint64_get_global(int id);

CI_DECLARE_FUNC(int) ci_server_shared_memblob_register(const char *name, size_t size);
CI_DECLARE_FUNC(ci_server_shared_blob_t *) ci_server_shared_memblob(int ID);
CI_DECLARE_FUNC(ci_server_shared_blob_t *) ci_server_shared_memblob_byname(const char *name);

#ifdef __cplusplus
}
#endif

#endif
