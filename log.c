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
     if (!req)
          return;

     if (access_check_logging(req) == CI_ACCESS_ALLOW)
          return;
     if (default_logger)
	  default_logger->log_access(req);
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
void file_log_access(ci_request_t *req);
void file_log_server(char *server, const char *format, va_list ap);

/*char *SERVER_LOG_FILE="var/log/server.log";
char *ACCESS_LOG_FILE="var/log/access.log";
*/

/*char *LOGS_DIR=LOGDIR;*/
char *SERVER_LOG_FILE = LOGDIR "/cicap-server.log";
char *ACCESS_LOG_FILE = LOGDIR "/cicap-access.log";

char *ACCESS_LOG_FORMAT = "%tl, %la %a %im %iu %is";


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


void file_log_access(ci_request_t *req)
{
    char logline[1024];
    if (!access_log)
          return;
    ci_format_text(req, ACCESS_LOG_FORMAT, logline, 1024, NULL);
    fprintf(access_log,"%s\n", logline); 
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
