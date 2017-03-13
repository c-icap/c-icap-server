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
#include "debug.h"
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

int store_pid(char *pidfile)
{
    int fd, bytes;
    pid_t pid;
    char strPid[30];           /*30 must be enough for storing pids on a string */
    pid = getpid();

    if ((fd = open(pidfile, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
        ci_debug_printf(1, "Cannot open the pid file: %s\n", pidfile);
        return 0;
    }
    snprintf(strPid, 29, "%d", pid);
    strPid[29] = '\0';
    bytes = write(fd, strPid, strlen(strPid));
    if (bytes != strlen(strPid)) {
        ci_debug_printf(1, "Cannot write to the pid file: %s\n", pidfile);
    }
    close(fd);
    return 1;
}

int clear_pid(char *pidfile)
{
    if ( 0 != remove(pidfile)) {
        ci_debug_printf(1, "Cannot delete the pid file: %s Error:%d\n", pidfile, errno);
        return 0;
    }
    return 1;
}

int is_icap_running(char *pidfile)
{
    int fd, bytes, ret;
    pid_t pid;
    char strPid[30];           /*30 must be enough for storing pids on a string */
    pid = getpid();

    if ((fd = open(pidfile, O_RDONLY, 0644)) < 0) {
        return 0;
    }
    bytes = read(fd, strPid, sizeof(strPid));
    close(fd);

    if (bytes < 0)
        return 0;

    if (bytes < sizeof(strPid) - 1)
        strPid[bytes] = '\0';
    else
        strPid[sizeof(strPid) - 1] = '\0';    /*Maybe check for errors? */
    pid = strtol(strPid, NULL, 10);
    if (pid <= 0)               /*garbage */
        return 0;
    ret = kill(pid, 0);
    if (ret < 0)
        return 0;

    return 1;
}



int set_running_permissions(char *user, char *group)
{
    unsigned int uid, gid;
    char *pend;
    struct passwd *pwd;
    struct group *grp;

    if (group) {               /*Configuration request to change ours group id */
        errno = 0;
        gid = strtol(group, &pend, 10);
        if (pend != '\0' || gid < 0 || errno != 0) {  /*string "group" does not contains a clear number */
            if ((grp = getgrnam(group)) == NULL) {
                ci_debug_printf(1,
                                "There is no group %s in password file!\n",
                                group);
                return 0;
            }
            gid = grp->gr_gid;
        } else if (getgrgid(gid) == NULL) {
            ci_debug_printf(1,
                            "There is no group with id=%d in password file!\n",
                            gid);
            return 0;
        }

        if (setgid(gid) != 0) {
            ci_debug_printf(1, "setgid to %d failed!!!!\n", gid);
            perror("Wtat is this; ");
            return 0;
        }
    }


    if (user) {                /*Gonfiguration request to change ours user id */
        errno = 0;
        uid = strtol(user, &pend, 10);
        if (pend != '\0' || uid < 0 || errno != 0) {  /*string "user" does not contain a clear number */
            if ((pwd = getpwnam(user)) == NULL) {
                ci_debug_printf(1,
                                "There is no user %s in password file!\n",
                                user);
                return 0;
            }
            uid = pwd->pw_uid;
        } else if (getpwuid(uid) == NULL) {
            ci_debug_printf(1,
                            "There is no user with id=%d in password file!\n",
                            uid);
            return 0;
        }

        if (setuid(uid) != 0) {
            ci_debug_printf(1, "setuid to %d failed!!!!\n", uid);
            return 0;
        }
    }


    return 1;
}
