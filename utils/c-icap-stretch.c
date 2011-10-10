/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */

#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "request.h"
#include "ci_threads.h"
#include "simple_api.h"
#include "net_io.h"
#include "cfg_param.h"
#include "debug.h"



/*GLOBALS ........*/
int CONN_TIMEOUT = 30;
char *servername;
char *service;
int threadsnum = 0;
int MAX_REQUESTS = 0;

time_t START_TIME = 0;
int FILES_NUMBER;
char **FILES;
ci_thread_t *threads;
ci_thread_mutex_t filemtx;
int file_indx = 0;
int requests_stats = 0;
int failed_requests_stats = 0;
int soft_failed_requests_stats = 0;
int in_bytes_stats = 0;
int out_bytes_stats = 0;
int req_errors_rw = 0;
int req_errors_r = 0;
int _THE_END = 0;


ci_thread_mutex_t statsmtx;



void print_stats()
{
     time_t rtime;
     time(&rtime);
     printf("Statistics:\n\t Files used :%d\n\t Number of threads :%d\n\t"
            " Requests served :%d\n\t Requests failed :%d\n\t Requests soft failed :%d\n\t"
	    " Incoming bytes :%d\n\t Outgoing bytes :%d\n"
            " \t Write Errors :%d\n",
            FILES_NUMBER,
            threadsnum, requests_stats, 
	    failed_requests_stats, soft_failed_requests_stats, 
	    in_bytes_stats, out_bytes_stats,
            req_errors_rw);
     rtime = rtime - START_TIME;
     printf("Running for %u seconds\n", (unsigned int) rtime);
}


static void sigint_handler(int sig)
{
     int i = 0;

/* */
     signal(SIGINT, SIG_IGN);
     signal(SIGCHLD, SIG_IGN);

     if (sig == SIGTERM) {
          printf("SIGTERM signal received for main server.\n");
          printf("Going to term children....\n");

     }
     else if (sig == SIGINT) {
          printf("SIGINT signal received for icap-stretch.\n");

     }
     else {
          printf("Signal %d received. Exiting ....\n", sig);
     }
     _THE_END = 1;
     for (i = 0; i < threadsnum; i++) {
          if (threads[i])
               ci_thread_join(threads[i]);      //What if a child is blocked??????
     }

     ci_thread_mutex_destroy(&filemtx);

     print_stats();

     exit(0);
}



int threadjobreqmod()
{
     struct sockaddr_in addr;
     struct hostent *hent;
     int port = 1344;
     int fd;


     hent = gethostbyname(servername);
     if (hent == NULL)
          addr.sin_addr.s_addr = inet_addr(servername);
     else
          memcpy(&addr.sin_addr, hent->h_addr, hent->h_length);
     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);


     while (1) {
          fd = socket(AF_INET, SOCK_STREAM, 0);
          if (fd == -1) {
               printf("Error opening socket ....\n");
               return -1;
          }

          if (connect(fd, (struct sockaddr *) &addr, sizeof(addr))) {
               printf("Error connecting to socket .....\n");
               return -1;
          }

          for (;;) {
	    /*Do a new request */
//             sleep(4);
               sleep(1);
          }
          sleep(1);
          close(fd);
     }
}




void build_headers(int fd, ci_headers_list_t *headers)
{
     struct stat filestat;
     int filesize;
     char lbuf[512];
//     struct tm ltime;
     time_t ltimet;
     
     ci_headers_add(headers, "Filetype: Unknown");
     ci_headers_add(headers, "User: chtsanti");

     fstat(fd, &filestat);
     filesize = filestat.st_size;

     strcpy(lbuf, "Date: ");
     time(&ltimet);
     ctime_r(&ltimet, lbuf + strlen(lbuf));
     lbuf[strlen(lbuf) - 1] = '\0';
     ci_headers_add(headers, lbuf);

     strcpy(lbuf, "Last-Modified: ");
     ctime_r(&ltimet, lbuf + strlen(lbuf));
     lbuf[strlen(lbuf) - 1] = '\0';
     ci_headers_add(headers, lbuf);

     sprintf(lbuf, "Content-Length: %d", filesize);
     ci_headers_add(headers, lbuf);

}



int fileread(void *fd, char *buf, int len)
{
     int ret;
     ret = read(*(int *) fd, buf, len);
     return ret;
}

int filewrite(void *fd, char *buf, int len)
{
     return len;
}


int do_file(ci_request_t *req, char *input_file, int *keepalive)
{
     int fd_in,fd_out;
     int ret;
     ci_headers_list_t *headers;

     if ((fd_in = open(input_file, O_RDONLY)) < 0) {
          ci_debug_printf(1, "Error opening file %s\n", input_file);
          return 0;
     }
     fd_out = 0;

     headers = ci_headers_create();     
     build_headers(fd_in, headers);

     ret = ci_client_icapfilter(req,
				CONN_TIMEOUT,
				headers,
				&fd_in,
				(int (*)(void *, char *, int)) fileread,
				&fd_out,
				(int (*)(void *, char *, int)) filewrite);
     close(fd_in);

     if (ret <=0 && req->bytes_out == 0 ){
          ci_debug_printf(2, "Is the ICAP connection  closed?\n");
	  *keepalive = 0;
	  return 0;
     }
     
     if (ret<= 0) {
          ci_debug_printf(1, "Error sending requests \n");
	  *keepalive = 0;
	  return 0;
     }

     *keepalive=req->keepalive;

     ci_headers_destroy(headers);
     // printf("Done(%d bytes).\n",totalbytes);
     ci_thread_mutex_lock(&statsmtx);
     in_bytes_stats += req->bytes_in;
     out_bytes_stats += req->bytes_out;
     ci_thread_mutex_unlock(&statsmtx);

     return 1;
}


int threadjobsendfiles()
{
     ci_request_t *req;
     ci_connection_t *conn;
     int port = 1344;
     int indx, keepalive, ret;
     int arand;

     while (1) {

	  if (!(conn = ci_client_connect_to(servername, port, AF_INET))) {
	    ci_debug_printf(1, "Failed to connect to icap server.....\n");
	    exit(-1);
	  }
	  req = ci_client_request(conn, servername, service);
          req->type = ICAP_RESPMOD;
	  req->preview = 512;

          for (;;) {


               ci_thread_mutex_lock(&filemtx);
               indx = file_indx;
               if (file_indx == (FILES_NUMBER - 1))
                    file_indx = 0;
               else
                    file_indx++;
               ci_thread_mutex_unlock(&filemtx);

               keepalive = 0;
               if ((ret = do_file(req, FILES[indx], &keepalive)) <= 0) {
		    ci_thread_mutex_lock(&statsmtx);
		    if (ret == 0) 
		         soft_failed_requests_stats++;
		    else 
		         failed_requests_stats++;
		    ci_thread_mutex_unlock(&statsmtx);
                    printf("Request failed...\n");
                    break;
               }

               ci_thread_mutex_lock(&statsmtx);
               requests_stats++;
               arand = rand();  /*rand is not thread safe .... */
               ci_thread_mutex_unlock(&statsmtx);

               if (_THE_END) {
                    printf("The end: thread dying\n");
		    ci_request_destroy(req);
                    return 0;
               }

	       if (keepalive == 0)
		 break;

               arand = (int) ((((double) arand) / (double) RAND_MAX) * 10.0);
               if (arand == 5 || arand == 7 || arand == 3) {    // 30% possibility ....
//                  printf("OK, closing the connection......\n");
                    break;
               }
//               sleep(1);
               usleep(500000);

//             printf("Keeping alive connection\n");

	       ci_client_request_reuse(req);
          }
	  ci_hard_close(conn->fd);
	  ci_request_destroy(req);
          usleep(1000000);
     }
}




int main(int argc, char **argv)
{
     int i;

     if (argc < 4) {
          printf
              ("Usage:\n%s servername service threadsnum max_requests file1 file2 .....\n",
               argv[0]);
          exit(1);
     }
     signal(SIGPIPE, SIG_IGN);
     signal(SIGINT, sigint_handler);

     time(&START_TIME);
     srand((int) START_TIME);

     servername = argv[1];
     service = argv[2];

     threadsnum = atoi(argv[3]);
     if (threadsnum <= 0)
          return 0;


     if ((MAX_REQUESTS = atoi(argv[4])) < 0)
          MAX_REQUESTS = 0;

     threads = malloc(sizeof(ci_thread_t) * threadsnum);
     if (!threads) {
          ci_debug_printf(1, "Error allocation memory for threads array\n");
          exit(-1);
     }
     for (i = 0; i < threadsnum; i++)
          threads[i] = 0;

     if (argc > 4) {            //


          FILES = argv + 4;
          FILES_NUMBER = argc - 4;
          ci_thread_mutex_init(&filemtx);
          ci_thread_mutex_init(&statsmtx);

          printf("Files to send:%d\n", FILES_NUMBER);
          for (i = 0; i < threadsnum; i++) {
               printf("Create thread %d\n", i);
               ci_thread_create(&(threads[i]),
                                (void *(*)(void *)) threadjobsendfiles, NULL);
//             sleep(1);
          }

     }
     else {                     //Construct reqmod......
       exit(-1);


          for (i = 0; i < threadsnum; i++) {
               printf("Create thread %d\n", i);
               ci_thread_create(&(threads[i]),
                                (void *(*)(void *)) threadjobreqmod,
                                (void *) NULL /*data*/);
               sleep(1);
          }
     }



     while (1) {
          sleep(1);
          if (MAX_REQUESTS && requests_stats >= MAX_REQUESTS) {
               printf("Oops max requests reached. Exiting .....\n");
               _THE_END = 1;
               break;
          }
          print_stats();
     }



     for (i = 0; i < threadsnum; i++) {
          ci_thread_join(threads[i]);
          printf("Thread %d exited\n", i);
     }

     print_stats();
     ci_thread_mutex_destroy(&filemtx);
     ci_thread_mutex_destroy(&statsmtx);
     return 0;
}
