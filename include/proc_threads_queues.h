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


#ifndef _PROC_THREADS_QUEUES_H
#define _PROC_THREADS_QUEUES_H

#include "net_io.h"
#include "ci_threads.h"
#include "proc_mutex.h"
#include "shared_mem.h"

enum KILL_MODE {NO_KILL=0,GRACEFULLY,IMMEDIATELY};

#ifdef _WIN32
#define process_pid_t HANDLE
#else 
#define process_pid_t int
#endif


struct connections_queue{
     ci_connection_t *connections;
     int used;
     int size;
     ci_thread_mutex_t queue_mtx;
     ci_thread_mutex_t cond_mtx;
     ci_thread_cond_t queue_cond;
};


typedef struct child_shared_data{
     int freeservers;
     int usedservers;
     int requests;
     int connections;
     process_pid_t pid;
     int idle;
     int to_be_killed;
#ifdef _WIN32
     HANDLE pipe;
#endif
} child_shared_data_t;


struct childs_queue{
     child_shared_data_t *childs;
     int size;
     ci_shared_mem_id_t shmid;
     ci_proc_mutex_t queue_mtx;
};



struct connections_queue *init_queue(int size);
void destroy_queue(struct connections_queue *q);
int put_to_queue(struct connections_queue *q,ci_connection_t *con);
int get_from_queue(struct connections_queue *q, ci_connection_t *con);
int wait_for_queue(struct connections_queue *q);
#define connections_pending(q) (q->used)


int create_childs_queue(struct childs_queue *q, int size);
int destroy_childs_queue(struct childs_queue *q);
int attach_childs_queue(struct childs_queue *q);
int dettach_childs_queue(struct childs_queue *q);
child_shared_data_t *get_child_data(struct childs_queue *q, process_pid_t pid);
child_shared_data_t *register_child(struct childs_queue *q, 
				    process_pid_t pid,
				    int maxservers
#ifdef _WIN32
				    ,HANDLE pipe
#endif
);

int remove_child(struct childs_queue *q, process_pid_t pid);
int find_a_child_to_be_killed(struct childs_queue *q);
int find_an_idle_child(struct childs_queue *q);
int childs_queue_stats(struct childs_queue *q, int *childs,
		       int *freeservers, int *used, int *maxrequests);

#endif
