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
#include <stdio.h>
#include "net_io.h"
#include "proc_mutex.h"
#include "debug.h"
#include "log.h"
#include "request.h"
#include "ci_threads.h"
#include "proc_threads_queues.h"
#include "cfg_param.h"
#include <windows.h>
#include <tchar.h>



typedef struct server_decl {
    int srv_id;
    ci_thread_t srv_pthread;
    struct connections_queue *con_queue;
    ci_request_t *current_req;
    int served_requests;
    int served_requests_no_reallocation;
} server_decl_t;


ci_thread_mutex_t threads_list_mtx;
server_decl_t **threads_list = NULL;

ci_thread_cond_t free_server_cond;
ci_thread_mutex_t counters_mtx;

struct childs_queue *childs_queue = NULL;
child_shared_data_t *child_data;
struct connections_queue *con_queue;

/*Interprocess accepting mutex ....*/
ci_proc_mutex_t accept_mutex;

ci_thread_t worker_thread;
TCHAR *C_ICAP_CMD = TEXT("c-icap.exe -c");

extern int KEEPALIVE_TIMEOUT;
extern int MAX_SECS_TO_LINGER;
extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;
extern struct ci_server_conf CI_CONF;
ci_socket LISTEN_SOCKET;

#define hard_close_connection(connection)  ci_hard_close(connection->fd)
#define close_connection(connection) ci_linger_close(connection->fd,MAX_SECS_TO_LINGER)
#define check_for_keepalive_data(fd) ci_wait_for_data(fd,KEEPALIVE_TIMEOUT,wait_for_read)

static void exit_normaly()
{
    int i = 0;
    server_decl_t *srv;
    ci_debug_printf(1, "Suppose that all children have already exited...\n");
    while ((srv = threads_list[i]) != NULL) {
        if (srv->current_req) {
            close_connection(srv->current_req->connection);
            ci_request_destroy(srv->current_req);
        }
        free(srv);
        threads_list[i] = NULL;
        i++;
    }
    free(threads_list);
    dettach_childs_queue(childs_queue);
    log_close();
}


static void cancel_all_threads()
{
    int i = 0;
//     ci_thread_mutex_lock(&threads_list_mtx);

    ci_thread_cond_broadcast(&(con_queue->queue_cond));        //What about childs that serve a request?
    while (threads_list[i] != NULL) {
        ci_debug_printf(1, "Cancel server %d, thread_id %d (%d)\n",
                        threads_list[i]->srv_id, threads_list[i]->srv_pthread,
                        i);
        ci_thread_join(threads_list[i]->srv_pthread);
        i++;
    }
//     ci_threadmutex_unlock(&threads_list_mtx);
}

server_decl_t *newthread(struct connections_queue *con_queue)
{
    int i = 0;
    server_decl_t *serv;
    serv = (server_decl_t *) malloc(sizeof(server_decl_t));
    serv->srv_id = 0;
//    serv->srv_pthread = pthread_self();
    serv->con_queue = con_queue;
    serv->served_requests = 0;
    serv->served_requests_no_reallocation = 0;
    serv->current_req = NULL;

    return serv;
}




int thread_main(server_decl_t * srv)
{
    struct connections_queue_item con;
    char clientname[CI_MAXHOSTNAMELEN + 1];
    int ret, request_status = 0;

//***********************
//     thread_signals();
//*************************
    //    srv->srv_id = getpid(); //Setting my pid ...
    //    srv->srv_pthread = pthread_self();
    for (;;) {
        if (child_data->to_be_killed) {
            ci_debug_printf(1, "Thread exiting.....\n");
            return 1;        //Exiting thread.....
        }

        if ((ret = get_from_queue(con_queue, &con)) == 0) {
            wait_for_queue(con_queue);       //It is better that the wait_for_queue to be
            //moved into the get_from_queue
            continue;
        }

        if (ret < 0) {        //An error has occured
            ci_debug_printf(1, "Error getting from connections queue\n");
            break;
        }

        ci_netio_init(con.conn.fd);

        ret = 1;
        if (srv->current_req == NULL)
            srv->current_req = newrequest(&con.conn);
        else
            ret = recycle_request(srv->current_req, &con.conn);

        if (srv->current_req == NULL || ret == 0) {
            ci_sockaddr_t_to_host(&(con.conn.claddr), clientname,
                                  CI_MAXHOSTNAMELEN);
            ci_debug_printf(1, "Request from %s denied...\n", clientname);
            hard_close_connection((&con.conn));
            continue;        /*The request rejected. Log an error and continue ... */
        }


        ci_atomic_add_i32(&(child_data->usedservers), 1);

        do {
            if ((request_status = process_request(srv->current_req)) < 0) {
                ci_debug_printf(1,
                                "Process request timeout or interupted....\n");
                break;      //
            }

            srv->served_requests++;
            srv->served_requests_no_reallocation++;

            ci_atomic_add_i64(&child_data->requests, 1);

            log_access(srv->current_req, request_status);
//             break; //No keep-alive ......

            if (child_data->to_be_killed)
                return 1;   //Exiting thread.....

            ci_debug_printf(1, "Keep-alive:%d\n",
                            srv->current_req->keepalive);
            if (srv->current_req->keepalive
                    && check_for_keepalive_data(srv->current_req->connection->
                                                fd) > 0) {
                ci_request_reset(srv->current_req);
                ci_debug_printf(1,
                                "Server %d going to serve new request from client (keep-alive) \n",
                                srv->srv_id);
            } else
                break;
        } while (1);

        if (srv->current_req) {
            if (request_status < 0)
                hard_close_connection(srv->current_req->connection);
            else
                close_connection(srv->current_req->connection);
        }
        if (srv->served_requests_no_reallocation >
                MAX_REQUESTS_BEFORE_REALLOCATE_MEM) {
            ci_debug_printf(1,
                            "Max requests reached, reallocate memory and buffers .....\n");
            ci_request_destroy(srv->current_req);
            srv->current_req = NULL;
            srv->served_requests_no_reallocation = 0;
        }

        ci_atomic_sub_i32(&child_data->usedservers, 1);
        ci_thread_cond_signal(&free_server_cond);

    }
    return 1;
}

/*TODO: Reuse of sockets created during this function.
To do this, call of DiconnectEx function needed instead of closesocket function
and AcceptEx and overlapped operations must used.
The connections queue needs a small redesign
maybe with lists instead of connections array....*/

int worker_main(ci_socket sockfd)
{
    struct connections_queue_item con;
    int claddrlen = sizeof(struct sockaddr_in);
//     char clientname[300];
    int haschild = 1, jobs_in_queue = 0;
    int32_t child_usedservers;
    int pid = 0, error;


    for (;;) {                 //Global for
        if (!ci_proc_mutex_lock(&accept_mutex)) {
            if (child_data->to_be_killed)
                return 1;
            continue;
        }
        child_data->idle = 0;
        pid = (int) child_data->pid;
        ci_debug_printf(1, "Child %d getting requests now ...\n", pid);

        do {                  //Getting requests while we have free servers.....
            ci_debug_printf(1, "In accept loop..................\n");
            error = 0;
            if (((con.conn.fd =
                        accept(sockfd, (struct sockaddr *) &(con.conn.claddr.sockaddr),
                               &claddrlen)) == INVALID_SOCKET) &&
//             if(((conn.fd = WSAAccept(sockfd, (struct sockaddr *)&(conn.claddr), &claddrlen,NULL,NULL)) == INVALID_SOCKET ) &&
                    (error = WSAGetLastError())) {
                ci_debug_printf(1,
                                "error accept .... %d\nExiting server ....\n",
                                error);
                exit(-1);   //For the moment .......
            }

            ci_debug_printf(1, "Accepting one connection...\n");
            claddrlen = sizeof(con.conn.srvaddr.sockaddr);
            getsockname(con.conn.fd,
                        (struct sockaddr *) &(con.conn.srvaddr.sockaddr),
                        &claddrlen);

            ci_fill_sockaddr(&con.conn.claddr);
            ci_fill_sockaddr(&con.conn.srvaddr);

            icap_socket_opts(sockfd, MAX_SECS_TO_LINGER);
            con.proto = CI_PROTO_ICAP;
            if ((jobs_in_queue = put_to_queue(con_queue, &con)) == 0) {
                ci_debug_printf(8,
                                "Jobs in Queue: %d, Free servers: %d, Used Servers: %d, Requests: %d\n",
                                jobs_in_queue, child_data->servers - child_data->usedservers,
                                child_data->usedservers,
                                child_data->requests);
                continue;
            }
            ci_atomic_load_i32(&child_data->usedservers, &child_usedservers);
            haschild = ((child_data->servers - child_usedservers) > 0 ? 1 : 0);
        } while (haschild);

        child_data->idle = 1;
        ci_proc_mutex_unlock(&accept_mutex);

        ci_atomic_load_i32(&child_data->usedservers, &child_usedservers);

        if (child_data->servers - child_usedservers == 0) {
            ci_debug_printf(1,
                            "Child %d waiting for a thread to accept more connections ...\n",
                            pid);
            ci_thread_mutex_lock(&counters_mtx);
            ci_thread_cond_wait(&free_server_cond, &counters_mtx);
            ci_thread_mutex_unlock(&counters_mtx);
        }

    }

}


void child_main(ci_socket sockfd)
{
    int claddrlen = sizeof(ci_sockaddr_t);
    ci_thread_t thread;
    int i, retcode, haschild = 1, jobs_in_queue = 0;
    int pid = 0;
    char op;
    HANDLE hStdin;
    DWORD dwRead;
    //   child_signals();
    //    pid = getpid();

    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if ((hStdin == INVALID_HANDLE_VALUE))
        ExitProcess(1);

    ci_thread_mutex_init(&threads_list_mtx);
    ci_thread_mutex_init(&counters_mtx);
    ci_thread_cond_init(&free_server_cond);


    threads_list =
        (server_decl_t **) malloc((CI_CONF.THREADS_PER_CHILD + 1) *
                                  sizeof(server_decl_t *));
    con_queue = init_queue(CI_CONF.THREADS_PER_CHILD);

    for (i = 0; i < CI_CONF.THREADS_PER_CHILD; i++) {
        if ((threads_list[i] = newthread(con_queue)) == NULL) {
            exit(-1);        // FATAL error.....
        }
        retcode = ci_thread_create(&thread,
                                   (void *(*)(void *)) thread_main,
                                   (void *) threads_list[i]);
    }
    threads_list[CI_CONF.THREADS_PER_CHILD] = NULL;
    ci_debug_printf(1, "Threads created ....\n");
    retcode = ci_thread_create(&worker_thread,
                               (void *(*)(void *)) worker_main,
                               (void *) (sockfd));

//Listen for events from main server better..............

    while (ReadFile(hStdin, &op, 1, &dwRead, NULL)) {
        printf("Operation Read: %c\n", op);
        if (op == 'q')
            goto end_child_main;
    }
    ci_thread_join(worker_thread);

end_child_main:
    cancel_all_threads();
    exit_normaly();
}




//#define MULTICHILD
#undef MULTICHILD
#ifdef MULTICHILD

int create_child(PROCESS_INFORMATION * pi, HANDLE * pipe)
{
    STARTUPINFO si;
    SECURITY_ATTRIBUTES saAttr;
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup, hSaveStdin;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    ZeroMemory(pi, sizeof(PROCESS_INFORMATION));

// Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;


// Save the handle to the current STDIN.

    hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);

// Create a pipe for the child process's STDIN.

    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
        printf("Stdin pipe creation failed\n");
        return 0;
    }

// Set a read handle to the pipe to be STDIN.

    if (!SetStdHandle(STD_INPUT_HANDLE, hChildStdinRd)) {
        printf("Redirecting Stdin failed");
        return 0;
    }

// Duplicate the write handle to the pipe so it is not inherited.

    if (!DuplicateHandle(GetCurrentProcess(), hChildStdinWr, GetCurrentProcess(), &hChildStdinWrDup, 0, FALSE, // not inherited
                         DUPLICATE_SAME_ACCESS)) {
        ci_debug_printf(1, "DuplicateHandle failed");
        return 0;
    }
    CloseHandle(hChildStdinWr);
    *pipe = hChildStdinWrDup;

    ci_debug_printf(1, "Going to start a child...\n");
    // Start the child process.

    if (!CreateProcessW(NULL,  // No module name (use command line).
                        C_ICAP_CMD,    // Command line.
                        NULL,  // Process handle not inheritable.
                        NULL,  // Thread handle not inheritable.
                        TRUE,  // Set handle inheritance to TRUE.
                        0,     // No creation flags.
                        NULL,  // Use parent's environment block.
                        NULL,  // Use parent's starting directory.
                        &si,   // Pointer to STARTUPINFO structure.
                        pi)    // Pointer to PROCESS_INFORMATION structure.
       ) {
        ci_debug_printf(1, "CreateProcess failed. (error:%d)\n",
                        GetLastError());
        return 0;
    }

    if (!SetStdHandle(STD_INPUT_HANDLE, hSaveStdin))
        printf("Re-redirecting Stdin failed\n");
    ci_debug_printf(1, "OK created....\n");
    return 1;
}


int send_handles(DWORD child_ID,
                 HANDLE pipe,
                 HANDLE child_handle,
                 SOCKET sock_fd,
                 HANDLE accept_mtx,
                 HANDLE shmem_id, HANDLE shmem_mtx, int qsize)
{
    DWORD dwWritten;
    HANDLE dupmutex;
    HANDLE dupshmem, dupshmemmtx;
    WSAPROTOCOL_INFO sock_info;

    memset(&sock_info, 0, sizeof(sock_info));

    if (WSADuplicateSocket(sock_fd, child_ID, &sock_info) != 0) {
        ci_debug_printf(1, "Error socket duplicating:%d\n",
                        WSAGetLastError());
    }

    DuplicateHandle(GetCurrentProcess(),
                    accept_mtx,
                    child_handle, &dupmutex, SYNCHRONIZE, FALSE, 0);
    DuplicateHandle(GetCurrentProcess(),
                    shmem_id,
                    child_handle,
                    &dupshmem, SYNCHRONIZE, FALSE, DUPLICATE_SAME_ACCESS);

    DuplicateHandle(GetCurrentProcess(),
                    shmem_mtx,
                    child_handle,
                    &dupshmemmtx, SYNCHRONIZE, FALSE, DUPLICATE_SAME_ACCESS);

    if (!WriteFile(pipe, &child_handle, sizeof(HANDLE), &dwWritten, NULL) ||
            dwWritten != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }

    if (!WriteFile(pipe, &pipe, sizeof(HANDLE), &dwWritten, NULL) ||
            dwWritten != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }

    if (!WriteFile(pipe, &dupmutex, sizeof(HANDLE), &dwWritten, NULL) ||
            dwWritten != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }
    if (!WriteFile(pipe, &dupshmem, sizeof(HANDLE), &dwWritten, NULL) ||
            dwWritten != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }
    if (!WriteFile(pipe, &dupshmemmtx, sizeof(HANDLE), &dwWritten, NULL) ||
            dwWritten != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }
    if (!WriteFile(pipe, &qsize, sizeof(int), &dwWritten, NULL) ||
            dwWritten != sizeof(int)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }
    if (!WriteFile
            (pipe, &sock_info, sizeof(WSAPROTOCOL_INFO), &dwWritten, NULL)
            || dwWritten != sizeof(WSAPROTOCOL_INFO)) {
        ci_debug_printf(1, "Error sending handles\n");
        return 0;
    }
//   snprintf(buf, sizeof(buf), "%d:%d:%d:%d:%d",child_handle,dupmutex,dupshmem,dupshmemmtx,qsize);
//   WriteFile(pipe, buf, strlen(buf)+1, &dwWritten, NULL);
    return 1;
}


HANDLE start_child(ci_socket fd)
{
    HANDLE child_pipe;
    PROCESS_INFORMATION pi;
    if (!create_child(&pi, &child_pipe))
        return 0;
    printf("For child %d Writing to pipe:%d\n", pi.hProcess, child_pipe);
    send_handles(pi.dwProcessId, child_pipe, pi.hProcess, fd, accept_mutex,
                 childs_queue->shmid, childs_queue->queue_mtx,
                 childs_queue->size);
    return pi.hProcess;
}


int do_child()
{
    HANDLE hStdin, child_handle, parent_pipe;
    DWORD dwRead;
    WSAPROTOCOL_INFO sock_info;
    ci_socket sock_fd;

    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if ((hStdin == INVALID_HANDLE_VALUE))
        ExitProcess(1);

//   ReadFile(hStdin, buf, 512, &dwRead, NULL);
//   printf("Reading \"%s\" from server\n",buf);

//   sscanf(buf,"%d:%d:%d:%d:%d",&child_handle,&accept_mutex,
//                          &(childs_queue.shmid),&(childs_queue.queue_mtx),
//        &(childs_queue.size));

    if (!ReadFile(hStdin, &child_handle, sizeof(HANDLE), &dwRead, NULL)
            || dwRead != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }

    if (!ReadFile(hStdin, &parent_pipe, sizeof(HANDLE), &dwRead, NULL)
            || dwRead != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }

    if (!ReadFile(hStdin, &accept_mutex, sizeof(HANDLE), &dwRead, NULL)
            || dwRead != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }
    if (!ReadFile(hStdin, &(childs_queue->shmid), sizeof(HANDLE), &dwRead, NULL)
            || dwRead != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }
    if (!ReadFile
            (hStdin, &(childs_queue->queue_mtx), sizeof(HANDLE), &dwRead, NULL)
            || dwRead != sizeof(HANDLE)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }
    if (!ReadFile(hStdin, &(childs_queue->size), sizeof(int), &dwRead, NULL) ||
            dwRead != sizeof(int)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }

    if (!ReadFile
            (hStdin, &(sock_info), sizeof(WSAPROTOCOL_INFO), &dwRead, NULL)
            || dwRead != sizeof(WSAPROTOCOL_INFO)) {
        ci_debug_printf(1, "Error reading handles.....\n");
        exit(0);
    }

    if ((sock_fd = WSASocket(FROM_PROTOCOL_INFO,
                             FROM_PROTOCOL_INFO,
                             FROM_PROTOCOL_INFO,
                             &(sock_info), 0, 0)) == INVALID_SOCKET) {
        ci_debug_printf(1, "Error in creating socket :%d\n",
                        WSAGetLastError());
        return 0;
    }


    if (!attach_childs_queue(childs_queue)) {
        ci_debug_printf(1, "Error in new child .....\n");
        return 0;
    }
    ci_debug_printf(1, "Shared memory attached....\n");
    child_data =
        register_child(childs_queue, child_handle, CI_CONF.THREADS_PER_CHILD,
                       parent_pipe);
    ci_debug_printf(1, "child registered ....\n");

    child_main(sock_fd);
    exit(0);
}

#endif

int tell_child_to_die(HANDLE pipe)
{
    DWORD dwWritten;
    char op = 'q';
    if (!WriteFile(pipe, &op, 1, &dwWritten, NULL) || dwWritten != 1) {
        return 0;
    }
    return 1;
}

int tell_child_to_restart(HANDLE pipe)
{
    DWORD dwWritten;
    char op = 'r';
    if (!WriteFile(pipe, &op, 1, &dwWritten, NULL) || dwWritten != 1) {
        return 0;
    }
    return 1;
}


ci_thread_mutex_t control_process_mtx;

int wait_achild_to_die()
{
    DWORD i, count, ret;
    HANDLE died_child, *child_handles =
        malloc(sizeof(HANDLE) * childs_queue->size);
    child_shared_data_t *ach;
    while (1) {
        ci_thread_mutex_lock(&control_process_mtx);
        for (i = 0, count = 0; i < (DWORD) childs_queue->size; i++) {
            if (childs_queue->childs[i].pid != 0)
                child_handles[count++] = childs_queue->childs[i].pid;
        }
        if (count == 0) {
            Sleep(100);
            continue;
        }
        ci_thread_mutex_unlock(&control_process_mtx);
        ret = WaitForMultipleObjects(count, child_handles, TRUE, INFINITE);
        if (ret == WAIT_TIMEOUT) {
            ci_debug_printf(1, "What !@#$%^&!!!!! No Timeout exists!!!!!!");
            continue;
        }
        if (ret == WAIT_FAILED) {
            ci_debug_printf(1, "Wait failed. Try again!!!!!!");
            continue;
        }
        ci_thread_mutex_lock(&control_process_mtx);
        died_child = child_handles[ret];
        ci_debug_printf(1,
                        "Child with handle %d died, lets clean-up the queue\n",
                        died_child);
        ach = get_child_data(childs_queue, died_child);
        CloseHandle(ach->pipe);
        remove_child(childs_queue, died_child);
        CloseHandle(died_child);
        ci_thread_mutex_unlock(&control_process_mtx);
    }
}


//int check_for_died_child(struct childs_queue *childs_queue){
int check_for_died_child(DWORD msecs)
{
    DWORD i, count, ret;
//     HANDLE died_child,*child_handles = malloc(sizeof(HANDLE)*childs_queue.size);
    HANDLE died_child, child_handles[MAXIMUM_WAIT_OBJECTS];
    child_shared_data_t *ach;
    for (i = 0, count = 0; i < (DWORD) childs_queue->size; i++) {
        if (childs_queue->childs[i].pid != (HANDLE) 0) {
            child_handles[count++] = childs_queue->childs[i].pid;
        }
        if (count == MAXIMUM_WAIT_OBJECTS)
            break;
    }
    if (count == 0) {
        ci_debug_printf(1, "Oups no children! waiting for a while.....\n!");
        Sleep(1000);
        return 0;
    }
    ci_debug_printf(1, "Objects :%d (max:%d)\n", count, MAXIMUM_WAIT_OBJECTS);
    ret = WaitForMultipleObjects(count, child_handles, FALSE, msecs);
//     ret = WaitForSingleObject(child_handles[0],msecs);
    if (ret == WAIT_TIMEOUT) {
        ci_debug_printf(1, "Operation timeout, no died child....\n");
        return 0;
    }
    if (ret == WAIT_FAILED) {
        ci_debug_printf(1, "Wait failed. Try again!!!!!!");
        return 0;
    }

    died_child = child_handles[ret];
    ci_debug_printf(1, "Child with handle %d died, lets clean-up the queue\n",
                    died_child);
    ach = get_child_data(childs_queue, died_child);
    CloseHandle(ach->pipe);
    remove_child(childs_queue, died_child);
    CloseHandle(died_child);
    return 1;
}

int init_server(int port, int *family)
{
    LISTEN_SOCKET = icap_init_server(port, family, MAX_SECS_TO_LINGER);


    if (LISTEN_SOCKET == CI_SOCKET_ERROR)
        return 0;
    return 1;
}


int start_server()
{

#ifdef MULTICHILD
    int child_indx, i;
    HANDLE child_handle;
    ci_thread_t mon_thread;
    int childs, freeservers, used;
    int64_t maxrequests;

    ci_proc_mutex_init(&accept_mutex);
    ci_thread_mutex_init(&control_process_mtx);

    if (!(childs_queue = create_childs_queue(CI_CONF.MAX_SERVERS))) {
        log_server(NULL, "Can't init shared memory.Fatal error, exiting!\n");
        ci_debug_printf(1,
                        "Can't init shared memory.Fatal error, exiting!\n");
        exit(0);
    }

    for (i = 0; i < CI_CONF.START_SERVERS + 2; i++) {
        child_handle = start_child(LISTEN_SOCKET);
    }

    /*Start died childs monitor thread*/
    /*     ci_thread_create(&mon_thread,
                  (void *(*)(void *))wait_achild_to_die,
                  (void *)NULL);
    */
    while (1) {
        if (check_for_died_child(5000))
            continue;
//        Sleep(5000);
        childs_queue_stats(childs_queue, &childs, &freeservers, &used,
                           &maxrequests);
        ci_debug_printf(1,
                        "Server stats: \n\t Children:%d\n\t Free servers:%d\n\tUsed servers:%d\n\tRequests served:%d\n",
                        childs, freeservers, used, maxrequests);

        if ((freeservers <= CI_CONF.MIN_SPARE_THREADS && childs < CI_CONF.MAX_SERVERS)
                || childs < CI_CONF.START_SERVERS) {
            ci_debug_printf(1, "Going to start a child .....\n");
            child_handle = start_child(LISTEN_SOCKET);
        } else if (freeservers >= CI_CONF.MAX_SPARE_THREADS && childs > CI_CONF.START_SERVERS) {
            ci_thread_mutex_lock(&control_process_mtx);
            if ((child_indx = find_an_idle_child(childs_queue)) < 0)
                continue;
            childs_queue->childs[child_indx].to_be_killed = GRACEFULLY;
            tell_child_to_die(childs_queue->childs[child_indx].pipe);
            ci_thread_mutex_unlock(&control_process_mtx);
            ci_debug_printf(1, "Going to stop child %d .....\n",
                            childs_queue->childs[child_indx].pid);
        }
    }
    /*
         for(i = 0; i<CI_CONF.START_SERVERS; i++){
          pid = wait(&status);
          ci_debug_printf(1,"The child %d died with status %d\n",pid,status);
         }
    */


#else
    child_data = (child_shared_data_t *) malloc(sizeof(child_shared_data_t));
    child_data->pid = 0;
    child_data->freeservers = CI_CONF.THREADS_PER_CHILD;
    child_data->usedservers = 0;
    child_data->requests = 0;
    child_data->to_be_killed = 0;
    child_data->idle = 1;
    child_main(LISTEN_SOCKET);
#endif

    return 1;
}
