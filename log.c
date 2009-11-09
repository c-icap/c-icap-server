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
#include "txt_format.h"
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
/*  logformat            */
/*   
     Maybe logformat manipulation functions should moved to the c-icap library, 
     because some platforms (eg. MS-WINDOWS) can not use functions and objects 
     defined in main executable. At this time ony the sys_logger.c module uses these
     functions which make sense on unix platforms (where there is not a such problem).
     Moreover the sys_logger can be compiled inside c-icap main executable to avoid
     such problems.
*/

struct logformat {
    char *name;
    char *fmt;
    struct logformat *next;
};

struct logformat *LOGFORMATS = NULL;

int logformat_add(char *name, char *format)
{
  struct logformat *lf, *tmp;
  lf = malloc(sizeof(struct logformat));
  if (!lf) {
     ci_debug_printf(1, "Error allocating memory in add_logformat\n");
     return 0;
  }
  lf->name = strdup(name);
  lf->fmt = strdup(format);

  if (!lf->name || !lf->fmt) {
      ci_debug_printf(1, "Error strduping in add_logformat\n");
      free(lf);
      return 0;
  }

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
   LOGFORMATS = NULL;
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
//char *ACCESS_LOG_FILE = NULL;

//char *ACCESS_LOG_FORMAT = NULL;

struct logfile {
    char *file;
    FILE *access_log;
    const char *log_fmt;
    /*acl_t *acls;*/
    struct logfile *next;
};
struct logfile *ACCESS_LOG_FILES = NULL;


logger_module_t file_logger = {
     "file_logger",
     NULL,
     file_log_open,
     file_log_close,
     file_log_access,
     file_log_server,
     NULL                       /*NULL configuration table */
};


//FILE *access_log = NULL;
FILE *server_log = NULL;

const char *DEFAULT_LOG_FORMAT = "%tl, %la %a %im %iu %is";

int file_log_open()
{
     int error=0;
     struct logfile *lf;
  /*
     if (!ACCESS_LOG_FORMAT)
         ACCESS_LOG_FORMAT = "%tl, %la %a %im %iu %is";
     if (ACCESS_LOG_FILE)  {
        access_log = fopen(ACCESS_LOG_FILE, "a+");
        if (!access_log)
            return 0;
        setvbuf(access_log, NULL, _IONBF, 0);
     }
  */
     for (lf = ACCESS_LOG_FILES; lf != NULL; lf = lf->next) {
          if (!lf->file) {
	       ci_debug_printf (1, "This is a bug! lf->file==NULL\n");
	       continue;
	  }
	  if (lf->log_fmt == NULL)
	      lf->log_fmt = (char *)DEFAULT_LOG_FORMAT;

	  lf->access_log = fopen(lf->file, "a+");
	  if (!lf->access_log) {
	      error = 1;
	      ci_debug_printf (1, "WARNING! Can not open log file: %s\n", lf->file);
	  }
	  else {
	      setvbuf(lf->access_log, NULL, _IONBF, 0);
	  }
     }

     server_log = fopen(SERVER_LOG_FILE, "a+");
     if (!server_log)
          return 0;
     setvbuf(server_log, NULL, _IONBF, 0);

     if (error)
         return 0;
     else
         return 1;
}

void file_log_close()
{
     struct logfile *lf, *tmp;

     lf = ACCESS_LOG_FILES;
     while(lf != NULL) {
          if (lf->access_log)
	      fclose(lf->access_log);
	  free(lf->file);

	  ACCESS_LOG_FILES = lf;
	  tmp = lf;
	  lf = lf->next;
	  free(tmp);
     }

     if (server_log)
          fclose(server_log);
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
    struct logfile *lf;
    char logline[1024];

    for (lf = ACCESS_LOG_FILES; lf != NULL; lf = lf->next) {
         if (lf->access_log) {
             ci_format_text(req, lf->log_fmt, logline, 1024, NULL);
	     fprintf(lf->access_log,"%s\n", logline); 
	 }
    }
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
     char *access_log_file, *access_log_format;
     struct logfile *lf, *newlf;

     access_log_file = strdup(file);
     if (!access_log_file)
       return 0;

     if (format) {
         /*the folowing return format txt or NULL. It is OK*/
         access_log_format = logformat_fmt(format);
     }
     else
         access_log_format = NULL;

     newlf = malloc(sizeof(struct logfile));
     newlf->file = access_log_file;
     newlf->log_fmt = (access_log_format != NULL? access_log_format : DEFAULT_LOG_FORMAT);
     newlf->access_log = NULL;
     newlf->next = NULL;
     
     if (!ACCESS_LOG_FILES){
         ACCESS_LOG_FILES = newlf;
     } 
     else {
	 for (lf = ACCESS_LOG_FILES; lf->next != NULL; lf = lf->next) {
              if (strcmp(lf->file, newlf->file)==0) {
                  ci_debug_printf(1, "Access log file %s already defined!\n",
                                  newlf->file);
                  free(newlf->file);
                  free(newlf);
                  return 0;
              }
         }

	 lf->next = newlf;
     }

     return 1;
}
