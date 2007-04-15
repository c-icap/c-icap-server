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
#include "util.h"
#include <time.h>

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
     struct tm br_tm;
     time_t tm;
     time(&tm);
     asctime_r(localtime_r(&tm, &br_tm), buf);
     buf[STR_TIME_SIZE - 1] = '\0';
     buf[strlen(buf) - 1] = '\0';
}

void ci_strtime_rfc822(char *buf)
{
     struct tm br_tm;
     time_t tm;
     time(&tm);
     gmtime_r(&tm, &br_tm);

     snprintf(buf, STR_TIME_SIZE, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
              days[br_tm.tm_wday],
              br_tm.tm_mday,
              months[br_tm.tm_mon],
              br_tm.tm_year + 1900, br_tm.tm_hour, br_tm.tm_min, br_tm.tm_sec);

     buf[STR_TIME_SIZE - 1] = '\0';
}

int ci_mktemp_file(char *dir, char *template, char *filename)
{
     strncpy(filename, dir, CI_FILENAME_LEN - sizeof(template) - 1);
     strcat(filename, template);
     return mkstemp(filename);
}


int ci_usleep(unsigned long usec){
  struct timespec us,ur;
  us.tv_sec = 0;
  us.tv_nsec = usec*1000;
  nanosleep(&us , &ur);
  return 0;
}
