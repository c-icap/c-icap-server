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
#include <errno.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include "debug.h"
#include "net_io.h"


const char *ci_sockaddr_t_to_host(ci_sockaddr_t *addr, char *hname, int maxhostlen){
     getnameinfo(&(addr->sockaddr), CI_SOCKADDR_SIZE,hname,maxhostlen-1,NULL,0,0);
     return (const char *)hname;
}


#ifdef HAVE_IPV6
int icap_init_server_ipv6(int port,int *protocol_family,int secs_to_linger){
     int fd;
     struct sockaddr_in6 addr;
  
     fd = socket(AF_INET6, SOCK_STREAM, 0);
     if(fd == -1){
	  ci_debug_printf(1,"Error opening ipv6 socket ....\n");
	  return CI_SOCKET_ERROR;
     }

     icap_socket_opts(fd,secs_to_linger);

     addr.sin6_family = AF_INET6;
     addr.sin6_port = htons(port);
     memcpy(&(addr.sin6_addr), &(in6addr_any),sizeof(struct in6_addr));

     if(bind(fd,(struct sockaddr *) &addr, sizeof(addr))){
	  ci_debug_printf(1,"Error bind  at ipv6 address \n");;
	  close(fd);
	  return CI_SOCKET_ERROR;
     }
     if(listen(fd, 512)){
	  ci_debug_printf(1,"Error listen at ipv6 address.....\n");
	  close(fd);
	  return CI_SOCKET_ERROR;
     }
     *protocol_family=AF_INET6;
     return fd;

}

#endif

int icap_init_server(int port,int *protocol_family,int secs_to_linger){
     int fd;
     struct sockaddr_in addr;

#ifdef HAVE_IPV6
     if((fd=icap_init_server_ipv6(port,protocol_family,secs_to_linger))!=CI_SOCKET_ERROR)
	  return fd;
     ci_debug_printf(1,"WARNING! Error bind to an ipv6 address. Trying ipv4...\n");
#endif

     fd = socket(AF_INET, SOCK_STREAM, 0);
     if(fd == -1){
	  ci_debug_printf(1,"Error opening socket ....\n");
	  return CI_SOCKET_ERROR;
     }

     icap_socket_opts(fd,secs_to_linger);

     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);
     addr.sin_addr.s_addr = INADDR_ANY;

     if(bind(fd,(struct sockaddr *) &addr, sizeof(addr))){
	  ci_debug_printf(1,"Error bind  \n");;
	  return CI_SOCKET_ERROR;
     }
     if(listen(fd, 512)){
	  ci_debug_printf(1,"Error listen .....\n");
	  return CI_SOCKET_ERROR;
     }
     *protocol_family=AF_INET;
     return fd;
}



int icap_socket_opts(ci_socket fd, int secs_to_linger){
     struct linger li;
     int value;
     /* if (fcntl(fd, F_SETFD, 1) == -1) {
        ci_debug_printf(1,"can't set close-on-exec on server socket!");
	}
     */

     value = 1;
     if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1){
	  ci_debug_printf(1,"setsockopt: unable to set SO_REUSEADDR\n");  
     }

     value = 1;
     if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,&value, sizeof (value)) == -1) {
	  ci_debug_printf(1,"setsockopt: unable to set TCP_NODELAY\n");
     }

     li.l_onoff = 1;
     li.l_linger = secs_to_linger;/*MAX_SECS_TO_LINGER;*/
  
     if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		    (char *) &li, sizeof(struct linger)) < 0) {
	  ci_debug_printf(1,"setsockopt: unable to set SO_LINGER \n");
     }
     return 1;
}




int ci_netio_init(int fd){
     fcntl(fd, F_SETFL, O_NONBLOCK ); //Setting newfd descriptor to nonblocking state....
     return 1;
}


int ci_wait_for_data(int fd,int secs,int what_wait){
     fd_set fds,*rfds,*wfds;
     struct timeval tv;
     int ret;

     if(secs>=0){
	  tv.tv_sec=secs;
	  tv.tv_usec=0;
     }
     
     FD_ZERO(&fds);
     FD_SET(fd,&fds);

     if(what_wait==wait_for_read){
	  rfds=&fds;
	  wfds=NULL;
     }
     else{
	  wfds=&fds;
	  rfds=NULL;
     }
     if((ret=select(fd+1,rfds,wfds,NULL,(secs>=0?&tv:NULL)))>0)
	  return 1;
     
     if(ret<0){
	  ci_debug_printf(5,"Fatal error while waiting for new data....\n");
     }
     return 0;
}


int ci_read(int fd,void *buf,size_t count,int timeout){
     int bytes=0;
     do{
	  bytes=read(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);
     
     if(bytes==-1 && errno==EAGAIN){

	  if(!ci_wait_for_data(fd,timeout,wait_for_read)){
	       return bytes;
	  }

	  do{
	       bytes=read(fd,buf,count);
	  }while(bytes==-1 && errno==EINTR);
     }
     if(bytes==0){
	  return -1;
     }
     return bytes;
}


int ci_write(int fd, const void *buf,size_t count,int timeout){
     int bytes=0;
     int remains=count;
     char *b= (char *)buf;

     while(remains>0){ //write until count bytes written
	  do{
	       bytes=write(fd,b,remains);
	  }while(bytes==-1 && errno==EINTR);
	  
	  if(bytes==-1 && errno==EAGAIN){
	       
	       if(!ci_wait_for_data(fd,timeout,wait_for_write)){
		    return bytes;
	       }
	       
	       do{
		    bytes=write(fd,b,remains);
	       }while(bytes==-1 && errno==EINTR);
	       
	  }
	  if(bytes<0)
	       return bytes;
	  b=b+bytes;//points to remaining bytes......
	  remains=remains-bytes;
     }//Ok......
     return count;
}


int ci_read_nonblock(int fd, void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=read(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);

     if(bytes<0 && errno==EAGAIN)
	  return 0;

     return bytes;
}



int ci_write_nonblock(int fd, const void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=write(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);

     if(bytes<0 && errno==EAGAIN)
	  return 0;

     return bytes;
}



int ci_linger_close(int fd, int timeout){
     char buf[10];
     int ret;
     ci_debug_printf(8,"Waiting to close connection\n");

     if(shutdown(fd,SHUT_WR)!=0){
	  close(fd);
	  return 1;
     }

     while(ci_wait_for_data(fd,timeout,wait_for_read) && (ret=ci_read_nonblock(fd,buf,10))>0)
	  ci_debug_printf(10,"OK I linger %d bytes.....\n",ret);

     close(fd);
     ci_debug_printf(8,"Connection closed ...\n");
     return 1;
}


int ci_hard_close(int fd){
	  close(fd);
	  return 1;
}


/*
int readline(int fd,char *buf){
     int i=0,readed=0;
     char c,oc=0;
     while((readed=icap_read(fd,&c,1))>0 && c!='\n'  && i<BUFSIZE ){
	  if(c=='\r'){
	       icap_read(fd,&c,1);
	       if(c=='\n')
		    break;
	       buf[i++]='\r';
	       buf[i++]=c;
	  }
	  else
	       buf[i++]=c;
     }
     buf[i]='\0';
     if(i==BUFSIZE){
	  ci_debug_printf("Readline error. Skip until eol ......\n");
	  while(icap_read(fd,&c,1)>0 && c!='\n');
     }
     return i;
     }
*/

