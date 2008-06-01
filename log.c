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
#include "log.h"
#include "util.h"
#include "access.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include <errno.h>

logger_module_t *default_logger = NULL;


int log_open()
{
     if (default_logger)
          return default_logger->log_open();
     return 0;
}


void log_close()
{
     if (default_logger) {
          default_logger->log_close();
     }
}

void log_reset()
{
     default_logger = NULL;
}

void log_access(ci_request_t * req, int status)
{                               /*req can not be NULL */
     char serverip[CI_IPLEN], clientip[CI_IPLEN];
     if (!req)
          return;

     if (access_check_logging(req) == CI_ACCESS_ALLOW)
          return;

     if (!ci_conn_remote_ip(req->connection, clientip))
	  strcpy(clientip, "-" );
     if (!ci_conn_local_ip(req->connection, serverip))
	  strcpy(serverip, "-");

     if (default_logger)
          default_logger->log_access(serverip,
                                     clientip,
                                     (char *) ci_method_string(req->type),
                                     req->service,
                                     req->args,
                                     (status == CI_OK ? "OK" : "ERROR"));
}


void log_server(ci_request_t * req, const char *format, ...)
{                               /*req can be NULL......... */
     va_list ap;
     va_start(ap, format);
     if (default_logger)
          default_logger->log_server("general", format, ap);    /*First argument must be changed..... */
     va_end(ap);
}

void vlog_server(ci_request_t * req, const char *format, va_list ap)
{
     if (default_logger)
          default_logger->log_server("", format, ap);
}


/****************************************************************************************/
/*  file_logger implementation. This is the default logger                              */
/*                                                                                      */

int file_log_open();
void file_log_close();
void file_log_access(char *server, char *clientname, char *method,
                     char *request, char *args, char *status);
void file_log_server(char *server, const char *format, va_list ap);

/*char *SERVER_LOG_FILE="var/log/server.log";
char *ACCESS_LOG_FILE="var/log/access.log";
*/

/*char *LOGS_DIR=LOGDIR;*/
char *SERVER_LOG_FILE = LOGDIR "/cicap-server.log";
char *ACCESS_LOG_FILE = LOGDIR "/cicap-access.log";

logger_module_t file_logger = {
     "file_logger",
     NULL,
     file_log_open,
     file_log_close,
     file_log_access,
     file_log_server,
     NULL                       /*NULL configuration table */
};


FILE *access_log = NULL;
FILE *server_log = NULL;


int file_log_open()
{

     access_log = fopen(ACCESS_LOG_FILE, "a+");
     server_log = fopen(SERVER_LOG_FILE, "a+");

     if (!access_log || !server_log)
          return 0;
     setvbuf(access_log, NULL, _IONBF, 0);
     setvbuf(server_log, NULL, _IONBF, 0);

     return 1;
}

void file_log_close()
{
     if (access_log)
          fclose(access_log);
     if (server_log)
          fclose(server_log);
     access_log = NULL;
     server_log = NULL;
}


/*
void log_flush(){
     if(access_log)
	  fflush(access_log);
     
     if(server_log)
	  fflush(server_log);
}
*/


void file_log_access(char *server, char *clientname, char *method,
                     char *request, char *args, char *status)
{
     char buf[STR_TIME_SIZE];
     if (!access_log)
          return;

     ci_strtime(buf);
     fprintf(access_log, "%s, %s, %s, %s, %s%c%s, %s\n", buf, server,
             clientname, method, request, (args == NULL ? ' ' : '?'),
             (args == NULL ? "" : args), status);
}


void file_log_server(char *server, const char *format, va_list ap)
{
     char buf[STR_TIME_SIZE];

     if (!server_log)
          return;

     ci_strtime(buf);
     fprintf(server_log, "%s, %s, ", buf, server);
     vfprintf(server_log, format, ap);
//     fprintf(server_log,"\n");
}
