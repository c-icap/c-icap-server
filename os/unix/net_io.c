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
#include "cfg_param.h"



int icap_socket_opts(ci_socket fd);


int icap_init_server(){
     int fd;
     struct sockaddr_in addr;
     struct linger li;
     int value,addrlen;
  
     fd = socket(AF_INET, SOCK_STREAM, 0);
     if(fd == -1){
	  debug_printf(1,"Error opening socket ....\n");
	  return CI_SOCKET_ERROR;
     }

     icap_socket_opts(fd);

     addr.sin_family = AF_INET;
     addr.sin_port = htons(PORT);
     addr.sin_addr.s_addr = INADDR_ANY;

     if(bind(fd,(struct sockaddr *) &addr, sizeof(addr))){
	  debug_printf(1,"Error bind  \n");;
	  return CI_SOCKET_ERROR;
     }
     if(listen(fd, 5)){
	  debug_printf(1,"Error listen .....\n");
	  return CI_SOCKET_ERROR;
     }
     return fd;
}



int icap_socket_opts(ci_socket fd){
     struct linger li;
     int value,addrlen;
     /* if (fcntl(fd, F_SETFD, 1) == -1) {
        debug_printf(1,"can't set close-on-exec on server socket!");
	}
     */

     value = 1;
     if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1){
	  debug_printf(1,"setsockopt: unable to set SO_REUSEADDR\n");  
     }

     value = 1;
     if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,&value, sizeof (value)) == -1) {
	  debug_printf(1,"setsockopt: unable to set TCP_NODELAY\n");
     }

     li.l_onoff = 1;
     li.l_linger = MAX_SECS_TO_LINGER;
  
     if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		    (char *) &li, sizeof(struct linger)) < 0) {
	  debug_printf(1,"setsockopt: unable to set SO_LINGER \n");
     }
     return 1;
}




int icap_netio_init(int fd){
     fcntl(fd, F_SETFL, O_NONBLOCK ); //Setting newfd descriptor to nonblocking state....
}


int wait_for_data(int fd,int secs,int what_wait){
     fd_set fds,*rfds,*wfds;
     struct timeval tv,*tv_param;
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
	  debug_printf(5,"Fatal error while waiting for new data....\n");
     }
//     debug_printf(1,"ERROR!!!!!!!!!!!!! wait for data time out after %d secs %d usecs\n",
//		  tv.tv_sec,tv.tv_usec);
     return 0;
}

int check_for_keepalive_data(fd){
     return wait_for_data(fd,KEEPALIVE_TIMEOUT,wait_for_read);
}


int wait_for_incomming_data(int fd){
     return wait_for_data(fd,TIMEOUT,wait_for_read);
}


int wait_for_outgoing_data(int fd){
     return wait_for_data(fd,TIMEOUT,wait_for_write);
}



int icap_read(int fd,void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=read(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);
     
     if(bytes==-1 && errno==EAGAIN){

	  if(!wait_for_data(fd,TIMEOUT,wait_for_read)){
	       return bytes;
	  }

	  do{
	       bytes=read(fd,buf,count);
	  }while(bytes==-1 && errno==EINTR);
     }
     if(bytes==0){
//	  debug_printf(1,"What the helll!!!! No data to read, TIMEOUT:%d, errno:%d\n",TIMEOUT,errno);
	  return -1;
     }
     return bytes;
}


int icap_write(int fd, const void *buf,size_t count){
     int bytes=0;
     int remains=count;
     char *b= (char *)buf;

     while(remains>0){ //write until count bytes written
	  do{
	       bytes=write(fd,b,remains);
	  }while(bytes==-1 && errno==EINTR);
	  
	  if(bytes==-1 && errno==EAGAIN){
	       
	       if(!wait_for_data(fd,TIMEOUT,wait_for_write)){
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


int icap_read_nonblock(int fd, void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=read(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);
#ifdef __CYGWIN__/*  In linux and solaris not needed. A select function always called 
                     before this function*/
     if(bytes<0 && errno==EAGAIN)
	  return 0;
#endif
     return bytes;
}



int icap_write_nonblock(int fd, const void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=write(fd,buf,count);
     }while(bytes==-1 && errno==EINTR);
#ifdef __CYGWIN__    /*In linux and solaris not needed. A select function 
                      always called before this function*/
     if(bytes<0 && errno==EAGAIN)
	  return 0;
#endif
     return bytes;
}



int icap_linger_close(int fd){
     char buf[10];
     int ret;
     debug_printf(8,"Waiting to close connection\n");

     if(shutdown(fd,SHUT_WR)!=0){
	  close(fd);
	  return;
     }

     while(wait_for_data(fd,MAX_SECS_TO_LINGER,wait_for_read) && (ret=icap_read_nonblock(fd,buf,10))>0)
	  debug_printf(8,"OK I linger %d bytes.....\n",ret);

     close(fd);
     debug_printf(8,"Connection closed ...\n");
}


int icap_hard_close(int fd){
	  close(fd);
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
	  debug_printf("Readline error. Skip until eol ......\n");
	  while(icap_read(fd,&c,1)>0 && c!='\n');
     }
     return i;
     }
*/

