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
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include "request.h"

 
char *servername;
char *modulename;

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

     str=buf;

     printf("Sending :\n");
     while(len=readline(str,511,f)){
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




int main(int argc, char **argv){
     int i,fd,len,threadsnum;
     struct sockaddr_in addr;
     struct hostent *hent;
     char *filename;
     int port=1344;

     if(argc<3){
	  printf("Usage:\n%s servername module filename ...\n",argv[0]);
	  exit(1);
     }

     servername=argv[1];
     modulename=argv[2];
     filename=argv[3];
     
     fd = socket(AF_INET, SOCK_STREAM, 0);
     if(fd == -1){
	  printf("Error oppening socket ....\n");
	  exit( -1);
     }
     
     hent = gethostbyname(servername);
     if(hent == NULL)
	  addr.sin_addr.s_addr = inet_addr(servername);
     else
	  memcpy(&addr.sin_addr, hent->h_addr, hent->h_length);
     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);
     if(connect(fd, (struct sockaddr *)&addr, sizeof(addr))){
	  printf("Error connecting to socket .....\n");
	  exit(-1);
     }


     dofile(fd,filename,NULL);

     close(fd);      
     
}
