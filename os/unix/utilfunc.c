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
#include "util.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#if defined(HAVE_TIOCGWINSZ)
#include <sys/ioctl.h>
#endif

static const char *days[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

static const char *months[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

void ci_strtime(char *buf)
{
    ci_strntime(buf, STR_TIME_SIZE);
}

void ci_strtime_rfc822(char *buf)
{
    ci_strntime_rfc822(buf, STR_TIME_SIZE);
}

void ci_strntime(char *buf, size_t size)
{
    assert(size > 0);
    struct tm br_tm;
    time_t tm;
    time(&tm);
    if (!strftime(buf, size, "%a %b %e %T %Y", localtime_r(&tm, &br_tm)))
        buf[0] = '\0';
}

void ci_to_strntime(char *buf, size_t size, const time_t *tm)
{
    assert(size > 0);
    struct tm br_tm;
    if (!strftime(buf, size, "%a %b %e %T %Y", localtime_r(tm, &br_tm)))
        buf[0] = '\0';
}

void ci_to_strntime_rfc822(char *buf, size_t size, const time_t *tm)
{
    assert(size > 0);
    struct tm br_tm;
    gmtime_r(tm, &br_tm);
    snprintf(buf, size, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
             days[br_tm.tm_wday],
             br_tm.tm_mday,
             months[br_tm.tm_mon],
             br_tm.tm_year + 1900, br_tm.tm_hour, br_tm.tm_min, br_tm.tm_sec);
}

void ci_strntime_rfc822(char *buf, size_t size)
{
    time_t tm;
    time(&tm);
    ci_to_strntime_rfc822(buf, size, &tm);
}

int ci_mktemp_file(char *dir, char *template, char *filename)
{
    snprintf(filename, CI_FILENAME_LEN, "%s%s",dir,template);
    return mkstemp(filename);
}


int ci_usleep(unsigned long usec)
{
    struct timespec us, ur;
    us.tv_sec = 0;
    us.tv_nsec = usec * 1000;
    nanosleep(&us, &ur);
    return 0;
}

int ci_screen_columns()
{
    int cols = 80;
#if defined(HAVE_TIOCGWINSZ)
    struct winsize ws;
    int fd;
    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0 && ioctl(fd, TIOCGWINSZ, &ws) == 0)
        cols = ws.ws_col;
    close(fd); // dont forget to close files
#endif
    return cols;
}
