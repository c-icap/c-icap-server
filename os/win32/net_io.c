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
#include "debug.h"
#include "net_io.h"
#include "cfg_param.h"


int icap_socket_opts(ci_socket fd);


int windows_init(){
     WORD wVersionRequested;
     WSADATA wsaData;
     int err;
 
     wVersionRequested = MAKEWORD( 2, 2 );
     
     err = WSAStartup( wVersionRequested, &wsaData );
     if ( err != 0 ) {
	  return 0;
     }
     
     if ( LOBYTE( wsaData.wVersion ) != 2 ||
	  HIBYTE( wsaData.wVersion ) != 2 ) {
	  WSACleanup( );
	  return 0; 
     }
     return 1;
}


ci_socket icap_init_server(){
     ci_socket s;
     int er;
     struct sockaddr_in addr;

     if(!windows_init()){
	  debug_printf(1,"Error initialize windows sockets...\n");
     }  

     s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if(s == INVALID_SOCKET){
	  er=WSAGetLastError();
	  debug_printf(1,"Error opening socket ....%d\n",er);
	  return CI_SOCKET_ERROR;
     }

     icap_socket_opts(s);

     addr.sin_family = AF_INET;
     addr.sin_port = htons(PORT);
     addr.sin_addr.s_addr = INADDR_ANY;

     if(bind(s,(struct sockaddr *) &addr, sizeof(addr))){
	  debug_printf(1,"Error bind  \n");;
	  return CI_SOCKET_ERROR;
     }
     if(listen(s, 5)){
	  debug_printf(1,"Error listen .....\n");
	  return CI_SOCKET_ERROR;
     }
     return s;
}



int icap_socket_opts(ci_socket s){
     struct linger li;
     BOOL value;
/*
     value = TRUE;
     if(setsockopt(s, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, 
		   (const char *)&value, sizeof(value)) == -1){
	  debug_printf(1,"setsockopt: unable to set SO_CONDITIONAL_ACCEPT\n");  
     }
*/

     value = TRUE;
     if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&value, sizeof(value)) == -1){
	  debug_printf(1,"setsockopt: unable to set SO_REUSEADDR\n");  
     }

     value = TRUE;
     if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,(const char *)&value, sizeof (value)) == -1) {
	  debug_printf(1,"setsockopt: unable to set TCP_NODELAY\n");
     }

     li.l_onoff = 1;
     li.l_linger = MAX_SECS_TO_LINGER;
  
     if (setsockopt(s, SOL_SOCKET, SO_LINGER,
		    (const char *) &li, sizeof(struct linger)) < 0) {
	  debug_printf(1,"setsockopt: unable to set SO_LINGER \n");
     }
     return 1;
}





int icap_netio_init(ci_socket s){
     u_long val;
     val=1;
     ioctlsocket(s, FIONBIO, &val ); 
     return 1;
}



int wait_for_data(ci_socket fd,int secs,int what_wait){
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
	  debug_printf(1,"Fatal error while waiting for new data....\n");
     }
     return 0;
}

int check_for_keepalive_data(ci_socket fd){
     return wait_for_data(fd,KEEPALIVE_TIMEOUT,wait_for_read);
}


int wait_for_incomming_data(ci_socket fd){
     return wait_for_data(fd,TIMEOUT,wait_for_read);
}


int wait_for_outgoing_data(ci_socket fd){
     return wait_for_data(fd,TIMEOUT,wait_for_write);
}



int icap_read(ci_socket fd,void *buf,size_t count){
     int bytes=0,err=0;

     do{
	  bytes=recv(fd,buf,count,0);
     }while(bytes==SOCKET_ERROR && (err=WSAGetLastError())==WSAEINTR);
 
     if(bytes==SOCKET_ERROR && err==WSAEWOULDBLOCK){

	  if(!wait_for_data(fd,TIMEOUT,wait_for_read)){
	       return bytes;
	  }

	  do{
	       bytes=recv(fd,buf,count,0);
	  }while(bytes==SOCKET_ERROR && (err=WSAGetLastError())==WSAEINTR);
     }
     if(bytes==0){
	  debug_printf(1,"What the helll!!!! No data to read, TIMEOUT:%d, errno:%d\n",
		       TIMEOUT,errno);
	  return -1;
     }
     return bytes;
}


int icap_write(ci_socket fd, const void *buf,size_t count){
     int bytes=0;
     int err=0;
     int remains=count;
     char *b= (char *)buf;

     while(remains>0){ //write until count bytes written
	  do{
	       bytes=send(fd,b,remains,0);
	  }while(bytes==SOCKET_ERROR && (err=WSAGetLastError())==WSAEINTR);
	  printf("OK writing %d bytes %s\n",bytes,b);
	  if(bytes==SOCKET_ERROR && err==WSAEWOULDBLOCK){
	       
	       if(!wait_for_data(fd,TIMEOUT,wait_for_write)){
		    return bytes;
	       }
	       
	       do{
		    bytes=send(fd,b,remains,0);
	       }while(bytes==SOCKET_ERROR && (err=WSAGetLastError())==WSAEINTR);
	       
	  }
	  if(bytes<0)
	       return bytes;
	  b=b+bytes;//points to remaining bytes......
	  remains=remains-bytes;
     }//Ok......

     return count;
}


int icap_read_nonblock(ci_socket fd, void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=recv(fd,buf,count,0);
     }while(bytes==SOCKET_ERROR && WSAGetLastError()==WSAEINTR);

     return bytes;
}


int icap_write_nonblock(ci_socket fd, const void *buf,size_t count){
     int bytes=0;
     do{
	  bytes=send(fd,buf,count,0);
     }while(bytes==SOCKET_ERROR && WSAGetLastError()==WSAEINTR);

     return bytes;
}

int icap_linger_close(ci_socket fd){
     char buf[10];
     int ret;
     debug_printf(1,"Waiting to close connection\n");

     if(shutdown(fd,SD_SEND)!=0){
	  closesocket(fd);
	  return 1;
     }

     while(wait_for_data(fd,MAX_SECS_TO_LINGER,wait_for_read) && (ret=icap_read_nonblock(fd,buf,10))>0)
	  debug_printf(1,"OK I linger %d bytes.....\n",ret);

     closesocket(fd);
     debug_printf(1,"Connection closed ...\n");
     return 1;
}


int icap_hard_close(ci_socket fd){
     closesocket(fd);
     return 1;
}
