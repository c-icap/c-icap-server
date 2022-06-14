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
#include "acl.h"
#include "proc_threads_queues.h"
#include "commands.h"
#include <errno.h>
#include <assert.h>

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
{
    /*req can not be NULL */
    if (!req)
        return;

    if (default_logger)
        default_logger->log_access(req);
}

extern process_pid_t MY_PROC_PID;
void log_server(ci_request_t * req, const char *format, ...)
{
    /*req can be NULL......... */
    va_list ap;
    char prefix[64];
    va_start(ap, format);
    if (default_logger) {
        if (MY_PROC_PID)
            snprintf(prefix, 64, "%u/%lu", (unsigned int)MY_PROC_PID, (unsigned long int)ci_thread_self());
        else /*probably the main process*/
            strcpy(prefix, "main proc");

        default_logger->log_server(prefix, format, ap);    /*First argument must be changed..... */
    }
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

int logformat_add(const char *name, const char *format)
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
    } while (tmp);
    LOGFORMATS = NULL;
}

char *logformat_fmt(const char *name)
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
void file_log_server(const char *server, const char *format, va_list ap);
void file_log_relog(const char *name, int type, const char **argv);

/*char *LOGS_DIR = LOGDIR;*/
char *SERVER_LOG_FILE = LOGDIR "/cicap-server.log";
/*char *ACCESS_LOG_FILE = LOGDIR "/cicap-access.log";*/

struct logfile {
    char *file;
    FILE *access_log;
    const char *log_fmt;
    ci_access_entry_t *access_list;
    ci_thread_rwlock_t rwlock;
    struct logfile *next;
};
struct logfile *ACCESS_LOG_FILES = NULL;
static ci_thread_rwlock_t systemlog_rwlock;


logger_module_t file_logger = {
    "file_logger",
    NULL,
    file_log_open,
    file_log_close,
    file_log_access,
    file_log_server,
    NULL                       /*NULL configuration table */
};


FILE *server_log = NULL;

const char *DEFAULT_LOG_FORMAT = "%tl, %la %a %im %iu %is";

FILE *logfile_open(const char *fname)
{
    FILE *f = fopen(fname, "a+");
    if (f)
        setvbuf(f, NULL, _IONBF, 0);
    return f;
}

int file_log_open()
{
    int error = 0, ret = 0;
    struct logfile *lf;

    assert(ret == 0);
    register_command("relog", MONITOR_PROC_CMD | CHILDS_PROC_CMD, file_log_relog);

    for (lf = ACCESS_LOG_FILES; lf != NULL; lf = lf->next) {
        if (!lf->file) {
            ci_debug_printf (1, "This is a bug! lf->file==NULL\n");
            continue;
        }
        if (lf->log_fmt == NULL)
            lf->log_fmt = (char *)DEFAULT_LOG_FORMAT;

        if (ci_thread_rwlock_init(&(lf->rwlock)) != 0) {
            ci_debug_printf (1, "WARNING! Can not initialize structures for log file: %s\n", lf->file);
            continue;
        }

        lf->access_log = logfile_open(lf->file);
        if (!lf->access_log) {
            error = 1;
            ci_debug_printf (1, "WARNING! Can not open log file: %s\n", lf->file);
        }
    }

    ret = ci_thread_rwlock_init(&systemlog_rwlock);
    if (ret != 0)
        return 0;
    server_log = logfile_open(SERVER_LOG_FILE);
    if (!server_log)
        return 0;

    if (error)
        return 0;
    else
        return 1;
}

void file_log_close()
{
    struct logfile *lf, *tmp;

    lf = ACCESS_LOG_FILES;
    while (lf != NULL) {
        if (lf->access_log)
            fclose(lf->access_log);
        free(lf->file);
        if (lf->access_list)
            ci_access_entry_release(lf->access_list);
        ci_thread_rwlock_destroy(&(lf->rwlock)); // Initialize logfile::rwlock
        tmp = lf;
        lf = lf->next;
        ACCESS_LOG_FILES = lf;
        free(tmp);
    }

    if (server_log)
        fclose(server_log);
    server_log = NULL;
    ci_thread_rwlock_destroy(&systemlog_rwlock); // destroy rwlock
}

void file_log_relog(const char *name, int type, const char **argv)
{
    struct logfile *lf;

    /* This code should match the appropriate code from file_log_close */
    for (lf = ACCESS_LOG_FILES; lf != NULL; lf = lf->next) {
        ci_thread_rwlock_wrlock(&(lf->rwlock)); /*obtain a write lock. When this function returns all file_log_access will block until write unlock*/
        if (lf->access_log)
            fclose(lf->access_log);
        lf->access_log = logfile_open(lf->file);
        ci_thread_rwlock_unlock(&(lf->rwlock));

        if (!lf->access_log)
            ci_debug_printf (1, "WARNING! Can not open log file: %s\n", lf->file);
    }

    ci_thread_rwlock_wrlock(&systemlog_rwlock);
    if (server_log)
        fclose(server_log);
    server_log = logfile_open(SERVER_LOG_FILE);
    ci_thread_rwlock_unlock(&systemlog_rwlock);
    /*if !server_log ???*/
}

void file_log_access(ci_request_t *req)
{
    struct logfile *lf;
    char logline[4096];

    for (lf = ACCESS_LOG_FILES; lf != NULL; lf = lf->next) {
        if (lf->access_list && !(ci_access_entry_match_request(lf->access_list, req) == CI_ACCESS_ALLOW)) {
            ci_debug_printf(6, "access log file %s does not match, skiping\n", lf->file);
            continue;
        }
        ci_debug_printf(6, "Log request to access log file %s\n", lf->file);
        ci_format_text(req, lf->log_fmt, logline, sizeof(logline), NULL);

        ci_thread_rwlock_rdlock(&lf->rwlock); /*obtain a read lock*/
        if (lf->access_log)
            fprintf(lf->access_log,"%s\n", logline);
        ci_thread_rwlock_unlock(&lf->rwlock); /*obtain a read lock*/
    }
}


void file_log_server(const char *server, const char *format, va_list ap)
{
    char buf[1024];

    if (!server_log)
        return;

    ci_strtime(buf); /* requires STR_TIME_SIZE=64 bytes size */
    const size_t len = strlen(buf);
    const size_t written = snprintf(buf + len,  sizeof(buf) - len, ", %s, %s", server, format);
    assert(written < sizeof(buf) - len);
    ci_thread_rwlock_rdlock(&systemlog_rwlock); /*obtain a read lock*/
    vfprintf(server_log, buf, ap);
    ci_thread_rwlock_unlock(&systemlog_rwlock); /*release a read lock*/
}


int file_log_addlogfile(const char *file, const char *format, const char **acls)
{
    char *access_log_file, *access_log_format;
    const char *acl_name;
    struct logfile *lf, *newlf;
    int i;

    access_log_file = strdup(file);
    if (!access_log_file)
        return 0;

    if (format) {
        /*the folowing return format txt or NULL. It is OK*/
        access_log_format = logformat_fmt(format);
    } else
        access_log_format = NULL;

    newlf = malloc(sizeof(struct logfile));
    newlf->file = access_log_file;
    newlf->log_fmt = (access_log_format != NULL? access_log_format : DEFAULT_LOG_FORMAT);
    newlf->access_log = NULL;
    newlf->access_list = NULL;
    newlf->next = NULL;

    if (acls != NULL && acls[0] != NULL) {
        if (ci_access_entry_new(&(newlf->access_list), CI_ACCESS_ALLOW) == NULL) {
            ci_debug_printf(1, "Error creating access list for access log file %s!\n",
                            newlf->file);
            free(newlf->file);
            free(newlf);
            return 0;
        }
        for (i = 0; acls[i] != NULL; i++) {
            acl_name = acls[i];
            if (!ci_access_entry_add_acl_by_name(newlf->access_list, acl_name)) {
                ci_debug_printf(1, "Error addind acl %s to access list for access log file %s!\n",
                                acl_name, newlf->file);
                ci_access_entry_release(newlf->access_list);
                free(newlf->file);
                free(newlf);
                return 0;
            }
        }
    }

    if (!ACCESS_LOG_FILES) {
        ACCESS_LOG_FILES = newlf;
    } else {
        for (lf = ACCESS_LOG_FILES; lf->next != NULL; lf = lf->next) {
            if (strcmp(lf->file, newlf->file)==0) {
                ci_debug_printf(1, "Access log file %s already defined!\n",
                                newlf->file);
                if (newlf->access_list)
                    ci_access_entry_release(newlf->access_list);
                free(newlf->file);
                free(newlf);
                return 0;
            }
        }

        lf->next = newlf;
    }

    return 1;
}
