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


void logformat_release();

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
     logformat_release();
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

/*************************************************************/
/* logformat */
struct logformat {
    char *name;
    char *fmt;
    struct logformat *next;
};

struct logformat *LOGFORMATS=NULL;

int logformat_add(char *name, char *format)
{
  struct logformat *lf, *tmp;
  lf = malloc(sizeof(struct logformat));
  if (!lf) {
     ci_debug_printf(1, "Error allocation memory in add_logformat\n");
     return 0;
  }
  lf->name = strdup(name);
  lf->fmt = strdup(format);
  lf->next = NULL;
  if (LOGFORMATS==NULL) {
     LOGFORMATS = lf;  
     return 1;
  }
  tmp = LOGFORMATS;
  while (tmp->next != NULL)  
         tmp = tmp->next;
  tmp->next = lf;
  return 1; 
}

void logformat_release() 
{
   struct logformat *cur, *tmp;

   if (!(tmp = LOGFORMATS))
      return;

   do {
         cur = tmp;
         tmp = tmp->next;
         free(cur->name);
         free(cur->fmt); 
         free(cur);
   } while(tmp);
}

char *logformat_fmt(char *name)
{
    struct logformat *tmp;
    if (!(tmp = LOGFORMATS))
      return NULL;

    while (tmp) {
       if (strcmp(tmp->name, name) == 0)
           return tmp->fmt;
       tmp = tmp->next;
    }
    return NULL;
}


/******************************************************************/
/*  file_logger implementation. This is the default logger        */
/*                                                                */

int file_log_open();
void file_log_close();
void file_log_access(ci_request_t *req);
void file_log_server(char *server, const char *format, va_list ap);

/*char *LOGS_DIR=LOGDIR;*/
char *SERVER_LOG_FILE = LOGDIR "/cicap-server.log";
/*char *ACCESS_LOG_FILE = LOGDIR "/cicap-access.log";*/
char *ACCESS_LOG_FILE = NULL;

char *ACCESS_LOG_FORMAT = NULL;

/* When we are going to support multiple access log files 
 We are going to use  structure like the following
*/
/*
struct logfile {
    char *file;
    FILE *access_log;
    char *log_fmt;
    acl_t *acls;
    struct logfile *next;
};
struct logfile *ACCESS_LOG_FILES = NULL;
*/

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
     if (!ACCESS_LOG_FORMAT)
         ACCESS_LOG_FORMAT = "%tl, %la %a %im %iu %is";
     if (ACCESS_LOG_FILE)  {
        access_log = fopen(ACCESS_LOG_FILE, "a+");
        if (!access_log)
            return 0;
        setvbuf(access_log, NULL, _IONBF, 0);
     }

     server_log = fopen(SERVER_LOG_FILE, "a+");
     if (!server_log)
          return 0;
     setvbuf(server_log, NULL, _IONBF, 0);

     return 1;
}

void file_log_close()
{
     ACCESS_LOG_FORMAT = NULL;
     ACCESS_LOG_FILE = NULL;

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


int file_log_addlogfile(char *file, char *format, char **acls) 
{
     ACCESS_LOG_FILE = strdup(file);
     if (format) {
         /*the folowing return format txt or NULL. It is OK*/
         ACCESS_LOG_FORMAT = logformat_fmt(format);
     }
     else
         ACCESS_LOG_FORMAT = NULL;
     return 1;
}
