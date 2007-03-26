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
#include "net_io.h"
#include "proc_mutex.h"
#include "debug.h"
#include "log.h"
#include "request.h"
#include "ci_threads.h"
#include "proc_threads_queues.h"
#include "cfg_param.h"
#include "commands.h"

extern int KEEPALIVE_TIMEOUT;
extern int MAX_KEEPALIVE_REQUESTS;
extern int MAX_SECS_TO_LINGER;
extern int START_CHILDS;
extern int MAX_CHILDS;
extern int START_SERVERS;
extern int MIN_FREE_SERVERS;
extern int MAX_FREE_SERVERS;
extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;
extern int MAX_REQUESTS_PER_CHILD;
extern struct icap_server_conf CONF;

typedef struct server_decl {
     int srv_id;
     ci_thread_t srv_pthread;
     struct connections_queue *con_queue;
     request_t *current_req;
     int served_requests;
     int served_requests_no_reallocation;
} server_decl_t;

#undef SINGLE_ACCEPT
ci_thread_mutex_t threads_list_mtx;
server_decl_t **threads_list = NULL;
ci_thread_t listener_thread_id = -1;
int listener_running = 0;

ci_thread_cond_t free_server_cond;
ci_thread_mutex_t counters_mtx;

struct childs_queue *childs_queue;
struct childs_queue *old_childs_queue = NULL;
child_shared_data_t *child_data;
struct connections_queue *con_queue;

/*Interprocess accepting mutex ....*/
ci_proc_mutex_t accept_mutex;

/*Main proccess variables*/
ci_socket LISTEN_SOCKET = -1;
int c_icap_going_to_term = 0;
int c_icap_reconfigure = 0;

#define hard_close_connection(connection)  ci_hard_close(connection->fd)
#define close_connection(connection) ci_linger_close(connection->fd,MAX_SECS_TO_LINGER)
#define check_for_keepalive_data(fd) ci_wait_for_data(fd,KEEPALIVE_TIMEOUT,wait_for_read)
void init_commands();
int init_server(int port, int *family);
int start_child(int fd);
/***************************************************************************************/
/*Signals managment functions                                                          */

static void term_handler_child(int sig)
{
     ci_debug_printf(5, "A termination signal received (%d).\n", sig);
     if (!child_data->father_said)
          child_data->to_be_killed = IMMEDIATELY;
     else
          child_data->to_be_killed = child_data->father_said;
}

static void sigpipe_handler(int sig)
{
     ci_debug_printf(5, "SIGPIPE signal received.\n");
}

static void empty(int sig)
{
     ci_debug_printf(10, "An empty signal handler (%d).\n", sig);
}

static void sigint_handler_main(int sig)
{
     if (sig == SIGTERM) {
          ci_debug_printf(5, "SIGTERM signal received for main server.\n");
     }
     else if (sig == SIGINT) {
          ci_debug_printf(5, "SIGINT signal received for main server.\n");
     }
     else {
          ci_debug_printf(5, "Signal %d received. Exiting ....\n", sig);
     }

     c_icap_going_to_term = 1;
}

static void sigchld_handler_main(int sig)
{
     /*Do nothing the signal will be ignored..... */
}

static void sighup_handler_main()
{
//     c_icap_reconfigure = 1;
    ci_proc_mutex_lock(&accept_mutex);
     ci_debug_printf(5,"SigHUP \n");
     ci_proc_mutex_unlock(&accept_mutex);
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
     int i = 0;
     server_decl_t *srv;
     ci_debug_printf(5, "Suppose that all threads are already exited ...\n");
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
     int wait_listener_time = 10000;

     /*Cancel listener thread .... */
     /*we are going to wait maximum about 10 ms */
     while (wait_listener_time > 0 && listener_running != 0) {
          /*Interrupt listener if it is waiting for threads or 
             waiting to accept new connections */
#ifdef SINGLE_ACCEPT
          pthread_cancel(listener_thread_id);   /*..... */
#else
          pthread_kill(listener_thread_id, SIGHUP);
#endif
          ci_thread_cond_signal(&free_server_cond);
          usleep(1000);
          wait_listener_time -= 10;
     }
     if (listener_running == 0) {
          ci_debug_printf(5,
                          "Going to waiting the listener thread (pid:%d) to exit!\n",
                          threads_list[0]->srv_id);
          ci_thread_join(listener_thread_id);
          ci_debug_printf(5, "OK canceling the listener thread (pid:%d)!\n",
                          threads_list[0]->srv_id);
     }
     else {
          /*fuck the listener! going down ..... */
     }

     /*We are going to interupt the waiting for queue childs.
        We are going to wait childs which serve a request. */
     ci_thread_cond_broadcast(&(con_queue->queue_cond));
     while (threads_list[i] != NULL) {
          ci_debug_printf(5, "Cancel server %d, thread_id %lu (%d)\n",
                          threads_list[i]->srv_id, threads_list[i]->srv_pthread,
                          i);
          ci_thread_join(threads_list[i]->srv_pthread);
          i++;
     }
}

static void kill_all_childs()
{
     int i = 0, status, pid;
     ci_debug_printf(5, "Going to term childs....\n");
     for (i = 0; i < childs_queue->size; i++) {
          if ((pid = childs_queue->childs[i].pid) == 0)
               continue;
          /*listener thread will be interupted it is good to know 
             that it will going down imediatelly */
          childs_queue->childs[i].father_said = IMMEDIATELY;
          kill(pid, SIGTERM);
     }

     for (i = 0; i < childs_queue->size; i++) {
          if (childs_queue->childs[i].pid == 0)
               continue;
          pid = wait(&status);
          ci_debug_printf(5, "Child %d died with status %d\n", pid, status);
     }
     ci_proc_mutex_destroy(&accept_mutex);
     destroy_childs_queue(childs_queue);
}

static void check_for_exited_childs()
{
     int status, pid, ret;
     while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
          ci_debug_printf(5, "The child %d died ...\n", pid);
          if (!WIFEXITED(status)) {
               ci_debug_printf(1, "The child %d not exited normaly.", pid);
               if (WIFSIGNALED(status))
                    ci_debug_printf(1, "signaled with signal:%d\n",
                                    WTERMSIG(status));
          }
          ret = remove_child(childs_queue, pid);
          if (ret == 0 && old_childs_queue) {
               ci_debug_printf(5,
                               "The child %d will be removed from the old list ...\n",
                               pid);
               remove_child(old_childs_queue, pid);
               if (childs_queue_is_empty(old_childs_queue)) {
                    ret = destroy_childs_queue(old_childs_queue);
                    /* if(!ret){} */
                    old_childs_queue = NULL;
               }
          }
     }
     if (pid < 0)
          ci_debug_printf(1, "Fatal error waiting a child to exit .....\n");
}

void system_reconfigure();
static void server_reconfigure()
{
     int i;
     if (old_childs_queue) {
          ci_debug_printf(1,
                          "A reconfigure pending. Ignoring reconfigure request.....\n");
          return;
     }

     /*shutdown all modules and services and reopen config file */
     system_reconfigure();

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
     childs_queue = malloc(sizeof(struct childs_queue));
     if (!create_childs_queue(childs_queue, 2 * MAX_CHILDS)) {
          ci_debug_printf(1,
                          "Can't init shared memory.Fatal error, exiting!\n");
          exit(0);              /*It is not enough. We must wait all childs to exit ..... */
     }
     /*
        Start new childs to handle new requests.
      */
     if (START_CHILDS > MAX_CHILDS)
          START_CHILDS = MAX_CHILDS;

     for (i = 0; i < START_CHILDS; i++) {
          start_child(LISTEN_SOCKET);
     }

     /*
        When all childs exits release the old shared mem block....
      */
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
     status = mkfifo(name, S_IRWXU | S_IRGRP | S_IROTH);
     if (status < 0 && errno != EEXIST)
          return -1;
     pipe = open(name, O_RDONLY | O_NONBLOCK);
     return pipe;
}

int ci_named_pipe_open(char *name)
{
     int pipe;
     pipe = open(name, O_RDONLY | O_NONBLOCK);
     return pipe;
}

void ci_named_pipe_close(int pipe_fd)
{
     close(pipe_fd);
}

int wait_for_commands(int pipe_fd, char *command_buffer, int secs)
{
     fd_set fds;
     struct timeval tv;
     int ret = 0;

     if (secs >= 0) {
          tv.tv_sec = secs;
          tv.tv_usec = 0;
     }
     FD_ZERO(&fds);
     FD_SET(pipe_fd, &fds);
     errno = 0;
     if ((ret =
          select(pipe_fd + 1, &fds, NULL, NULL,
                 (secs >= 0 ? &tv : NULL))) > 0) {
          if (FD_ISSET(pipe_fd, &fds)) {
               ret =
                   ci_read_nonblock(pipe_fd, command_buffer,
                                    COMMANDS_BUFFER_SIZE - 1);
               command_buffer[ret] = '\0';
               return ret;
          }
     }
     if (ret < 0 && errno != EINTR) {
          ci_debug_printf(1,
                          "Unxpected error waiting for events in control socket!\n");
          /*returning 0 we are causing reopening control socket! */
          return 0;
     }

     return -1;                 /*expired */
}

void handle_monitor_process_commands(char *cmd_line)
{
     ci_command_t *command;
     int i;
     if ((command = find_command(cmd_line)) != NULL) {
          if (command->type & MONITOR_PROC_CMD)
               execute_command(command, cmd_line, MONITOR_PROC_CMD);
          if (command->type & CHILDS_PROC_CMD) {
               for (i = 0; i < childs_queue->size; i++) {
                    write(childs_queue->childs[i].pipe, cmd_line,
                          strlen(cmd_line));
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
/*Childs  functions                                                                  */

server_decl_t *newthread(struct connections_queue *con_queue)
{
     server_decl_t *serv;
     serv = (server_decl_t *) malloc(sizeof(server_decl_t));
     serv->srv_id = 0;
//    serv->srv_pthread=pthread_self();
     serv->con_queue = con_queue;
     serv->served_requests = 0;
     serv->served_requests_no_reallocation = 0;
     serv->current_req = NULL;

     return serv;
}

int thread_main(server_decl_t * srv)
{
     ci_connection_t con;
     char clientname[CI_MAXHOSTNAMELEN + 1];
     int ret, request_status = 0;
     int keepalive_reqs;
//***********************
     thread_signals(0);
//*************************
     srv->srv_id = getpid();    //Setting my pid ...
     srv->srv_pthread = pthread_self();

     for (;;) {
          /*
             If we must shutdown IMEDIATELLY it is time to leave the server
             else if we are going to shutdown GRACEFULLY we are going to die 
             only if there are not any accepted connections
           */
          if (child_data->to_be_killed == IMMEDIATELY)
               return 1;

          if ((ret = get_from_queue(con_queue, &con)) == 0) {
               if (child_data->to_be_killed)
                    return 1;
               ret = wait_for_queue(con_queue);
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

          ci_netio_init(con.fd);
          ret = 1;
          if (srv->current_req == NULL)
               srv->current_req = newrequest(&con);
          else
               ret = recycle_request(srv->current_req, &con);

          if (srv->current_req == NULL || ret == 0) {
               ci_sockaddr_t_to_host(&(con.claddr), clientname,
                                     CI_MAXHOSTNAMELEN);
               ci_debug_printf(1, "Request from %s denied...\n", clientname);
               hard_close_connection((&con));
               goto end_of_main_loop_thread;    /*The request rejected. Log an error and continue ... */
          }

          keepalive_reqs = 0;
          do {
               if (MAX_KEEPALIVE_REQUESTS > 0
                   && keepalive_reqs >= MAX_KEEPALIVE_REQUESTS)
                    srv->current_req->keepalive = 0;    /*do not keep alive connection */
               if (child_data->to_be_killed)    /*We are going to die do not keep-alive */
                    srv->current_req->keepalive = 0;

               if ((request_status = process_request(srv->current_req)) < 0) {
                    ci_debug_printf(5,
                                    "Process request timeout or interupted....\n");
                    ci_request_reset(srv->current_req);
                    break;      //
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

               if (child_data->to_be_killed)
                    break;      //Just exiting the keep-alive loop

               ci_debug_printf(8, "Keep-alive:%d\n",
                               srv->current_req->keepalive);
               if (srv->current_req->keepalive
                   && check_for_keepalive_data(srv->current_req->connection->
                                               fd) > 0) {
                    ci_request_reset(srv->current_req);
                    ci_debug_printf(8,
                                    "Server %d going to serve new request from client(keep-alive) \n",
                                    srv->srv_id);
               }
               else
                    break;
          } while (1);

          if (srv->current_req) {
               if (request_status < 0 || child_data->to_be_killed) {
                    hard_close_connection(srv->current_req->connection);
               }
               else {
                    close_connection(srv->current_req->connection);
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
     return 0;
}

void listener_thread(int *fd)
{
     ci_connection_t conn;
     socklen_t claddrlen = sizeof(struct sockaddr_in);
     int haschild = 1, jobs_in_queue = 0;
     int pid, sockfd;
     sockfd = *fd;
     thread_signals(1);
     pid = getpid();
     listener_thread_id = pthread_self();
     for (;;) {                 //Global for
          if (child_data->to_be_killed) {
               ci_debug_printf(5, "Listener of pid:%d exiting!\n", pid);
               goto LISTENER_FAILS;
          }
          if (!ci_proc_mutex_lock(&accept_mutex)) {
               if (errno == EINTR) {
                    ci_debug_printf(5,
                                    "proc_mutex_lock interrupted (EINTR received, pid=%d)!\n",
                                    pid);
                    /*Try again to take the lock */
                    continue;
               }
               else {
                    ci_debug_printf(1,
                                    "Unknown errno:%d in proc_mutex_lock of  pid:%d exiting!\n",
                                    errno, pid);
                    goto LISTENER_FAILS_UNLOCKED;
               }
          }
          child_data->idle = 0;
          ci_debug_printf(7, "Child %d getting requests now ...\n", pid);
          do {                  //Getting requests while we have free servers.....
#ifndef SINGLE_ACCEPT
               fd_set fds;
               int ret;
               do {
                    FD_ZERO(&fds);
                    FD_SET(sockfd, &fds);
                    errno = 0;
                    ret = select(sockfd + 1, &fds, NULL, NULL, NULL);
                    if (ret < 0) {
                         if (errno != EINTR) {
                              ci_debug_printf(1,
                                              "Error in select %d! Exiting server!\n",
                                              errno);
                              goto LISTENER_FAILS;
                         }
                         if (child_data->to_be_killed) {
                              ci_debug_printf(5,
                                              "Listener server signaled to exit!\n");
                              goto LISTENER_FAILS;
                         }
                    }
               } while (errno == EINTR);
#endif
               do {
                    errno = 0;
                    claddrlen = sizeof(conn.claddr.sockaddr);
                    if (((conn.fd =
                          accept(sockfd,
                                 (struct sockaddr *) &(conn.claddr.sockaddr),
                                 &claddrlen)) == -1)) {
                         if (errno != EINTR) {
                              ci_debug_printf(1,
                                              "Error accept %d!\nExiting server!\n",
                                              errno);
                              goto LISTENER_FAILS;
                         }
                         if (child_data->to_be_killed) {
                              ci_debug_printf(5,
                                              "Listener server signaled to exit!\n");
                              goto LISTENER_FAILS;
                         }
                    }
               } while (errno == EINTR);
               claddrlen = sizeof(conn.srvaddr.sockaddr);
               getsockname(conn.fd,
                           (struct sockaddr *) &(conn.srvaddr.sockaddr),
                           &claddrlen);
               ci_fill_sockaddr(&conn.claddr);
               ci_fill_sockaddr(&conn.srvaddr);

               icap_socket_opts(sockfd, MAX_SECS_TO_LINGER);

               if ((jobs_in_queue = put_to_queue(con_queue, &conn)) == 0) {
                    ci_debug_printf(1,
                                    "ERROR!!!!!!NO AVAILABLE SERVERS!THIS IS A BUG!!!!!!!!\n");
                    ci_debug_printf(1,
                                    "Jobs in Queue:%d,Free servers:%d, Used Servers :%d, Requests %d\n",
                                    jobs_in_queue, child_data->freeservers,
                                    child_data->usedservers,
                                    child_data->requests);
                    goto LISTENER_FAILS;
               }
               ci_thread_mutex_lock(&counters_mtx);
               haschild =
                   ((child_data->freeservers - jobs_in_queue) > 0 ? 1 : 0);
               ci_thread_mutex_unlock(&counters_mtx);
               (child_data->connections)++;     //NUM of Requests....
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
                               "Mutex lock interupted while trying to unlock proc_mutex, pid:%d\n",
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
     listener_running = 0;
     return;

   LISTENER_FAILS:
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
                          "Mutex lock interupted while trying to unlock proc_mutex before terminating\n");
     }
     return;
}

void child_main(int sockfd, int pipefd)
{
     ci_thread_t thread;
     int i, ret;

     child_signals();
     ci_thread_mutex_init(&threads_list_mtx);
     ci_thread_mutex_init(&counters_mtx);
     ci_thread_cond_init(&free_server_cond);


     threads_list =
         (server_decl_t **) malloc((START_SERVERS + 1) *
                                   sizeof(server_decl_t *));
     con_queue = init_queue(START_SERVERS);

     for (i = 0; i < START_SERVERS; i++) {
          if ((threads_list[i] = newthread(con_queue)) == NULL) {
               exit(-1);        // FATAL error.....
          }
          ret =
              ci_thread_create(&thread, (void *(*)(void *)) thread_main,
                               (void *) threads_list[i]);
     }
     threads_list[START_SERVERS] = NULL;
     /*Now start the listener thread.... */
     ret = ci_thread_create(&thread, (void *(*)(void *)) listener_thread,
                            (void *) &sockfd);
     listener_running = 1;

     for (;;) {
          char buf[512];
          int bytes;
          if ((ret = ci_wait_for_data(pipefd, 5, wait_for_read)) > 0) { /*data input */
               bytes = ci_read_nonblock(pipefd, buf, 511);
               if (bytes == 0) {
                    ci_debug_printf(1,
                                    "Parent closes the pipe connection! Going to term immediately!\n");
                    child_data->to_be_killed = IMMEDIATELY;
                    break;
               }
               buf[bytes] = '\0';
               handle_child_process_commands(buf);
//               ci_debug_printf(1, "Coming data: %s\n", buf);
          }
          else if (ret < 0) {
               ci_debug_printf(1,
                               "An error occured while waiting commands from parent. Terminating!\n");
               child_data->to_be_killed = IMMEDIATELY;
          }
          if (!listener_running) {
               ci_debug_printf(1,
                               "Ohh!! something happen to listener thread! Terminating\n");
               child_data->to_be_killed = GRACEFULLY;
          }
/*
          if (MAX_REQUESTS_PER_CHILD > 0
              && child_data->requests > MAX_REQUESTS_PER_CHILD) {
               ci_debug_printf(5,
                               "Maximum number of requests reached.Going down\n");
               child_data->to_be_killed = GRACEFULLY;
          }
*/
          if (child_data->to_be_killed)
               break;
//          ci_debug_printf(5, "Do some tests\n");
     }

     if (child_data->to_be_killed == IMMEDIATELY)
          exit(0);
     /*Else we are going to exit gracefully */
     ci_debug_printf(5, "Child :%d going down :%d\n", getpid(),
                     child_data->to_be_killed);
     cancel_all_threads();
     exit_normaly();
}


/*****************************************************************************************/
/*Main process functions                                                                 */

#define MULTICHILD
//#undef MULTICHILD

int start_child(int fd)
{
     int pid;
     int pfd[2];

     if (pipe(pfd) < 0) {
          ci_debug_printf(1,
                          "Error creating pipe for comunication with child\n");
          return -1;
     }
     if (fcntl(pfd[0], F_SETFL, O_NONBLOCK) < 0
         || fcntl(pfd[1], F_SETFL, O_NONBLOCK) < 0) {
          ci_debug_printf(1, "Error making nonblocking the child pipe\n");
          close(pfd[0]);
          close(pfd[1]);
     }
     if ((pid = fork()) == 0) { //A Child .......
          attach_childs_queue(childs_queue);
          child_data =
              register_child(childs_queue, getpid(), START_SERVERS, pfd[1]);
          close(pfd[1]);
          child_main(fd, pfd[0]);
          exit(0);
     }
     else {
          close(pfd[0]);
          return pid;
     }
}

void stop_command(char *name, int type, char **argv)
{
     c_icap_going_to_term = 1;
}

void reconfigure_command(char *name, int type, char **argv)
{
     if (type == MONITOR_PROC_CMD)
          server_reconfigure();
}

void test_command(char *name, int type, char **argv)
{
     int i = 0;
     ci_debug_printf(1, "Test command for %s. Arguments:",
                     (type ==
                      MONITOR_PROC_CMD ? "monitor procces" : "child proces"));
     while (argv[i] != NULL) {
          ci_debug_printf(1, "%s,", argv[i]);
          i++;
     }
     ci_debug_printf(1, "\n");
}

int init_server(int port, int *family)
{
     if (LISTEN_SOCKET != -1)
          close(LISTEN_SOCKET);

     LISTEN_SOCKET = icap_init_server(port, family, MAX_SECS_TO_LINGER);
     if (LISTEN_SOCKET == CI_SOCKET_ERROR)
          return 0;
     return 1;
}

void init_commands()
{
     register_command("stop", MONITOR_PROC_CMD, stop_command);
     register_command("reconfigure", MONITOR_PROC_CMD, reconfigure_command);
     register_command("test", MONITOR_PROC_CMD | CHILDS_PROC_CMD, test_command);
}

int start_server()
{
     int child_indx, pid, i, ctl_socket;
     int childs, freeservers, used, maxrequests, ret;
     char command_buffer[COMMANDS_BUFFER_SIZE];

     ctl_socket = ci_named_pipe_create(CONF.COMMANDS_SOCKET);
     if (ctl_socket < 0) {
          ci_debug_printf(1,
                          "Error opening control socket:%s.Fatal error, exiting!\n",
                          CONF.COMMANDS_SOCKET);
          exit(0);
     }

     ci_proc_mutex_init(&accept_mutex);
     childs_queue = malloc(sizeof(struct childs_queue));
     if (!create_childs_queue(childs_queue, 2 * MAX_CHILDS)) {
          ci_debug_printf(1,
                          "Can't init shared memory.Fatal error, exiting!\n");
          exit(0);
     }

     init_commands();
     pid = 1;
#ifdef MULTICHILD
     if (START_CHILDS > MAX_CHILDS)
          START_CHILDS = MAX_CHILDS;

     for (i = 0; i < START_CHILDS; i++) {
          if (pid)
               pid = start_child(LISTEN_SOCKET);
     }
     if (pid != 0) {
          main_signals();

          while (1) {
               if ((ret = wait_for_commands(ctl_socket, command_buffer, 1)) > 0) {
                    ci_debug_printf(5, "I get the command: %s\n",
                                    command_buffer);
                    handle_monitor_process_commands(command_buffer);
               }
               if (ret == 0) {  /*Eof received on pipe. Going to reopen ... */
                    ci_named_pipe_close(ctl_socket);
                    ctl_socket = ci_named_pipe_open(CONF.COMMANDS_SOCKET);
                    if (ctl_socket < 0) {
                         ci_debug_printf(1,
                                         "Error opening control socket.We are unstable going down!");
                         c_icap_going_to_term = 1;
                    }
               }

               if (c_icap_going_to_term)
                    break;
               childs_queue_stats(childs_queue, &childs, &freeservers, &used,
                                  &maxrequests);
               ci_debug_printf(10,
                               "Server stats: \n\t Childs:%d\n\t Free servers:%d\n"
                               "\tUsed servers:%d\n\tRequests served:%d\n",
                               childs, freeservers, used, maxrequests);
	       if((child_indx = find_a_child_nrequests(childs_queue, MAX_REQUESTS_PER_CHILD)) >=0 ){
		    ci_debug_printf(8, "Max requests reached for child :%d of pid :%d\n", 
				    child_indx, childs_queue->childs[child_indx].pid);
		    pid = start_child(LISTEN_SOCKET);
		    usleep(500);
		    childs_queue->childs[child_indx].father_said =
			 GRACEFULLY;
		    /*kill a server ... */
		    kill(childs_queue->childs[child_indx].pid, SIGTERM);

	       }
               else if ((freeservers <= MIN_FREE_SERVERS && childs < MAX_CHILDS)
                   || childs < START_CHILDS) {
                    ci_debug_printf(8,
                                    "Free Servers:%d,childs:%d. Going to start a child .....\n",
                                    freeservers, childs);
                    pid = start_child(LISTEN_SOCKET);
               }
               else if (freeservers >= MAX_FREE_SERVERS &&
                        childs > START_CHILDS &&
                        (freeservers - START_SERVERS) > MIN_FREE_SERVERS) {

                    if ((child_indx = find_an_idle_child(childs_queue)) >= 0) {
                         childs_queue->childs[child_indx].father_said =
                             GRACEFULLY;
                         ci_debug_printf(8,
                                         "Free Servers:%d,childs:%d. Going to stop child %d(index:%d)\n",
                                         freeservers, childs,
                                         childs_queue->childs[child_indx].pid,
                                         child_indx);
                         /*kill a server ... */
                         kill(childs_queue->childs[child_indx].pid, SIGTERM);

                    }
               }
               else if (childs == MAX_CHILDS && freeservers < MIN_FREE_SERVERS) {
                    ci_debug_printf(1,
                                    "ATTENTION!!!! Not enought available servers!!!!! "
                                    "Maybe you must increase the MaxServers and the "
                                    "ThreadsPerChild values in c-icap.conf file!!!!!!!!!");
               }
               if (c_icap_going_to_term)
                    break;
               check_for_exited_childs();
               if (c_icap_reconfigure) {
                    c_icap_reconfigure = 0;
                    server_reconfigure();
               }
          }
          /*Main process exit point */
          ci_debug_printf(1,
                          "Possibly a term signal received. Monitor process going to term all childs\n");
          kill_all_childs();
          exit(0);
     }
#else
     child_data = (child_shared_data_t *) malloc(sizeof(child_shared_data_t));
     child_data->pid = 0;
     child_data->freeservers = START_SERVERS;
     child_data->usedservers = 0;
     child_data->requests = 0;
     child_data->connections = 0;
     child_data->to_be_killed = 0;
     child_data->father_said = 0;
     child_data->idle = 1;
     child_main(LISTEN_SOCKET, 0);
#endif
     return 1;
}
