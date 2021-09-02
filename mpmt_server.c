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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#if defined(USE_POLL)
#include <poll.h>
#else
#include <sys/select.h>
#endif
#include <assert.h>
#include "acl.h"
#include "net_io.h"
#if defined(USE_OPENSSL)
#include "net_io_ssl.h"
#endif
#include "proc_mutex.h"
#include "debug.h"
#include "log.h"
#include "request.h"
#include "ci_threads.h"
#include "proc_threads_queues.h"
#include "port.h"
#include "cfg_param.h"
#include "commands.h"
#include "util.h"

#define MULTICHILD
//#undef MULTICHILD


extern int MAX_KEEPALIVE_REQUESTS;
extern int MAX_SECS_TO_LINGER;
extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;
extern int MAX_REQUESTS_PER_CHILD;
extern struct ci_server_conf CI_CONF;

typedef struct server_decl {
    int srv_id;
    ci_thread_t srv_pthread;
    struct connections_queue *con_queue;
    ci_request_t *current_req;
    int served_requests;
    int served_requests_no_reallocation;
    int running;
} server_decl_t;

ci_thread_mutex_t threads_list_mtx;
server_decl_t **threads_list = NULL;
ci_thread_t listener_thread_id;
int listener_running = 0;

ci_thread_cond_t free_server_cond;
ci_thread_mutex_t counters_mtx;

struct childs_queue *childs_queue = NULL;
struct childs_queue *old_childs_queue = NULL;
child_shared_data_t *child_data = NULL;
struct connections_queue *con_queue;
process_pid_t MY_PROC_PID = 0;
/*Child shutdown timeout is 10 seconds:*/
const int CHILD_SHUTDOWN_TIMEOUT = 10;
int CHILD_HALT = 0;

/*Interprocess accepting mutex ....*/
ci_proc_mutex_t accept_mutex;

/*Main proccess variables*/
int c_icap_going_to_term = 0;
int c_icap_reconfigure = 0;

void init_commands();
int init_server();
int start_child();
void system_shutdown();
void config_destroy();

/***************************************************************************************/
/*Signals managment functions                                                          */

static void term_handler_child(int sig)
{
    if (!child_data)
        return; /*going down?*/

    if (!child_data->father_said)
        child_data->to_be_killed = IMMEDIATELY;
    else
        child_data->to_be_killed = child_data->father_said;
}

static void sigpipe_handler(int sig)
{
}

static void empty(int sig)
{
}

static void sigint_handler_main(int sig)
{
    if (sig == SIGTERM) {
    } else if (sig == SIGINT) {
    } else {
    }

    c_icap_going_to_term = 1;
}

static void sigchld_handler_main(int sig)
{
    /*Do nothing the signal will be ignored..... */
}

static void sighup_handler_main()
{
    c_icap_reconfigure = 1;
}

void child_signals()
{
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, term_handler_child);
    signal(SIGHUP, empty);

    /*Maybe the SIGCHLD must not ignored but better
       a signal handler must be developed with an
       interface for use from modules
     */
    /*     signal(SIGCHLD,SIG_IGN);*/
}

void main_signals()
{
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGTERM, sigint_handler_main);
    signal(SIGINT, sigint_handler_main);
    signal(SIGCHLD, sigchld_handler_main);
    signal(SIGHUP, sighup_handler_main);
}


void thread_signals(int islistener)
{
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGINT);
    if (!islistener)
        sigaddset(&sig_mask, SIGHUP);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL))
        ci_debug_printf(5, "O an error....\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

/*****************************************************************************/
/*Functions for handling  operations for events                              */

static void exit_normaly()
{
    system_shutdown();
#ifdef MULTICHILD
    child_data = NULL;
    dettach_childs_queue(childs_queue);
    config_destroy();
    commands_destroy();
    ci_acl_destroy();
    ci_mem_exit();
#endif
}

static void release_thread_i(int i)
{
    if (threads_list[i]->current_req) {
        ci_request_destroy(threads_list[i]->current_req);
    }
    free(threads_list[i]);
    threads_list[i] = NULL;
}

static void cancel_all_threads()
{
    int i = 0;
    int wait_listener_time = 10000;
    int wait_for_workers = CHILD_SHUTDOWN_TIMEOUT>5?CHILD_SHUTDOWN_TIMEOUT:5;
    int servers_running;
    /*Cancel listener thread .... */
    /*we are going to wait maximum about 10 ms */
    while (wait_listener_time > 0 && listener_running != 0) {
        /*Interrupt listener if it is waiting for threads or
           waiting to accept new connections */
        pthread_kill(listener_thread_id, SIGHUP);
        ci_thread_cond_signal(&free_server_cond);
        ci_usleep(1000);
        wait_listener_time -= 10;
    }
    if (listener_running == 0) {
        ci_debug_printf(5,
                        "Going to wait for the listener thread (pid: %d) to exit!\n",
                        threads_list[0]->srv_id);
        ci_thread_join(listener_thread_id);
        ci_debug_printf(5, "OK, cancelling the listener thread (pid: %d)!\n",
                        threads_list[0]->srv_id);
    } else {
        /*fuck the listener! going down ..... */
    }

    /*We are going to interupt the waiting for queue childs.
       We are going to wait threads which serve a request. */
    ci_thread_cond_broadcast(&(con_queue->queue_cond));
    /*wait for a milisecond*/
    ci_usleep(1000);
    servers_running = CI_CONF.THREADS_PER_CHILD;
    while (servers_running && wait_for_workers >= 0) {
        /*child_data->to_be_killed, may change while we are inside this loop*/
        if (child_data->to_be_killed == IMMEDIATELY) {
            CHILD_HALT = 1;
        }
        for (i=0; i<CI_CONF.THREADS_PER_CHILD; i++) {
            if (threads_list[i] != NULL) { /* if the i thread is still alive*/
                if (!threads_list[i]->running) { /*if the i thread is not running any more*/
                    ci_debug_printf(5, "Cancel server %" PRIu64 ", thread_id %" PRIu64 " (%d)\n",
                                    (uint64_t)threads_list[i]->srv_id, (uint64_t)threads_list[i]->srv_pthread,
                                    i);
                    ci_thread_join(threads_list[i]->srv_pthread);
                    release_thread_i(i);
                    servers_running --;
                } else if (child_data->to_be_killed == IMMEDIATELY) {
                    /*The thread is still running, and we have a timeout for waiting
                      the thread to exit. */
                    if (wait_for_workers <= 2) {
		        ci_debug_printf(5, "Thread %" PRIu64 " still running near the timeout. Try to kill it\n", (uint64_t)threads_list[i]->srv_pthread);
                        pthread_kill( threads_list[i]->srv_pthread, SIGTERM);
                    }
                }
            }/*the i thread is still alive*/
        } /* for(i=0;i< CI_CONF.THREADS_PER_CHILD;i++)*/

        /*wait for 1 second for the next round*/
        ci_usleep(999999);

        /*
          The child_data->to_be_killed may change while we are running this function.
          In the case it has/got the value IMMEDIATELY decrease wait_for_workers:
        */
        if (child_data->to_be_killed == IMMEDIATELY)
            wait_for_workers --;

    } /* while(servers_running)*/

    if (servers_running) {
        ci_debug_printf(5, "Not all the servers canceled. Anyway exiting....\n");
    } else {
        ci_debug_printf(5, "All servers canceled\n");
        free(threads_list);
    }
}

static void send_term_to_childs(struct childs_queue *q)
{
    int i, pid;
    for (i = 0; i < q->size; i++) {
        if ((pid = q->childs[i].pid) == 0)
            continue;
        if (q->childs[i].to_be_killed != IMMEDIATELY) {
            /*Child did not informed yet*/
            q->childs[i].father_said = IMMEDIATELY;
            kill(pid, SIGTERM);
        }
    }
}

static void wait_childs_to_exit(struct childs_queue *q)
{
    int i, status, pid;
    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid == 0)
            continue;
        ci_debug_printf(5, "Wait for child with pid: %d\n", q->childs[i].pid);
        if (q->childs[i].to_be_killed != IMMEDIATELY) {
            ci_debug_printf(5, "Child %d not signaled yet!\n", q->childs[i].pid);
            continue;
        }

        do {
            errno = 0;
            pid = waitpid(q->childs[i].pid, &status, WNOHANG);
        } while (pid < 0 && errno == EINTR);

        if (pid > 0) {
            remove_child(q, pid, 0);
            ci_debug_printf(5, "Child %d died with status %d\n", pid, status);
        }
    }
}

static void kill_all_childs()
{
    int childs_running;
    ci_debug_printf(5, "Going to term children....\n");

    do {
        send_term_to_childs(childs_queue);
        if (old_childs_queue)
            send_term_to_childs(old_childs_queue);

        /*wait for 30 milisecond for childs to take care*/
        ci_usleep(30000);

        wait_childs_to_exit(childs_queue);
        childs_running = !childs_queue_is_empty(childs_queue);
        if (old_childs_queue) {
            wait_childs_to_exit(old_childs_queue);
            childs_running += !childs_queue_is_empty(old_childs_queue);
        }
    } while (childs_running);

    ci_proc_mutex_destroy(&accept_mutex);
    destroy_childs_queue(childs_queue);
    childs_queue = NULL;
    if (old_childs_queue)
        destroy_childs_queue(old_childs_queue);
    old_childs_queue = NULL;
}

static void check_for_exited_childs()
{
    int status, pid, ret, exit_status;
    exit_status = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ci_debug_printf(5, "Child %d died ...\n", pid);
        if (!WIFEXITED(status)) {
            ci_debug_printf(1, "Child %d did not exit normally.", pid);
            exit_status = 1;
            if (WIFSIGNALED(status))
                ci_debug_printf(1, "signaled with signal: %d\n",
                                WTERMSIG(status));
        }
        ret = remove_child(childs_queue, pid, exit_status);
        if (ret == 0 && old_childs_queue) {
            ci_debug_printf(5,
                            "Child %d will be removed from the old list ...\n",
                            pid);
            remove_child(old_childs_queue, pid, exit_status);
            if (childs_queue_is_empty(old_childs_queue)) {
                if (!destroy_childs_queue(old_childs_queue)) {
                    ci_debug_printf(1, "WARNING: can not release unused shared mem block after reconfigure\n");
                }
                old_childs_queue = NULL;
            }
        }
        commands_exec_child_cleanup((process_pid_t)pid, exit_status);
    }
    if (pid < 0)
        ci_debug_printf(1, "Fatal error waiting for a child to exit .....\n");
}

int system_reconfigure();
static int server_reconfigure()
{
    int i;
    if (old_childs_queue) {
        ci_debug_printf(1,
                        "A reconfigure pending. Ignoring reconfigure request.....\n");
        return 1;
    }

    /*shutdown all modules and services and reopen config file */
    if (!system_reconfigure())
        return 0;

    /*initialize commands table for server */
    init_commands();

    /*
       Mark all existing childs as to_be_killed gracefully
       (childs_queue.childs[child_indx].to_be_killed = GRACEFULLY)
     */
    for (i = 0; i < childs_queue->size; i++) {
        if (childs_queue->childs[i].pid != 0) {
            childs_queue->childs[i].father_said = GRACEFULLY;
            kill(childs_queue->childs[i].pid, SIGTERM);
        }
    }

    /*
       Create new shared mem for childs queue
     */
    old_childs_queue = childs_queue;
    if (!(childs_queue = create_childs_queue(2 * CI_CONF.MAX_SERVERS))) {
        ci_debug_printf(1,
                        "Cannot init shared memory. Fatal error, exiting!\n");
        return 0;              /*It is not enough. We must wait all childs to exit ..... */
    }
    /*
       Start new childs to handle new requests.
     */
    if (CI_CONF.START_SERVERS > CI_CONF.MAX_SERVERS)
        CI_CONF.START_SERVERS = CI_CONF.MAX_SERVERS;

    for (i = 0; i < CI_CONF.START_SERVERS; i++) {
        start_child();
    }

    /*
       When all childs exits release the old shared mem block....
     */
    return 1;
}

/*************************************************************************************/
/*Functions for handling commands                                                    */

/*
  I must develop an api for pipes (named and anonymous) under the os/unix and
  os/win32 directories
*/
int ci_named_pipe_create(char *name)
{
    int status, pipe;
    errno = 0;
    status = mkfifo(name, S_IRUSR | S_IWUSR | S_IWGRP);
    if (status < 0 && errno != EEXIST)
        return -1;
    pipe = open(name, O_RDWR | O_NONBLOCK);
    return pipe;
}

int ci_named_pipe_open(char *name)
{
    int pipe;
    pipe = open(name, O_RDWR | O_NONBLOCK);
    return pipe;
}

void ci_named_pipe_close(int pipe_fd)
{
    close(pipe_fd);
}

int wait_for_commands(int ctl_fd, char *command_buffer, int secs)
{
    int ret = 0;

#if defined(USE_POLL)
    struct pollfd pfds[1];
    secs = secs > 0 ? secs * 1000 : -1;
    pfds[0].fd = ctl_fd;
    pfds[0].events = POLLIN;
    if ((ret = poll(pfds, 1, secs)) > 0) {
        if (pfds[0].revents & (POLLERR | POLLNVAL))
            ret = -1;
    }
#else
    struct timeval tv;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctl_fd, &fds);
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    if ((ret = select(ctl_fd + 1, &fds, NULL, NULL, (secs >= 0 ? &tv : NULL))) > 0) {
        if (!FD_ISSET(ctl_fd, &fds))
            ret = 0;
    }
#endif

    if (ret > 0) {
        ret = ci_read_nonblock(ctl_fd, command_buffer, COMMANDS_BUFFER_SIZE - 1);
        if (ret > 0) {
            command_buffer[ret] = '\0';
            return ret;
        }
        if (ret == 0 ) {
            /*read return 0, This is an eof, we must return -1 to force socket reopening!*/
            return -1;
        }
    }
    if (ret < 0 && errno != EINTR) {
        ci_debug_printf(1,
                        "Unexpected error waiting for or reading  events in control socket!\n");
        /*returning -1 we are causing reopening control socket! */
        return -1;
    }

    return 0; /*expired */
}

void handle_monitor_process_commands(char *cmd_line)
{
    ci_command_t *command;
    int i, bytes;
    if ((command = find_command(cmd_line)) != NULL) {
        if (command->type & MONITOR_PROC_CMD)
            execute_command(command, cmd_line, MONITOR_PROC_CMD);
        if (command->type & CHILDS_PROC_CMD) {
            for (i = 0; i < childs_queue->size; i++) {
                do {
                    bytes = write(childs_queue->childs[i].pipe, cmd_line,
                                  strlen(cmd_line));
                } while (bytes < 0 && errno == EINTR);
            }
        }
        if (command->type & MONITOR_PROC_POST_CMD)
            execute_command(command, cmd_line, MONITOR_PROC_POST_CMD);
    }
}

void handle_child_process_commands(char *cmd_line)
{
    ci_command_t *command;
    if ((command = find_command(cmd_line)) != NULL) {
        if (command->type & CHILDS_PROC_CMD)
            execute_command(command, cmd_line, CHILDS_PROC_CMD);
    }
}


/*************************************************************************************/
/*Children  functions                                                                  */

server_decl_t *newthread(struct connections_queue *con_queue)
{
    server_decl_t *serv;
    serv = (server_decl_t *) malloc(sizeof(server_decl_t));
    serv->srv_id = 0;
    serv->con_queue = con_queue;
    serv->served_requests = 0;
    serv->served_requests_no_reallocation = 0;
    serv->current_req = NULL;
    serv->running = 1;

    return serv;
}

int thread_main(server_decl_t * srv)
{
    ci_connection_t con;
    char clientname[CI_MAXHOSTNAMELEN + 1];
    int ret, request_status = CI_NO_STATUS;
    int keepalive_reqs;
//***********************
    thread_signals(0);
//*************************
    srv->srv_id = getpid();    //Setting my pid ...

    for (;;) {
        /*
           If we must shutdown IMEDIATELLY it is time to leave the server
           else if we are going to shutdown GRACEFULLY we are going to die
           only if there are not any accepted connections
         */
        if (child_data->to_be_killed == IMMEDIATELY) {
            srv->running = 0;
            return 1;
        }

        if ((ret = get_from_queue(con_queue, &con)) == 0) {
            if (child_data->to_be_killed) {
                srv->running = 0;
                return 1;
            }
            ret = wait_for_queue(con_queue);
            if (ret >= 0)
                continue;
        }

        if (ret < 0) {        //An error has occured
            ci_debug_printf(1,
                            "Fatal Error!!! Error getting a connection from connections queue!!!\n");
            break;
        }

        ci_thread_mutex_lock(&counters_mtx);  /*Update counters as soon as possible */
        (child_data->freeservers)--;
        (child_data->usedservers)++;
        ci_thread_mutex_unlock(&counters_mtx);

        ret = 1;
        if (srv->current_req == NULL)
            srv->current_req = newrequest(&con);
        else
            ret = recycle_request(srv->current_req, &con);

        if (srv->current_req == NULL || ret == 0) {
            ci_sockaddr_t_to_host(&(con.claddr), clientname,
                                  CI_MAXHOSTNAMELEN);
            ci_debug_printf(1, "Request from %s denied...\n", clientname);
            ci_connection_hard_close(&con);
            goto end_of_main_loop_thread;    /*The request rejected. Log an error and continue ... */
        }

        keepalive_reqs = 0;
        do {
            if (MAX_KEEPALIVE_REQUESTS > 0
                    && keepalive_reqs >= MAX_KEEPALIVE_REQUESTS)
                srv->current_req->keepalive = 0;    /*do not keep alive connection */
            if (child_data->to_be_killed)    /*We are going to die do not keep-alive */
                srv->current_req->keepalive = 0;

            if ((request_status = process_request(srv->current_req)) == CI_NO_STATUS) {
                ci_debug_printf(5, "connection closed or request timed-out or request interrupted....\n");
                ci_connection_hard_close(srv->current_req->connection);
                ci_request_reset(srv->current_req);
                break;
            }
            srv->served_requests++;
            srv->served_requests_no_reallocation++;
            keepalive_reqs++;

            /*Increase served requests. I dont like this. The delay is small but I don't like... */
            ci_thread_mutex_lock(&counters_mtx);
            (child_data->requests)++;
            ci_thread_mutex_unlock(&counters_mtx);

            log_access(srv->current_req, request_status);
//             break; //No keep-alive ......

            if (child_data->to_be_killed == IMMEDIATELY)
                break;      //Just exiting the keep-alive loop

            /*if we are going to term gracefully we will try to keep our promice for
              keepalived request....
             */
            if (child_data->to_be_killed == GRACEFULLY &&
                    srv->current_req->keepalive == 0)
                break;

            ci_debug_printf(8, "Keep-alive:%d\n",
                            srv->current_req->keepalive);
            if (srv->current_req->keepalive && keepalive_request(srv->current_req)) {
                ci_debug_printf(8,
                                "Server %d going to serve new request from client (keep-alive) \n",
                                srv->srv_id);
            } else
                break;
        } while (1);

        if (srv->current_req) {
            if (request_status != CI_OK || child_data->to_be_killed) {
                ci_connection_hard_close(srv->current_req->connection);
            } else {
                ci_connection_linger_close(srv->current_req->connection, MAX_SECS_TO_LINGER);
            }
        }
        if (srv->served_requests_no_reallocation >
                MAX_REQUESTS_BEFORE_REALLOCATE_MEM) {
            ci_debug_printf(5,
                            "Max requests reached, reallocate memory and buffers .....\n");
            ci_request_destroy(srv->current_req);
            srv->current_req = NULL;
            srv->served_requests_no_reallocation = 0;
        }


end_of_main_loop_thread:
        ci_thread_mutex_lock(&counters_mtx);
        (child_data->freeservers)++;
        (child_data->usedservers)--;
        ci_thread_mutex_unlock(&counters_mtx);
        ci_thread_cond_signal(&free_server_cond);

    }
    srv->running = 0;
    return 0;
}

void listener_thread(void *unused)
{
    ci_connection_t conn;
    ci_port_t *port;
    int haschild = 1, jobs_in_queue = 0;
    int pid;
    thread_signals(1);
    /*Wait main process to signal us to start accepting requests*/
    ci_thread_mutex_lock(&counters_mtx);
    listener_running = 1;
    ci_thread_cond_wait(&free_server_cond, &counters_mtx);
    ci_thread_mutex_unlock(&counters_mtx);
    pid = getpid();
    for (;;) {                 //Global for
        if (child_data->to_be_killed) {
            ci_debug_printf(5, "Listener of pid: %d exiting!\n", pid);
            goto LISTENER_FAILS_UNLOCKED;
        }
        if (!ci_proc_mutex_lock(&accept_mutex)) {
            if (errno == EINTR) {
                ci_debug_printf(5,
                                "proc_mutex_lock interrupted (EINTR received, pid=%d)!\n",
                                pid);
                /*Try again to take the lock */
                continue;
            } else {
                ci_debug_printf(1,
                                "Unknown errno %d in proc_mutex_lock of pid %d. Exiting!\n",
                                errno, pid);
                goto LISTENER_FAILS_UNLOCKED;
            }
        }
        child_data->idle = 0;
        ci_debug_printf(7, "Child %d getting requests now ...\n", pid);
        do {                  //Getting requests while we have free servers.....

#if defined(USE_POLL)
            struct pollfd pfds[1024];
            assert(CI_CONF.PORTS->count < 1024);
#else
            fd_set fds;
#endif
            int i;
            do {
                int ret;
                errno = 0;
#if defined(USE_POLL)
                for (i = 0; (port = (ci_port_t *)ci_vector_get(CI_CONF.PORTS, i)) != NULL; ++i) {
                    pfds[i].fd = port->accept_socket;
                    pfds[i].events = POLLIN;
                }
                ret = poll(pfds, i, -1);
#else
                int max_fd = 0;
                FD_ZERO(&fds);
                for (i = 0; (port = (ci_port_t *)ci_vector_get(CI_CONF.PORTS, i)) != NULL; ++i) {
                    if (port->accept_socket > max_fd) max_fd = port->accept_socket;
                    FD_SET(port->accept_socket, &fds);
                }
                ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
#endif
                if (ret < 0) {
                    if (errno != EINTR) {
                        ci_debug_printf(1,
                                        "Error in select %d! Exiting server!\n",
                                        errno);
                        goto LISTENER_FAILS;
                    }
                    if (child_data->to_be_killed) {
                        ci_debug_printf(5,
                                        "Listener server signalled to exit!\n");
                        goto LISTENER_FAILS;
                    }
                }
            } while (errno == EINTR);

            for (i = 0; (port = (ci_port_t *)ci_vector_get(CI_CONF.PORTS, i)) != NULL; ++i) {
#if defined(USE_POLL)
                if (!(pfds[i].revents & POLLIN))
                    continue;
#else
                if (!FD_ISSET(port->accept_socket, &fds))
                    continue;
#endif
                int ret = 0;
                do {
                    ci_connection_reset(&conn);
#ifdef USE_OPENSSL
                    if (port->tls_accept_details)
                        ret = icap_accept_tls_connection(port, &conn);
                    else
#endif
                        ret = icap_accept_raw_connection(port, &conn);
                    if (ret <= 0) {
                        if (child_data->to_be_killed) {
                            ci_debug_printf(5, "Accept aborted: listener server signalled to exit!\n");
                            goto LISTENER_FAILS;
                        } else if (ret == -2) {
                            ci_debug_printf(1, "Fatal error while accepting!\n");
                            goto LISTENER_FAILS;
                        } /*else ret is -1 for aborted, or zero for EINTR*/
                    }
                } while (ret == 0);

                // Probably ECONNABORTED or similar error
                if (!ci_socket_valid(conn.fd))
                    continue;

                /*Do w need the following? Options has been set in icap_init_server*/
                icap_socket_opts(port->accept_socket, MAX_SECS_TO_LINGER);

                if ((jobs_in_queue = put_to_queue(con_queue, &conn)) == 0) {
                    ci_debug_printf(1,
                                    "ERROR!!!!!! NO AVAILABLE SERVERS! THIS IS A BUG!!!!!!!!\n");
                    ci_debug_printf(1,
                                    "Jobs in Queue: %d, Free servers: %d, Used Servers: %d, Requests: %d\n",
                                    jobs_in_queue, child_data->freeservers,
                                    child_data->usedservers,
                                    child_data->requests);
                    goto LISTENER_FAILS;
                }
                (child_data->connections)++;     //NUM of Requests....
            } /*for (Listen_SOCKETS[i]...*/

            if (child_data->to_be_killed) {
                ci_debug_printf(5, "Listener server must exit!\n");
                goto LISTENER_FAILS;
            }
            ci_thread_mutex_lock(&counters_mtx);
            haschild =
                ((child_data->freeservers - jobs_in_queue) > 0 ? 1 : 0);
            ci_thread_mutex_unlock(&counters_mtx);
        } while (haschild);
        ci_debug_printf(7, "Child %d STOPS getting requests now ...\n", pid);
        child_data->idle = 1;
        while (!ci_proc_mutex_unlock(&accept_mutex)) {
            if (errno != EINTR) {
                ci_debug_printf(1,
                                "Error:%d while trying to unlock proc_mutex, exiting listener of server:%d\n",
                                errno, pid);
                goto LISTENER_FAILS_UNLOCKED;
            }
            ci_debug_printf(5,
                            "Mutex lock interrupted while trying to unlock proc_mutex, pid: %d\n",
                            pid);
        }

        ci_thread_mutex_lock(&counters_mtx);
        if ((child_data->freeservers - connections_pending(con_queue)) <= 0) {
            ci_debug_printf(7,
                            "Child %d waiting for a thread to accept more connections ...\n",
                            pid);
            ci_thread_cond_wait(&free_server_cond, &counters_mtx);
        }
        ci_thread_mutex_unlock(&counters_mtx);
    }
LISTENER_FAILS_UNLOCKED:
    /* The code commented out, because causes closing TLS listening ports
       for all of the kids and parent.
    ci_port_list_release(CI_CONF.PORTS);
    CI_CONF.PORTS = NULL;*/
    listener_running = 0;
    return;

LISTENER_FAILS:
    /* The code commented out, because causes closing TLS listening ports
       for all of the kids and parent.
    ci_port_list_release(CI_CONF.PORTS);
    CI_CONF.PORTS = NULL*/
    listener_running = 0;
    errno = 0;
    while (!ci_proc_mutex_unlock(&accept_mutex)) {
        if (errno != EINTR) {
            ci_debug_printf(1,
                            "Error:%d while trying to unlock proc_mutex of server:%d\n",
                            errno, pid);
            break;
        }
        ci_debug_printf(7,
                        "Mutex lock interrupted while trying to unlock proc_mutex before terminating\n");
    }
    return;
}

void child_main(int pipefd)
{
    ci_thread_t thread;
    int i, ret;

    signal(SIGTERM, SIG_IGN);  /*Ignore parent requests to kill us untill we are up and running */
    ci_thread_mutex_init(&threads_list_mtx);
    ci_thread_mutex_init(&counters_mtx);
    ci_thread_cond_init(&free_server_cond);

    ci_stat_attach_mem(child_data->stats, child_data->stats_size, NULL);

    threads_list =
        (server_decl_t **) malloc((CI_CONF.THREADS_PER_CHILD + 1) *
                                  sizeof(server_decl_t *));
    con_queue = init_queue(CI_CONF.THREADS_PER_CHILD);

    for (i = 0; i < CI_CONF.THREADS_PER_CHILD; i++) {
        if ((threads_list[i] = newthread(con_queue)) == NULL) {
            exit(-1);        // FATAL error.....
        }
        ret =
            ci_thread_create(&thread, (void *(*)(void *)) thread_main,
                             (void *) threads_list[i]);
        if (ret == 0)
            threads_list[i]->srv_pthread = thread;
    }
    threads_list[CI_CONF.THREADS_PER_CHILD] = NULL;
    /*Now start the listener thread.... */
    ret = ci_thread_create(&thread, (void *(*)(void *)) listener_thread,
                           NULL);
    if (ret == 0)
        listener_thread_id = thread;

    /*set srand for child......*/
    srand(((unsigned int)time(NULL)) + (unsigned int)getpid());
    /*I suppose that all my threads are up now. We can setup our signal handlers */
    child_signals();

    /* A signal from parent may comes while we are starting.
       Listener will not accept any request in this case, (it checks on
       the beggining of the accept loop for parent commands) so we can
       shutdown imediatelly even if the parent said gracefuly.*/
    if (child_data->father_said)
        child_data->to_be_killed = IMMEDIATELY;

    /*start child commands may have non thread safe code but the worker threads
      does not serving requests yet.*/
    commands_execute_start_child();

    /*Signal listener to start accepting requests.*/
    int doStart = 0;
    do {
        ci_thread_mutex_lock(&counters_mtx);
        doStart = listener_running;
        ci_thread_mutex_unlock(&counters_mtx);
        if (!doStart)
            ci_usleep(5);
    } while (!doStart);
    ci_thread_cond_signal(&free_server_cond);

    while (!child_data->to_be_killed) {
        char buf[512];
        int bytes;
        ret = ci_wait_for_data(pipefd, 1, ci_wait_for_read);
        if (ret < 0) {
            ci_debug_printf(1,
                            "An error occured while waiting for commands from parent. Terminating!\n");
            child_data->to_be_killed = IMMEDIATELY;
        } else if (ret & ci_wait_for_read) { /*data input */
            bytes = ci_read_nonblock(pipefd, buf, 511);
            if (bytes <= 0) {
                ci_debug_printf(1,
                                "Parent closed the pipe connection! Going to term immediately!\n");
                child_data->to_be_killed = IMMEDIATELY;
            } else {
                buf[bytes] = '\0';
                handle_child_process_commands(buf);
            }
        }

        if (!listener_running && !child_data->to_be_killed) {
            ci_debug_printf(1,
                            "Ohh!! something happened to listener thread! Terminating\n");
            child_data->to_be_killed = GRACEFULLY;
        }
        commands_exec_scheduled(CI_CMD_ONDEMAND);
    }

    ci_debug_printf(5, "Child :%d going down :%s\n", getpid(),
                    child_data->to_be_killed == IMMEDIATELY?
                    "IMMEDIATELY" : "GRACEFULLY");

    cancel_all_threads();
    commands_execute_stop_child();
    exit_normaly();
}


/*****************************************************************************************/
/*Main process functions                                                                 */

int start_child()
{
    int pid;
    int pfd[2];
    int children_num, free_servers, used_servers, max_requests;

    if (pipe(pfd) < 0) {
        ci_debug_printf(1,
                        "Error creating pipe for communication with child\n");
        return -1;
    }
    if (fcntl(pfd[0], F_SETFL, O_NONBLOCK) < 0
            || fcntl(pfd[1], F_SETFL, O_NONBLOCK) < 0) {
        ci_debug_printf(1, "Error making the child pipe non-blocking\n");
        close(pfd[0]);
        close(pfd[1]);
    }
    if ((pid = fork()) == 0) { //A Child .......
        MY_PROC_PID = getpid();
        if (!attach_childs_queue(childs_queue)) {
            ci_debug_printf(1, "Can not access shared memory for %d child\n", (int)MY_PROC_PID);
            exit(-2);
        }
        child_data =
            register_child(childs_queue, getpid(), CI_CONF.THREADS_PER_CHILD, pfd[1]);
        if (!child_data) {
            childs_queue_stats(childs_queue, &children_num, &free_servers,
                               &used_servers, &max_requests);
            ci_debug_printf(1, "Not available slot in shared memory for %d child (Number of slots: %d, Running children: %d, Free servers: %d, Used servers: %d)!\n", (int)MY_PROC_PID,
                            childs_queue->size,
                            children_num,
                            free_servers,
                            used_servers
                           );
            exit(-3);
        }
        close(pfd[1]);
        child_main(pfd[0]);
        exit(0);
    } else {
        close(pfd[0]);
        announce_child(childs_queue, pid);
        return pid;
    }
}

void stop_command(const char *name, int type, const char **argv)
{
    c_icap_going_to_term = 1;
}

void reconfigure_command(const char *name, int type, const char **argv)
{
    if (type == MONITOR_PROC_CMD)
        c_icap_reconfigure = 1;
    //server_reconfigure();
}

void dump_statistics_command(const char *name, int type, const char **argv)
{
    if (type == MONITOR_PROC_CMD)
        dump_queue_statistics(childs_queue);
}

void test_command(const char *name, int type, const char **argv)
{
    int i = 0;
    ci_debug_printf(1, "Test command for %s. Arguments:",
                    (type ==
                     MONITOR_PROC_CMD ? "monitor process" : "child process"));
    while (argv[i] != NULL) {
        ci_debug_printf(1, "%s,", argv[i]);
        i++;
    }
    ci_debug_printf(1, "\n");
}

int init_server()
{
    int i;
    ci_port_t *p;

    if (!CI_CONF.PORTS) {
        ci_debug_printf(1, "No ports configured!\n");
        return 0;
    }

#ifdef USE_OPENSSL
    if (CI_CONF.TLS_ENABLED) {
        ci_tls_init();
        ci_tls_set_passphrase_script(CI_CONF.TLS_PASSPHRASE);
    }
#endif

    for (i = 0; (p = (ci_port_t *)ci_vector_get(CI_CONF.PORTS, i)); ++i) {
        if (p->configured)
            continue;

#ifdef USE_OPENSSL
        if (p->tls_enabled) {
            if (!icap_init_server_tls(p))
                return 0;
        } else
#endif
            if (CI_SOCKET_INVALID == icap_init_server(p))
                return 0;
        p->configured = 1;
    }

    return 1;
}

void init_commands()
{
    register_command("stop", MONITOR_PROC_CMD, stop_command);
    register_command("reconfigure", MONITOR_PROC_CMD, reconfigure_command);
    register_command("dump_statistics", MONITOR_PROC_CMD, dump_statistics_command);
    register_command("test", MONITOR_PROC_CMD | CHILDS_PROC_CMD, test_command);
}

int start_server()
{
    int child_indx, pid, i, ctl_socket;
    int childs, freeservers, used, maxrequests, ret;
    char command_buffer[COMMANDS_BUFFER_SIZE];
    int user_informed = 0;

    ctl_socket = ci_named_pipe_create(CI_CONF.COMMANDS_SOCKET);
    if (ctl_socket < 0) {
        ci_debug_printf(1,
                        "Error opening control socket %s: %s. Fatal error, exiting!\n",
                        strerror(errno),
                        CI_CONF.COMMANDS_SOCKET);
        exit(0);
    }

    if (!ci_proc_mutex_init(&accept_mutex, "accept")) {
        ci_debug_printf(1,
                        "Can't init mutex for accepting conenctions. Fatal error, exiting!\n");
        exit(0);
    }
    if (!(childs_queue = create_childs_queue(2 * CI_CONF.MAX_SERVERS))) {
        ci_proc_mutex_destroy(&accept_mutex);
        ci_debug_printf(1,
                        "Can't init shared memory. Fatal error, exiting!\n");
        exit(0);
    }

    init_commands();
    pid = 1;
#ifdef MULTICHILD
    if (CI_CONF.START_SERVERS > CI_CONF.MAX_SERVERS)
        CI_CONF.START_SERVERS = CI_CONF.MAX_SERVERS;

    for (i = 0; i < CI_CONF.START_SERVERS; i++) {
        if (pid)
            pid = start_child();
    }
    if (pid != 0) {
        main_signals();

        execute_commands_no_lock(CI_CMD_MONITOR_START);
        while (1) {
            if ((ret = wait_for_commands(ctl_socket, command_buffer, 1)) > 0) {
                ci_debug_printf(5, "I received the command: %s\n",
                                command_buffer);
                handle_monitor_process_commands(command_buffer);
            }
            if (ret < 0) {  /*Eof received on pipe. Going to reopen ... */
                ci_named_pipe_close(ctl_socket);
                ctl_socket = ci_named_pipe_open(CI_CONF.COMMANDS_SOCKET);
                if (ctl_socket < 0) {
                    ci_debug_printf(1,
                                    "Error opening control socket. We are unstable and going down!");
                    c_icap_going_to_term = 1;
                }
            }

            if (c_icap_going_to_term)
                break;
            childs_queue_stats(childs_queue, &childs, &freeservers, &used,
                               &maxrequests);
            ci_debug_printf(10,
                            "Server stats: \n\t Children: %d\n\t Free servers: %d\n"
                            "\tUsed servers:%d\n\tRequests served:%d\n",
                            childs, freeservers, used, maxrequests);
            if (MAX_REQUESTS_PER_CHILD > 0 && (child_indx =
                                                   find_a_child_nrequests
                                                   (childs_queue,
                                                    MAX_REQUESTS_PER_CHILD)) >=
                    0) {
                ci_debug_printf(8,
                                "Max requests reached for child :%d of pid :%d\n",
                                child_indx,
                                childs_queue->childs[child_indx].pid);
                pid = start_child();
                //         usleep(500);
                childs_queue->childs[child_indx].father_said = GRACEFULLY;
                /*kill a server ... */
                kill(childs_queue->childs[child_indx].pid, SIGTERM);

            } else if ((freeservers <= CI_CONF.MIN_SPARE_THREADS && childs < CI_CONF.MAX_SERVERS)
                       || childs < CI_CONF.START_SERVERS) {
                ci_debug_printf(8,
                                "Free Servers: %d, children: %d. Going to start a child .....\n",
                                freeservers, childs);
                pid = start_child();
            } else if (freeservers >= CI_CONF.MAX_SPARE_THREADS &&
                       childs > CI_CONF.START_SERVERS &&
                       (freeservers - CI_CONF.THREADS_PER_CHILD) > CI_CONF.MIN_SPARE_THREADS) {

                if ((child_indx = find_an_idle_child(childs_queue)) >= 0) {
                    childs_queue->childs[child_indx].father_said =
                        GRACEFULLY;
                    ci_debug_printf(8,
                                    "Free Servers: %d, children: %d. Going to stop child %d(index: %d)\n",
                                    freeservers, childs,
                                    childs_queue->childs[child_indx].pid,
                                    child_indx);
                    /*kill a server ... */
                    kill(childs_queue->childs[child_indx].pid, SIGTERM);
                    user_informed = 0;
                }
            } else if (childs == CI_CONF.MAX_SERVERS && freeservers < CI_CONF.MIN_SPARE_THREADS) {
                if (! user_informed) {
                    ci_debug_printf(1,
                                    "ATTENTION!!!! Not enough available servers (children %d, free servers %d, used servers %d)!!!!! "
                                    "Maybe you should increase the MaxServers and the "
                                    "ThreadsPerChild values in c-icap.conf file!!!!!!!!!", childs, freeservers, used);
                    user_informed = 1;
                }
            }
            if (c_icap_going_to_term)
                break;
            check_for_exited_childs();
            if (c_icap_reconfigure) {
                c_icap_reconfigure = 0;
                if (!server_reconfigure()) {
                    ci_debug_printf(1, "Error while reconfiguring, exiting!\n");
                    break;
                }
            }
            commands_exec_scheduled(CI_CMD_MONITOR_ONDEMAND);
        }
        /*Main process exit point */
        ci_debug_printf(1,
                        "Possibly a term signal received. Monitor process going to term all children\n");
        kill_all_childs();
        system_shutdown();
        ci_port_list_release(CI_CONF.PORTS);
        CI_CONF.PORTS = NULL;
        ci_debug_printf(1, "Exiting....\n");
        return 1;
    }
#else
    child_data = (child_shared_data_t *) malloc(sizeof(child_shared_data_t));
    child_data->pid = 0;
    child_data->freeservers = CI_CONF.THREADS_PER_CHILD;
    child_data->usedservers = 0;
    child_data->requests = 0;
    child_data->connections = 0;
    child_data->to_be_killed = 0;
    child_data->father_said = 0;
    child_data->idle = 1;
    child_data->stats_size = ci_stat_memblock_size();
    child_data->stats = malloc(child_data->stats_size);
    child_data->stats->sig = MEMBLOCK_SIG;
    ci_stat_attach_mem(child_data->stats, child_data->stats_size, NULL);
    child_main(0);
    ci_proc_mutex_destroy(&accept_mutex);
    destroy_childs_queue(childs_queue);
    childs_queue = NULL;
#endif
    return 1;
}
