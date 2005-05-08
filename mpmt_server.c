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



extern int KEEPALIVE_TIMEOUT;
extern int MAX_SECS_TO_LINGER;
extern int START_CHILDS;
extern int MAX_CHILDS;
extern int START_SERVERS;
extern int MIN_FREE_SERVERS;
extern int MAX_FREE_SERVERS;
extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;

typedef struct server_decl{
     int srv_id;
     ci_thread_t srv_pthread;
     struct connections_queue *con_queue;
     request_t *current_req;
     int served_requests;
     int served_requests_no_reallocation;
} server_decl_t;


ci_thread_mutex_t threads_list_mtx;
server_decl_t **threads_list=NULL;

ci_thread_cond_t free_server_cond;
ci_thread_mutex_t counters_mtx;

struct childs_queue childs_queue;
child_shared_data_t *child_data;
struct connections_queue *con_queue;

/*Interprocess accepting mutex ....*/
ci_proc_mutex_t accept_mutex;



#define hard_close_connection(connection)  ci_hard_close(connection->fd)
#define close_connection(connection) ci_linger_close(connection->fd,MAX_SECS_TO_LINGER)
#define check_for_keepalive_data(fd) ci_wait_for_data(fd,KEEPALIVE_TIMEOUT,wait_for_read)




static void sigpipe_handler(int sig){
    ci_debug_printf(1,"SIGPIPE signal received.\n");
    log_server(NULL,"%s","SIGPIPE signal received.\n");
}

static void exit_normaly(){
     int i=0;
     server_decl_t *srv;
     ci_debug_printf(5,"Suppose that all threads are allready exited...\n");
     while((srv=threads_list[i])!=NULL){
	  if(srv->current_req){
	       close_connection(srv->current_req->connection);
	       destroy_request(srv->current_req);
	  }
	  free(srv);
	  threads_list[i]=NULL;
	  i++;
     }
     free(threads_list);
     dettach_childs_queue(&childs_queue);
     log_close();
}


static void cancel_all_threads(){
     int i=0;
     int retval,pid,status;
//     ci_thread_mutex_lock(&threads_list_mtx);

     ci_thread_cond_broadcast(&(con_queue->queue_cond));//What about childs that serve a request?
     while(threads_list[i]!=NULL){
	  ci_debug_printf(5,"Cancel server %d, thread_id %d (%d)\n",threads_list[i]->srv_id,
		       threads_list[i]->srv_pthread,i);
	  ci_thread_join(threads_list[i]->srv_pthread);
	  i++;
     }
//     ci_threadmutex_unlock(&threads_list_mtx);
}


static void term_handler_child(int sig){
     int i=0;
     ci_debug_printf(5,"A termination signal received (%d).\n",sig);
     if(child_data->to_be_killed==GRACEFULLY){
	  ci_debug_printf(5,"Exiting gracefully\n");
	  cancel_all_threads();
	  exit_normaly();
     }
     exit(0);
}

static void sigint_handler_main(int sig){
     int i=0,status,pid;

/* */
     signal(SIGINT,SIG_IGN );
     signal(SIGCHLD,SIG_IGN);

     if(sig==SIGTERM){
        ci_debug_printf(5,"SIGTERM signal received for main server.\n");
        ci_debug_printf(5,"Going to term childs....\n");
        for(i=0;i<childs_queue.size;i++){
	     if(childs_queue.childs[i].pid==0)
		  continue;
	     kill(pid,SIGTERM);
        }
     }
     else if(sig==SIGINT){
	  ci_debug_printf(5,"SIGINT signal received for main server.\n");
     }
     else{
	  ci_debug_printf(5,"Signal %d received. Exiting ....\n",sig);
     }
     
     for(i=0;i<childs_queue.size;i++){
	  if(childs_queue.childs[i].pid==0)
	       continue;
	  pid=wait(&status);
	  ci_debug_printf(5,"Child %d died with status %d\n",pid,status);
     }
     ci_proc_mutex_destroy(&accept_mutex);
     destroy_childs_queue(&childs_queue);
     exit(0);
}

static void empty(int sig){
    ci_debug_printf(10,"An empty signal handler (%d).\n",sig);
}


static void sigchld_handler_main(int sig){
     int status,pid,i;
     if((pid=wait(&status))<0){
	  ci_debug_printf(1,"Fatal error waiting a child to exit .....\n");
	  return;
     }
     if(pid>0){
	  ci_debug_printf(5,"The child %d died ...\n",pid);
	  remove_child(&childs_queue,pid);
	  if(!WIFEXITED(status)){
	       ci_debug_printf(3,"The child not exited normaly....");
	       if(WIFSIGNALED(status))
		    ci_debug_printf(3,"signaled with signal:%d\n",WTERMSIG(status));
	  }

     }
}

void child_signals(){
     signal(SIGPIPE, sigpipe_handler);
     signal(SIGINT, term_handler_child);
     signal(SIGTERM,term_handler_child);
     signal(SIGHUP,empty);
}

void main_signals(){
     signal(SIGPIPE, sigpipe_handler);
     signal(SIGTERM, sigint_handler_main);
     signal(SIGINT, sigint_handler_main);
     signal(SIGCHLD,sigchld_handler_main);
}


void thread_signals(){
     sigset_t sig_mask;
     sigemptyset(&sig_mask);
     sigaddset(&sig_mask,SIGINT);
     if(pthread_sigmask(SIG_BLOCK,&sig_mask,NULL))
	  ci_debug_printf(5,"O an error....\n");
     pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
     pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
}




server_decl_t *newthread(struct connections_queue *con_queue){
     int i=0;
     server_decl_t *serv;
     serv=(server_decl_t *)malloc(sizeof(server_decl_t));
     serv->srv_id=0;
//    serv->srv_pthread=pthread_self();
     serv->con_queue=con_queue;
     serv->served_requests=0;
     serv->served_requests_no_reallocation=0;
     serv->current_req=NULL;

     return serv;
}




int thread_main(server_decl_t *srv){
     ci_connection_t con;
     ci_thread_mutex_t cont_mtx;
     char clientname[CI_MAXHOSTNAMELEN+1];
     int max,ret,request_status=0;
     request_t *tmp;
//***********************
     thread_signals();
//*************************
     srv->srv_id=getpid(); //Setting my pid ...
     srv->srv_pthread=pthread_self();

     for(;;){
	  if(child_data->to_be_killed)
	       return; //Exiting thread.....
	  
	  if((ret=get_from_queue(con_queue,&con))==0){
	       ret=wait_for_queue(con_queue); //It is better that the wait_for_queue to be 
                                          //moved into the get_from_queue 
	       continue; 
	  }

	  
	  if(ret<0){ //An error has occured
	       ci_debug_printf(1,"Fatal Error!!! Error getting a connection from connections queue!!!\n");
	       break;
	  }

	  ci_thread_mutex_lock(&counters_mtx); /*Update counters as soon as possible .......*/
	  (child_data->freeservers)--;
	  (child_data->usedservers)++;
	  ci_thread_mutex_unlock(&counters_mtx);



	  ci_netio_init(con.fd);

	  ret=1;
	  if(srv->current_req==NULL)
	       srv->current_req=newrequest(&con);
	  else
	       ret=recycle_request(srv->current_req,&con);
	  
	  if(srv->current_req==NULL || ret==0){
	       ci_addrtohost(&(con.claddr.sin_addr),clientname, CI_MAXHOSTNAMELEN);
	       ci_debug_printf(1,"Request from %s denied...\n",clientname);
	       hard_close_connection((&con));
	       goto end_of_main_loop_thread;/*The request rejected. Log an error and continue ...*/
	  }



	  do{
	       if((request_status=process_request(srv->current_req))<0){
		    ci_debug_printf(5,"Process request timeout or interupted....\n");
		    reset_request(srv->current_req);
		    break;//
	       }
	       srv->served_requests++;
	       srv->served_requests_no_reallocation++;

    /*Increase served requests. I dont like this. The delay is small but I don't like...*/
	       ci_thread_mutex_lock(&counters_mtx); 
	       (child_data->requests)++; 
	       ci_thread_mutex_unlock(&counters_mtx);



	       log_access(srv->current_req,request_status);
//	       break; //No keep-alive ......

	       if(child_data->to_be_killed)
		    return; //Exiting thread.....
	      
               ci_debug_printf(8,"Keep-alive:%d\n",srv->current_req->keepalive); 
	       if(srv->current_req->keepalive && check_for_keepalive_data(srv->current_req->connection->fd)){
		    reset_request(srv->current_req);
		    ci_debug_printf(8,"Server %d going to serve new request from client(keep-alive) \n",
				 srv->srv_id);
	       }
	       else
		    break;
	  }while(1);
	  
	  if(srv->current_req){
	       if(request_status<0)
		    hard_close_connection(srv->current_req->connection);
	       else
		    close_connection(srv->current_req->connection);
	  }
	  if(srv->served_requests_no_reallocation > MAX_REQUESTS_BEFORE_REALLOCATE_MEM){
	       ci_debug_printf(5,"Max requests reached, reallocate memory and buffers .....\n");
	       destroy_request(srv->current_req);
	       srv->current_req=NULL;
	       srv->served_requests_no_reallocation=0;
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


void child_main(int sockfd){
     ci_connection_t conn;
     int claddrlen=sizeof(struct sockaddr_in);
     ci_thread_t thread;
     char clientname[300];
     int i,retcode,haschild=1,jobs_in_queue=0;
     int pid=0;

     child_signals();
     pid=getpid();
     ci_thread_mutex_init(&threads_list_mtx);
     ci_thread_mutex_init(&counters_mtx);
     ci_thread_cond_init(&free_server_cond);


     threads_list=(server_decl_t **)malloc((START_SERVERS+1)*sizeof(server_decl_t *));
     con_queue=init_queue(START_SERVERS);

     for(i=0;i<START_SERVERS;i++){
	  if((threads_list[i]=newthread(con_queue))==NULL){
	       exit(-1);// FATAL error.....
	  }
	  retcode=ci_thread_create(&thread,(void *(*)(void *))thread_main,(void *)threads_list[i]);
     }
     threads_list[START_SERVERS]=NULL;

     for(;;){ //Global for
	  if(!ci_proc_mutex_lock(&accept_mutex)){
	       
	       if(errno==EINTR){
		    ci_debug_printf(5,"EINTR received\n");
		    if(child_data->to_be_killed)
			 goto end_child_main;
		    continue;
	       }
	  }
	  child_data->idle=0;
	  ci_debug_printf(7,"Child %d getting requests now ...\n",pid);
	  do{//Getting requests while we have free servers.....
	       do{
		    errno = 0;
		    if(((conn.fd = accept(sockfd, (struct sockaddr *)&(conn.claddr), &claddrlen)) == -1) && errno != EINTR){
			 ci_debug_printf(1,"error accept .... %d\nExiting server ....\n",errno);
			 exit(-1); //For the moment .......
			 goto end_child_main ;
		    }
		    if(errno==EINTR && child_data->to_be_killed)
			 goto end_child_main;
	       }while(errno==EINTR);

	       getsockname(conn.fd,(struct sockaddr *)&(conn.srvaddr),&claddrlen);


	       icap_socket_opts(sockfd,MAX_SECS_TO_LINGER);
	       
	       if((jobs_in_queue=put_to_queue(con_queue,&conn))==0){
		    ci_debug_printf(1,"ERROR!!!!!!NO AVAILABLE SERVERS!THIS IS A BUG!!!!!!!!\n");
		    child_data->to_be_killed=GRACEFULLY;
		    ci_debug_printf(1,"Jobs in Queue:%d,Free servers:%d, Used Servers :%d, Requests %d\n",
				 jobs_in_queue,
				 child_data->freeservers,child_data->usedservers,child_data->requests);
		    
		    goto end_child_main;
	       }
	       ci_thread_mutex_lock(&counters_mtx);	       
	       haschild=((child_data->freeservers-jobs_in_queue)>0?1:0);
	       ci_thread_mutex_unlock(&counters_mtx);
	       (child_data->connections)++; //NUM of Requests....
	  }while(haschild);

	  child_data->idle=1;
	  ci_proc_mutex_unlock(&accept_mutex);

	  ci_thread_mutex_lock(&counters_mtx);
	  if((child_data->freeservers-connections_pending(con_queue))<=0){
	       ci_debug_printf(7,"Child %d waiting for a thread to accept more connections ...\n",pid);
	       ci_thread_cond_wait(&free_server_cond,&counters_mtx);
	  }
	  ci_thread_mutex_unlock(&counters_mtx);

     }

end_child_main:
     cancel_all_threads();
     
     exit_normaly();
}

#define MULTICHILD
//#undef MULTICHILD

int start_child(int fd){
     int pid;
     if((pid=fork())==0){//A Child .......
	  attach_childs_queue(&childs_queue);
	  child_data=register_child(&childs_queue,getpid(),START_SERVERS);
	  child_main(fd);
	  exit(0);
     }
     else
	  return pid;
}



int start_server(int fd){
     int child_indx,pid,i,status;
     int childs,freeservers,used,maxrequests;

     ci_proc_mutex_init(&accept_mutex);
     if(!create_childs_queue(&childs_queue,MAX_CHILDS)){
	  ci_debug_printf(1,"Can't init shared memory.Fatal error, exiting!\n");
	  exit(0);
     }

     pid=1;


#ifdef MULTICHILD 

     for(i=0;i< START_CHILDS; i++){
	  if(pid)
	       pid=start_child(fd);
     }
     if(pid!=0){
	  main_signals();

	  while(1){
	       sleep(1); /*Must be replaced by nanosleep. */
	       childs_queue_stats(&childs_queue,&childs,&freeservers, &used, &maxrequests);
	       ci_debug_printf(10,"Server stats: \n\t Childs:%d\n\t Free servers:%d\n\tUsed servers:%d\n\tRequests served:%d\n",
			    childs, freeservers,used, maxrequests);
	       if( (freeservers<=MIN_FREE_SERVERS && childs<MAX_CHILDS) ||childs<START_CHILDS){
		    ci_debug_printf(8,"Free Servers:%d,childs:%d. Going to start a child .....\n",
				    freeservers,childs);
		    pid=start_child(fd);
	       }
	       else if(freeservers>=MAX_FREE_SERVERS&& 
		       childs>START_CHILDS && 
		       (freeservers-START_SERVERS)>MIN_FREE_SERVERS){
		    
		    if((child_indx=find_an_idle_child(&childs_queue))>=0){
			 childs_queue.childs[child_indx].to_be_killed=GRACEFULLY;
			 ci_debug_printf(8,"Free Servers:%d,childs:%d. Going to stop child %d(index:%d) .....\n",
					 freeservers,childs,childs_queue.childs[child_indx].pid,child_indx);
			 /*kill a server ...*/
			 kill(childs_queue.childs[child_indx].pid,SIGINT);
			
		    }
	       }    
	       else if(childs==MAX_CHILDS && freeservers < MIN_FREE_SERVERS ){
		    ci_debug_printf(1,"ATTENTION!!!! Not enought available servers!!!!! Maybe you must increase the MaxServers and the ThreadsPerChild values in c-icap.conf file!!!!!!!!!");
	       }
	  }
	  for(i=0;i<START_CHILDS;i++){
	       pid=wait(&status);
	       ci_debug_printf(5,"The child %d died with status %d\n",pid,status);
	  }
     }
#else
     child_data=(child_shared_data_t *)malloc(sizeof(child_shared_data_t));     
     child_data->pid=0;
     child_data->freeservers=START_SERVERS;
     child_data->usedservers=0;
     child_data->requests=0;
     child_data->connections=0;
     child_data->to_be_killed=0;
     child_data->idle=1;     
     child_main(fd);
#endif


}
