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
#include "net_io.h"
#include "debug.h"
#include "module.h"
#include "log.h"
#include "cfg_param.h"
#include "filetype.h"
#include "acl.h"
#include "txtTemplate.h"
#include "commands.h"

/*
extern char *PIDFILE;
extern char *RUN_USER;
extern char *RUN_GROUP;
extern int PORT;
*/

extern int DAEMON_MODE;
extern int MAX_SECS_TO_LINGER;
char MY_HOSTNAME[CI_MAXHOSTNAMELEN + 1];

void init_conf_tables();
int init_body_system();
int config(int, char **);
int init_server();
int start_server();
int store_pid(char *pidfile);
int clear_pid(char *pidfile);
int is_icap_running(char *pidfile);
int set_running_permissions(char *user, char *group);
void init_internal_lookup_tables();
void request_stats_init();
int mem_init();
void init_http_auth();

void compute_my_hostname()
{
    char hostname[64];
    struct hostent *hent;
    int ret;
    ret = gethostname(hostname, 63);
    if (ret == 0) {
        hostname[63] = '\0';
        if ((hent = gethostbyname(hostname)) != NULL) {
            strncpy(MY_HOSTNAME, hent->h_name, CI_MAXHOSTNAMELEN);
            MY_HOSTNAME[CI_MAXHOSTNAMELEN] = '\0';
        } else
            strcpy(MY_HOSTNAME, hostname);
    } else
        strcpy(MY_HOSTNAME, "localhost");
}

#if ! defined(_WIN32)
void run_as_daemon()
{
    int fd;
    int pid, sid;
    pid = fork();
    if (pid < 0) {
        ci_debug_printf(1, "Unable to fork. exiting...");
        exit(-1);
    }
    if (pid > 0)
        exit(0);
    /* Change the file mode mask */
    umask(0);
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        ci_debug_printf(1, "Unable to create a new SID for the main process. exiting...");
        exit(-1);
    }
    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        ci_debug_printf(1, "Unable to change the working directory. exiting...");
        exit(-1);
    }

    /* Direct standard file descriptors to "/dev/null"*/
    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        ci_debug_printf(1, "Unable to open '/dev/null'. exiting...");
        exit(-1);
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        ci_debug_printf(1, "Unable to set stdin to '/dev/null'. exiting...");
        exit(-1);
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        ci_debug_printf(1, "Unable to set stdout to '/dev/null'. exiting...");
        exit(-1);
    }

    if (dup2(fd, STDERR_FILENO) < 0) {
        ci_debug_printf(1, "Unable to set stderr to '/dev/null'. exiting...");
        exit(-1);
    }
    close(fd);
}
#endif

int main(int argc, char **argv)
{
#if ! defined(_WIN32)
    __log_error = (void (*)(void *, const char *,...)) log_server;     /*set c-icap library log  function */
#else
    __vlog_error = vlog_server;        /*set c-icap library  log function */
#endif

    mem_init();
    init_internal_lookup_tables();
    ci_acl_init();
    init_http_auth();
    if (init_body_system() != CI_OK) {
        ci_debug_printf(1, "Can not initialize body system\n");
        exit(-1);
    }
    ci_txt_template_init();
    ci_txt_template_set_dir(DATADIR"templates");
    commands_init();

    if (!(CI_CONF.MAGIC_DB = ci_magic_db_load(CI_CONF.magics_file))) {
        ci_debug_printf(1, "Can not load magic file %s!!!\n",
                        CI_CONF.magics_file);
    }
    init_conf_tables();
    request_stats_init();
    init_modules();
    init_services();
    config(argc, argv);
    compute_my_hostname();
    ci_debug_printf(2, "My hostname is: %s\n", MY_HOSTNAME);

    if (!log_open()) {
        ci_debug_printf(1, "Can not init loggers. Exiting.....\n");
        exit(-1);
    }

#if ! defined(_WIN32)
    if (is_icap_running(CI_CONF.PIDFILE)) {
        ci_debug_printf(1, "c-icap server already running!\n");
        exit(-1);
    }
    if (DAEMON_MODE)
        run_as_daemon();
    if (!set_running_permissions(CI_CONF.RUN_USER, CI_CONF.RUN_GROUP))
        exit(-1);
    store_pid(CI_CONF.PIDFILE);
#endif

    if (!init_server())
        return -1;
    post_init_modules();
    post_init_services();
    start_server();
    commands_destroy();
    clear_pid(CI_CONF.PIDFILE);
    return 0;
}
