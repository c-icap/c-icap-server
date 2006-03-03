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


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "request.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"
 
//char *servername;
//char *modulename;

int icap_write(int fd, const void *buf,size_t count){
     int bytes=0;
     int remains=count;
     char *b= (char *)buf;
     
     while(remains>0){ //write until count bytes written
          do{
               bytes=write(fd,b,remains);
          }while(bytes==-1 && errno==EINTR);
	  
          if(bytes<0)
               return bytes;
          b=b+bytes;//points to remaining bytes......
          remains=remains-bytes;
     }//Ok......
     return count;
}


int readline(char *buf,int max,FILE *f){
     char c;
     int i=0;
     while((c=getc(f))!=EOF){
	  buf[i]=c;
	  i++;
	  if(c=='\n'|| i>=max)
	       return i;
     }
     return 0;
}



void dofile(int fd,char *filename,char *savefilename){
     char buf[1024];
     char *str;
     FILE *f;
     int len;
     f=fopen(filename,"r");

     if(f==NULL){
	 printf("FATAL no such file :%s\n",filename);
	 exit(-1);
     }

     str=buf;

     printf("Sending :\n");
     while((len=readline(str,511,f))>0){
	  if(len>1){
	       str[len-1]='\r';
	       str[len]='\n';
	       str[len+1]='\0';
	       len++;
	  }
	  else{
	       strcpy(str,"\r\n");
	       len=2;
	  }
	  printf("\t %s ",str);
	  str+=len;
     }
     strcpy(str,"\r\n");
     str+=2;
     
     fclose(f);
     len=str-buf;
     icap_write(fd,buf,len);

     printf("\n\nRESPONCE: ");
     while((len=read(fd,buf,512))>0){
	  buf[len]='\0';
//	  printf("(BYTES=%d)\n%s",len,buf);
	  printf("%s",buf);
     }

}

char *icap_server="localhost";
char *input_file=NULL;
char *output_file=NULL;
int RESPMOD=1;

static struct options_entry options[]={
     {"-i","icap_servername",&icap_server,ci_cfg_set_str,"The icap server name"},
     {"-f","filename",&input_file,ci_cfg_set_str,"Send this file to the icap server.\nDefault is to send an options request"},
     {"-o","filename",&output_file,ci_cfg_set_str,"Save output to this file.\nDefault is to send to the stdout"},
     {"-req",NULL,&RESPMOD,ci_cfg_disable,"Send a request modification instead of responce modification"},
     {"-d","level", &CI_DEBUG_LEVEL,ci_cfg_enable,"Print debug info to stdout"},
     {NULL,NULL,NULL,NULL}
};

void log_errors(request_t *req, const char *format,... ){
     va_list ap;
     va_start(ap,format);
     vfprintf(stderr,format,ap);
     va_end(ap);
}

void vlog_errors(request_t *req, const char *format, va_list ap){
    vfprintf(stderr,format,ap);
}

int ci_host_to_sockaddr_t(char *servername,ci_sockaddr_t *addr){
    int ret=0;
    struct addrinfo hints,*res;
    memset(&hints,0,sizeof(hints));
    hints.ai_family=PF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=0;
    if((ret=getaddrinfo(servername,NULL,&hints,&res))!=0){
	ci_debug_printf(1,"Error geting addrinfo return was:%d\n",ret);
	return 0;
    }
    //fill the addr..... and 
    memcpy(addr,res->ai_addr,CI_SOCKADDR_SIZE);
    freeaddrinfo(res);
    return 1;
}


ci_connection_t *ci_connect_to(char *servername,int port){
    ci_connection_t *connection=malloc(sizeof(ci_connection_t));
    char hostname[CI_MAXHOSTNAMELEN+1];
    int addrlen=0;
    if(!connection)
	return NULL;
    connection->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(connection->fd == -1){
	ci_debug_printf(1,"Error oppening socket ....\n");
	free(connection);
	return NULL;
    }

    if(!ci_host_to_sockaddr_t(servername,&(connection->srvaddr))){
	free(connection);
	return NULL;	
    }
    
    connection->srvaddr.sockaddr.sin_port=htons(port);

    if(connect(connection->fd, (struct sockaddr *)&(connection->srvaddr.sockaddr), CI_SOCKADDR_SIZE)){
	ci_sockaddr_t_to_host(&(connection->srvaddr), hostname, CI_MAXHOSTNAMELEN);
	ci_debug_printf(1,"Error connecting to socket (host: %s) .....\n",hostname);
	free(connection);
	return NULL;
    }
     
    addrlen=CI_SOCKADDR_SIZE;
    getsockname(connection->fd,(struct sockaddr *)&(connection->claddr.sockaddr),&addrlen);
    ci_fill_sockaddr(&(connection->claddr));
    ci_fill_sockaddr(&(connection->srvaddr));

    return connection;
}




int main(int argc, char **argv){
//     int fd;
     int port=1344;
     ci_connection_t *conn;

     CI_DEBUG_LEVEL=1; /*Default debug level is 1*/

     if(!ci_args_apply(argc,argv,options)){
	 ci_args_usage(argv[0],options);
	 exit(-1);
     }

#if ! defined(_WIN32)
     __log_error=(void(*)(void *, const char *,... ))log_errors; /*set c-icap library log  function*/
#else
     __vlog_error=vlog_errors; /*set c-icap library  log function for win32.....*/
#endif


     if(!(conn=ci_connect_to(icap_server,port))){
	 ci_debug_printf(1,"Failed to connect to icap server.....\n");
	 exit(-1);
     }


     dofile(conn->fd,input_file,NULL);

     close(conn->fd);      
     return 0;
}
