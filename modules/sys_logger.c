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
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include "log.h"
#include "module.h"
#include "cfg_param.h"



/************************************************************************/
/*  sys_logger implementation.                                          */
/*                                                                      */

int sys_log_open();
void sys_log_close();
void sys_log_access(char *server, char *clientname,char *method,char *request,char *args, char *status);
void sys_log_server(char *server, const char *format, va_list ap );

char *log_ident="c-icap: ";
static int FACILITY=LOG_DAEMON;
static int ACCESS_PRIORITY=LOG_INFO;
static int SERVER_PRIORITY=LOG_CRIT;

int SetFacility(char *directive,char **argv,void *setdata);
int SetPriority(char *directive,char **argv,void *setdata);
int SetPrefix(char *directive,char **argv,void *setdata);

/*Configuration Table .....*/
static struct conf_entry conf_variables[]={
     {"Facility",NULL,SetFacility,NULL},
     {"acces_priority",&ACCESS_PRIORITY,SetPriority,NULL},
     {"server_priority",&SERVER_PRIORITY,SetPriority,NULL},
     {"Prefix",&log_ident,setStr,NULL},
     {NULL,NULL,NULL,NULL}
};



CI_DECLARE_DATA logger_module_t module={
     "sys_logger",
     NULL,
     sys_log_open,
     sys_log_close,
     sys_log_access,
     sys_log_server,
     conf_variables
};


int SetFacility(char *directive,char **argv,void *setdata){
     if(argv==NULL || argv[0]==NULL){
//	  ci_debug_printf(1,"Missing arguments in directive\n");
	  return 0;
     }
     if(strcmp(argv[0],"daemon")==0)
	  FACILITY=LOG_DAEMON;
     else if(strcmp(argv[0],"user")==0)
	  FACILITY=LOG_USER;
     else if(strncmp(argv[0],"local",5)==0 && strlen(argv[0])==6) {
	  switch(argv[0][5]){
	  case '0':
	       FACILITY=LOG_LOCAL0;
	       break;
	  case '1':
	       FACILITY=LOG_LOCAL1;
	       break;
	  case '2':
	       FACILITY=LOG_LOCAL2;
	       break;
	  case '3':
	       FACILITY=LOG_LOCAL3;
	       break;
	  case '4':
	       FACILITY=LOG_LOCAL4;
	       break;
	  case '5':
	       FACILITY=LOG_LOCAL5;
	       break;
	  case '6':
	       FACILITY=LOG_LOCAL6;
	       break;
	  case '7':
	       FACILITY=LOG_LOCAL7;
	       break;
	  }
     }
}

int SetPriority(char *directive,char **argv,void *setdata){
     if(argv==NULL || argv[0]==NULL){
	  ci_debug_printf(1,"Missing arguments in directive\n");
	  return 0;
     }
     if(!setdata)
	  return 0;

     if(strcmp(argv[0],"alert")==0)
	  *((int*)setdata)=LOG_ALERT;
     else if(strcmp(argv[0],"crit")==0)
	  *((int*)setdata)=LOG_CRIT;
     else if(strcmp(argv[0],"debug")==0)
	  *((int*)setdata)=LOG_DEBUG;
     else if(strcmp(argv[0],"emerg")==0)
	  *((int*)setdata)=LOG_EMERG;
     else if(strcmp(argv[0],"err")==0)
	  *((int*)setdata)=LOG_ERR;
     else if(strcmp(argv[0],"info")==0)
	  *((int*)setdata)=LOG_INFO;
     else if(strcmp(argv[0],"notice")==0)
	  *((int*)setdata)=LOG_NOTICE;
     else if(strcmp(argv[0],"warning")==0)
	  *((int*)setdata)=LOG_WARNING;
     return 1;
}


int sys_log_open(){
     openlog(log_ident,0,FACILITY);
     return 1;
}

void sys_log_close(){
     closelog();
}



void sys_log_access(char *server, char *clientname,char *method,char *request,char *args, char *status){

     syslog(ACCESS_PRIORITY,"%s, %s, %s, %s%c%s, %s\n",server,clientname,
	    method,
	    request,
	    (args==NULL?' ':'?'),
	    (args==NULL?"":args),
	    status);
}


void sys_log_server(char *server, const char *format, va_list ap ){
     char buf[512];
     char prefix[150];

     snprintf(prefix,149,"%s, %s ",server, format);
     prefix[149]='\0';

     vsnprintf(buf,511,(const char *)prefix,ap);
     buf[511]='\0';
     syslog(SERVER_PRIORITY,"%s",buf);
}

