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



void ci_strtime(char *buf){
     struct tm br_tm;
     time_t tm;
     time(&tm);
     asctime_r(localtime_r(&tm,&br_tm),buf);
     buf[STR_TIME_SIZE-1]='\0';
     buf[strlen(buf)-2]='\0';
}



int ci_mktemp_file(char*dir,char*template,char *filename){
     int fd;
     strncpy(filename,dir,CI_FILENAME_LEN-sizeof(template)-1);
     strcat(filename,template);
     return  mkstemp(filename);
}

