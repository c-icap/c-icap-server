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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <ci_threads.h>


char *servername;
int keepalive=0;
int FILES_NUMBER;
char **FILES;



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


int icap_readline(int fd,char *buf,int BUFSIZE){
     int i=0,readed=0;
     char c,oc=0;
     while((readed=read(fd,&c,1))>0 && c!='\n'  && i<BUFSIZE ){
          if(c=='\r'){
               read(fd,&c,1);
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
          printf("Readline error. Skip until eol ......\n");
          while(read(fd,&c,1)>0 && c!='\n');
     }
     return i;
}



struct buffer{
     char *buf;
     int size;
};



int readheaderresponce(int fd){
     char lbuf[512],*tmpstr;
     int len,bytes,startbody=0,remainbytes=-1;
     int blocks=0;
     bytes=0;
     printf("\n\nRESPONCE: ");
     while(len=read(fd,lbuf,512)){
	  bytes+=len;
	  lbuf[len]='\0';

	  if((tmpstr=strstr(lbuf,"null-body="))!=NULL){
	       tmpstr+=10;
	       startbody=atoi(tmpstr);
	  }

	  if((tmpstr=strstr(lbuf,"\r\n\r\n"))!=NULL){
	       remainbytes=bytes-(tmpstr+4-lbuf);
	       blocks++;
	  }
	  

//	  printf("(BYTES=%d)\n%s",len,lbuf);
//	  printf("%s",lbuf);
	  printf("Readed bytes:%d, startbody at:%d, remain bytes:%d, blocks:%d\n",
		 bytes,startbody,remainbytes,blocks);;
	  if(blocks==2)
	       return bytes;
     }

}

int readallresponce(int fd){
     char c,cprev,lbuf[512];
     int len,bytes,remains,toread,i,totalbytes;
     i=0;
     while(1){
	  icap_readline(fd,lbuf,512);
	  if(strlen(lbuf)==0)
	       i++;
	  if(i==2)
	       break;
     }
     printf("OK headers readed...\n");
     totalbytes=0;
     while(1){
	  c=0;cprev=0;
	  for(i=0;!(c=='\n' && cprev=='\r');i++){
	       cprev=c;
	       if(i>10 || read(fd,&c,1)<=0){
		    lbuf[i]='\0';
		    printf("Error.Exiting i:%d, lbuf:%s",i,lbuf);
		    return 0;
	       }
	       lbuf[i]=c;
	  }
	  len=strtol(lbuf,NULL,16);
//	  printf("Going to read %d bytes\n",len);
	  if(len==0){
	       read(fd,&c,1);
	       read(fd,&c,1);
	       return totalbytes;
	  }
	  totalbytes+=len;
	  remains=len+2;
	  while(remains>0){
	       toread=(remains>512?512:remains);
	       len=read(fd,lbuf,toread);
	       remains=remains-len;
	  }
     }
     
}



int threadjobreqmod(struct buffer *buf){
     struct sockaddr_in addr;
     struct hostent *hent;
     int port=1344;
     int fd;
     
     
     hent = gethostbyname(servername);
     if(hent == NULL)
	  addr.sin_addr.s_addr = inet_addr(servername);
     else
	  memcpy(&addr.sin_addr, hent->h_addr, hent->h_length);
     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);

     
     while(1){
	  fd = socket(AF_INET, SOCK_STREAM, 0);
	  if(fd == -1){
	       printf("Error oppening socket ....\n");
	       return -1;
	  }
	  
	  if(connect(fd, (struct sockaddr *)&addr, sizeof(addr))){
	       printf("Error connecting to socket .....\n");
	       return -1;
	  }
	  
	  for(;;){	  
	       if(icap_write(fd,buf->buf,buf->size)<0){
		    printf("Connection closed try again...\n");
		    break;
	       }
	       readheaderresponce(fd);
	       if(!keepalive)
		    break;
//	       sleep(4);
	       sleep(1);
	  }
	  sleep(1);
	  close(fd); 
     }
}


const char *reqheader="GET /manual/invoking.html HTTP/1.0\r\n"\
                      "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.4) Gecko/20030624\r\n"\
                      "Accept: text/xml,application/xml,application/xhtml+xml,text/html;\r\n"\
                      "Accept-Language: en-us,en;q=0.5\r\n"\
                      "Accept-Encoding: gzip,deflate\r\n"\
                      "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"\
                      "Keep-Alive: 300\r\n"\
                      "Referer: http://barbarian.athens.home.gr/manual/\r\n"\
                      "Via: 1.1 fortune.athens.home.gr:8080 (squid/2.5.STABLE1)\r\n"\
                      "X-Forwarded-For: 192.168.1.4\r\n"\
                      "Host: barbarian.athens.home.gr\r\n"\
                      "Cache-Control: max-age=259200\r\n";



void buildreqheaders(char *buf){
     char lstr[128];
     strcpy(buf,"REQMOD icap://");
     strcat(buf,servername);
     strcat(buf,":1344/");
     strcat(buf,"echo");//modulename better
     strcat(buf," ICAP/1.0\r\n");
     sprintf(lstr,"Encapsulated: req-hdr=0, null-body=%d\r\n",strlen(reqheader));
     strcat(buf,lstr);
     strcat(buf,"Connection: keep-alive\r\n\r\n");
     strcat(buf,reqheader);
//     strcat(buf,"\r\n");
     keepalive=1;
}



void buildrespmodfile(FILE *f,char *buf){
     struct stat filestat;
     int filesize;
     char lbuf[512];
//     struct tm ltime;
     time_t ltimet;
     fstat(fileno(f),&filestat);
     filesize=filestat.st_size;
     strcpy(lbuf,"HTTP/1.1 200 OK\r\n");
     strcat(lbuf,"Date: ");
     time(&ltimet);
     ctime_r(&ltimet,lbuf+strlen(lbuf));
     lbuf[strlen(lbuf)-1]='\0';
     strcat(lbuf,"\r\n");
     strcat(lbuf,"Last-Modified: ");
     ctime_r(&ltimet,lbuf+strlen(lbuf));
     lbuf[strlen(lbuf)-1]='\0';
     strcat(lbuf,"\r\nContent-Length: ");
     sprintf(lbuf+strlen(lbuf),"%d\r\n",filesize);

     strcat(lbuf,"\r\n");

     strcpy(buf,"RESPMOD icap://");
     strcat(buf,servername);
     strcat(buf,":1344/");
     strcat(buf,"echo");//modulename better
     strcat(buf," ICAP/1.0\r\n");
     sprintf(buf+strlen(buf),"Encapsulated: res-hdr=0, res-body=%d\r\n",strlen(lbuf));
     strcat(buf,"Connection: keep-alive\r\n\r\n");
     strcat(buf,lbuf);
     keepalive=1;
     printf(buf);
}



int do_file(int fd, char *filename){
     FILE *f;
     char lg[10],lbuf[512],tmpbuf[522];
     int bytes,len,totalbytes;

     if((f=fopen(filename,"r"))==NULL)
	  return 0;

     buildrespmodfile(f,lbuf);
     
     if(icap_write(fd,lbuf,strlen(lbuf))<0)
	  return 0;


     printf("Sending file:\n");
     
     totalbytes=0;
     while((len=fread(lbuf,sizeof(char),512,f))>0){
	  totalbytes+=len;
	  bytes=sprintf(lg,"%X\r\n",len);
	  icap_write(fd,lg,bytes);
	  icap_write(fd,lbuf,len);
	  icap_write(fd,"\r\n",2);
//	  printf("Sending chunksize :%d\n",len);
     }
     icap_write(fd,"0\r\n\r\n",5);
     fclose(f);
     printf("Done(%d bytes). Reading responce.....\n",totalbytes);
     totalbytes=readallresponce(fd);
     printf("Done(%d bytes).\n",totalbytes);
     
}

ci_thread_mutex_t filemtx;
int file_indx=0;

int threadjobsendfiles(){
     struct sockaddr_in addr;
     struct hostent *hent;
     int port=1344;
     int fd,indx;
     
     
     hent = gethostbyname(servername);
     if(hent == NULL)
	  addr.sin_addr.s_addr = inet_addr(servername);
     else
	  memcpy(&addr.sin_addr, hent->h_addr, hent->h_length);
     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);

     
     while(1){
	  fd = socket(AF_INET, SOCK_STREAM, 0);
	  if(fd == -1){
	       printf("Error oppening socket ....\n");
	       return -1;
	  }
	  
	  if(connect(fd, (struct sockaddr *)&addr, sizeof(addr))){
	       printf("Error connecting to socket .....\n");
	       return -1;
	  }
	  
	  for(;;){	


	       ci_thread_mutex_lock(&filemtx);
	       indx=file_indx;
	       if(file_indx==(FILES_NUMBER-1))
		    file_indx=0;
	       else
		    file_indx++;
	       ci_thread_mutex_unlock(&filemtx);
	       
	       if(do_file(fd,FILES[indx])==0)
		    break;
//	       sleep(1);
	  }
	  close(fd);
     }
}




int main(int argc, char **argv){
     int i,fd,len,threadsnum;
     struct buffer buf;
     char *str;
     FILE *f;
     ci_thread_t *threads;

     if(argc<3){
	  printf("Usage:\n%s servername theadsnum filename\n",argv[0]);
	  exit(1);
     }
     signal(SIGPIPE,SIG_IGN);

     servername=argv[1];
	  
     threadsnum=atoi(argv[2]);
     if(threadsnum<=0)
	  return 0;

     threads=malloc(sizeof(ci_thread_t)*threadsnum);

     
     if(argc>3){ //

	  FILES=argv+3;
	  FILES_NUMBER=argc-3;
	  ci_thread_mutex_init(&filemtx);
	  printf("Files to send:%d\n",FILES_NUMBER);
	  for(i=0;i<threadsnum;i++){
	       ci_thread_create(&(threads[i]),threadjobsendfiles,NULL);	  
	       sleep(1);
	  }	  

     }
     else{ //Construct reqmod......
	  buf.buf=malloc(1024*sizeof(char));
	  buildreqheaders(buf.buf);
	  buf.size=strlen(buf.buf);
	  
	  for(i=0;i<threadsnum;i++){
	       ci_thread_create(&(threads[i]),threadjobreqmod,(void *)&buf);	  
	       sleep(1);
	  }
     }


     for(i=0;i<threadsnum;i++){
	  ci_thread_join(threads[i]);	  
     }

     ci_thread_mutex_destroy(&filemtx);
     
}
