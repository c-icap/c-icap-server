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
#include "net_io.h"
#include "debug.h"
#include "module.h"
#include "log.h"
#include "cfg_param.h"


extern char *PIDFILE;
extern char *RUN_USER;
extern char *RUN_GROUP;
extern int DAEMON_MODE;

int config(int,char **);
int start_server(ci_socket fd);
int store_pid(char *pidfile);
int set_running_permissions(char *user,char *group);


#if ! defined(_WIN32)
void run_as_daemon(){
     int pid;
     if((pid=fork())<0){
	  debug_printf(1,"Can not fork. exiting...");
	  exit(-1);
     }
     if(pid)
	  exit(0);
}
#endif

int main(int argc,char **argv){
     ci_socket s;

     init_modules();
     config(argc,argv);

#if ! defined(_WIN32)     
     if(DAEMON_MODE)
	  run_as_daemon();


     store_pid(PIDFILE);
     if(!set_running_permissions(RUN_USER,RUN_GROUP))
	  exit(-1);
#endif

     if(!log_open()){
          debug_printf(1,"Can not init loggers. Exiting.....\n");
          exit(-1);
     }
 
     s=icap_init_server(); 


     if(s==CI_SOCKET_ERROR)
	  return -1;

     start_server(s);

     return 0;
}

