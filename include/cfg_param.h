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


#ifndef __CFG_PARAM_H
#define __CFG_PARAM_H



struct conf_entry{
     char *name;
     void *data;
     int (*action)(char *name, char **argv,void *setdata);
     char *msg;
};

int register_conf_table(char *name,struct conf_entry *table);

int setStr(char *directive,char **argv,void *setdata);
int setInt(char *directive,char **argv,void *setdata);
int setDisable(char *directive,char **argv,void *setdata);
int setEnable(char *directive,char **argv,void *setdata);
int LoadService(char *dorective,char **argv,void *setdata);
int LoadModule(char *directive,char **argv,void *setdata);
int SetLogger(char *directive,char **argv,void *setdata);
int setTmpDir(char *directive,char **argv,void *setdata);


extern int TIMEOUT;
extern int KEEPALIVE_TIMEOUT;
extern int MAX_SECS_TO_LINGER;
extern int START_CHILDS;
extern int MAX_CHILDS;
extern int START_SERVERS;
extern int MIN_FREE_SERVERS;
extern int MAX_FREE_SERVERS;
extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;
extern int PORT;
extern char *SERVER_LOG_FILE;
extern char *ACCESS_LOG_FILE;

#endif
