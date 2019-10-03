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
#include <syslog.h>
#include "log.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include "txt_format.h"
#include "access.h"
#include "acl.h"


/************************************************************************/
/*  sys_logger implementation.                                          */
/*                                                                      */

int sys_log_open();
void sys_log_close();
void sys_log_access(ci_request_t *req);
void sys_log_server(const char *server, const char *format, va_list ap);

char *log_ident = "c-icap: ";
static int FACILITY = LOG_DAEMON;
static int ACCESS_PRIORITY = LOG_INFO;
static int SERVER_PRIORITY = LOG_CRIT;
char *syslog_logformat = "%la %a %im %iu %is";
static ci_access_entry_t *syslog_access_list = NULL;


int cfg_set_facility(const char *directive, const char **argv, void *setdata);
int cfg_set_priority(const char *directive, const char **argv, void *setdata);
/*int cfg_set_prefix(const char *directive,const char **argv,void *setdata);*/
int cfg_syslog_logformat(const char *directive, const char **argv, void *setdata);
int cfg_syslog_access(const char *directive, const char **argv, void *setdata);


/*
   functions declared in log.c. This file is not included in c-icap library
   but defined in primary c-icap binary.
*/
extern char *logformat_fmt(const char *name);

/*Configuration Table .....*/
static struct ci_conf_entry conf_variables[] = {
    {"Facility", NULL, cfg_set_facility, NULL},
    {"access_priority", &ACCESS_PRIORITY, cfg_set_priority, NULL},
    {"server_priority", &SERVER_PRIORITY, cfg_set_priority, NULL},
    {"Prefix", &log_ident, ci_cfg_set_str, NULL},
    {"LogFormat", NULL, cfg_syslog_logformat},
    {"access", NULL, cfg_syslog_access},
    {NULL, NULL, NULL, NULL}
};



CI_DECLARE_MOD_DATA logger_module_t module = {
    "sys_logger",
    NULL,
    sys_log_open,
    sys_log_close,
    sys_log_access,
    sys_log_server,
    conf_variables
};


int cfg_set_facility(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL) {
//        ci_debug_printf(1,"Missing arguments in directive\n");
        return 0;
    }
    if (strcmp(argv[0], "daemon") == 0)
        FACILITY = LOG_DAEMON;
    else if (strcmp(argv[0], "user") == 0)
        FACILITY = LOG_USER;
    else if (strncmp(argv[0], "local", 5) == 0 && strlen(argv[0]) == 6) {
        switch (argv[0][5]) {
        case '0':
            FACILITY = LOG_LOCAL0;
            break;
        case '1':
            FACILITY = LOG_LOCAL1;
            break;
        case '2':
            FACILITY = LOG_LOCAL2;
            break;
        case '3':
            FACILITY = LOG_LOCAL3;
            break;
        case '4':
            FACILITY = LOG_LOCAL4;
            break;
        case '5':
            FACILITY = LOG_LOCAL5;
            break;
        case '6':
            FACILITY = LOG_LOCAL6;
            break;
        case '7':
            FACILITY = LOG_LOCAL7;
            break;
        }
    }
    return 1;
}

int cfg_set_priority(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive\n");
        return 0;
    }
    if (!setdata)
        return 0;

    if (strcmp(argv[0], "alert") == 0)
        *((int *) setdata) = LOG_ALERT;
    else if (strcmp(argv[0], "crit") == 0)
        *((int *) setdata) = LOG_CRIT;
    else if (strcmp(argv[0], "debug") == 0)
        *((int *) setdata) = LOG_DEBUG;
    else if (strcmp(argv[0], "emerg") == 0)
        *((int *) setdata) = LOG_EMERG;
    else if (strcmp(argv[0], "err") == 0)
        *((int *) setdata) = LOG_ERR;
    else if (strcmp(argv[0], "info") == 0)
        *((int *) setdata) = LOG_INFO;
    else if (strcmp(argv[0], "notice") == 0)
        *((int *) setdata) = LOG_NOTICE;
    else if (strcmp(argv[0], "warning") == 0)
        *((int *) setdata) = LOG_WARNING;
    return 1;
}

int cfg_syslog_logformat(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive\n");
        return 0;
    }
    /* the folowing return format txt or NULL. It is OK */
    syslog_logformat = logformat_fmt(argv[0]);
    return 1;
}

int sys_log_open()
{
    openlog(log_ident, 0, FACILITY);
    return 1;
}

void sys_log_close()
{
    closelog();
    if (syslog_access_list)
        ci_access_entry_release(syslog_access_list);
    syslog_access_list = NULL;
}



void sys_log_access(ci_request_t *req)
{
    char logline[1024];
    if (!syslog_logformat)
        return;

    if (syslog_access_list && !(ci_access_entry_match_request(syslog_access_list, req) == CI_ACCESS_ALLOW)) {
        ci_debug_printf(6, "Access list for syslog access does not match\n");
        return;
    }

    ci_format_text(req, syslog_logformat, logline, 1024, NULL);

    syslog(ACCESS_PRIORITY, "%s\n", logline);
}


void sys_log_server(const char *server, const char *format, va_list ap)
{
    char buf[512];
    char prefix[150];

    snprintf(prefix, sizeof(prefix), "%s, %s ", server, format);
    vsnprintf(buf, sizeof(buf), (const char *) prefix, ap);
    syslog(SERVER_PRIORITY, "%s", buf);
}


int cfg_syslog_access(const char *directive, const char **argv, void *setdata)
{
    int argc, error;
    const char *acl_spec_name;

    if (argv[0] == NULL) {
        ci_debug_printf(1, "Parse error in directive %s \n", directive);
        return 0;
    }

    if (ci_access_entry_new(&syslog_access_list, CI_ACCESS_ALLOW) == NULL) {
        ci_debug_printf(1, "Error creating access list for syslog logger!\n");
        return 0;
    }

    ci_debug_printf(1,"Creating new access entry for syslog module with specs:\n");
    error = 0;
    for (argc = 0; argv[argc] != NULL; argc++) {
        acl_spec_name = argv[argc];
        /*TODO: check return type.....*/
        if (!ci_access_entry_add_acl_by_name(syslog_access_list, acl_spec_name)) {
            ci_debug_printf(1,"Error adding acl spec: %s. Probably does not exist!\n", acl_spec_name);
            error = 1;
        } else
            ci_debug_printf(1,"\tAdding acl spec: %s\n", acl_spec_name);
    }

    if (error)
        return 0;

    return 1;
}
