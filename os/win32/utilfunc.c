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
#include <Windows.h>
#include <Winbase.h>
#include <time.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include "util.h"

int strncasecmp(const char *s1, const char *s2, size_t n)
{
	int r = 0;
	while ( n
		&& ((s1 == s2) ||
		    !(r = ((int)( tolower(*((unsigned char *)s1))))
		      - tolower(*((unsigned char *)s2))))
		&& (--n, ++s2, *s1++));
	return r;
}


static const char *days[]={
     "Sun", 
     "Mon", 
     "Tue", 
     "Wed",
     "Thu", 
     "Fri", 
     "Sat"
};

static const char *months[]={
     "",
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

void ci_strtime(char *buf){
     SYSTEMTIME tm;
     GetLocalTime(&tm);
     
     buf[0]='\0';
     snprintf(buf,STR_TIME_SIZE,"%s %s %d %d:%d:%d %d",
	      days[tm.wDayOfWeek],
	      months[tm.wMonth],
	      tm.wDay,
	      tm.wHour,tm.wMinute,
	      tm.wSecond,tm.wYear);
     
     buf[STR_TIME_SIZE-1]='\0';
}


int ci_mktemp_file(char*dir,char *filename){
     int fd;
     GetTempFileName(dir,"CI_TMP",1,filename);
     fd=open(filename,O_RDWR|O_CREAT,S_IREAD|S_IWRITE);
     return fd;
}


#ifndef offsetof
#define offsetof(type,field) ((int) ( (char *)&((type *)0)->field))
#endif
/*
static const unsigned char at_data[] = {
	'S', 'u', 'n', 'M', 'o', 'n', 'T', 'u', 'e', 'W', 'e', 'd',
	'T', 'h', 'u', 'F', 'r', 'i', 'S', 'a', 't',

	'J', 'a', 'n', 'F', 'e', 'b', 'M', 'a', 'r', 'A', 'p', 'r',
	'M', 'a', 'y', 'J', 'u', 'n', 'J', 'u', 'l', 'A', 'u', 'g',
	'S', 'e', 'p', 'O', 'c', 't', 'N', 'o', 'v', 'D', 'e', 'c', 
	'?', '?', '?', 
	' ', '?', '?', '?',
	' ', '0',
	offsetof(struct tm, tm_mday),
	' ', '0',
	offsetof(struct tm, tm_hour),
	':', '0',
	offsetof(struct tm, tm_min),
	':', '0',
	offsetof(struct tm, tm_sec),
	' ', '?', '?', '?', '?', '\n', 0
};
*/
/*
char *asctime_r(const struct tm *ptm, char *buffer)
{
	int tmp;
	assert(ptm);
	assert(buffer);
	memcpy(buffer, at_data + 3*(7 + 12), sizeof(at_data) - 3*(7 + 12));
	if (((unsigned int)(ptm->tm_wday)) <= 6) {
	     memcpy(buffer, at_data + 3 * ptm->tm_wday, 3);
	}
	
	if (((unsigned int)(ptm->tm_mon)) <= 11) {
	     memcpy(buffer + 4, at_data + 3*7 + 3 * ptm->tm_mon, 3);
	}
	
	buffer += 19;
	tmp = ptm->tm_year + 1900;
	if (((unsigned int) tmp) < 10000) {
	     buffer += 4;
	     do {
		  *buffer = '0' + (tmp % 10);
		  tmp /= 10;
	     } while (*--buffer == '?');
	}

	do {
	     --buffer;
	     tmp = *((int *)(((const char *) ptm) + (int) *buffer));
	     
	     if (((unsigned int) tmp) >= 100) { 
		  buffer[-1] = *buffer = '?';
	     } else
	     {
		  *buffer = '0' + (tmp % 10);
		  
		  buffer[-1] += (tmp/10);
	     }
	} while ((buffer -= 2)[-2] == '0');
	
	if (*++buffer == '0') {		
	     *buffer = ' ';
	}
	
	return buffer - 8;
}
*/
