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

CI_DECLARE_FUNC(int) setStr(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setInt(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setDisable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setEnable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) config(int argc, char **argv);


CI_DECLARE_DATA extern int TIMEOUT;
CI_DECLARE_DATA extern int KEEPALIVE_TIMEOUT;
CI_DECLARE_DATA extern int MAX_SECS_TO_LINGER;
CI_DECLARE_DATA extern int START_CHILDS;
CI_DECLARE_DATA extern int MAX_CHILDS;
CI_DECLARE_DATA extern int START_SERVERS;
CI_DECLARE_DATA extern int MIN_FREE_SERVERS;
CI_DECLARE_DATA extern int MAX_FREE_SERVERS;
CI_DECLARE_DATA extern int MAX_REQUESTS_BEFORE_REALLOCATE_MEM;
CI_DECLARE_DATA extern int PORT;
CI_DECLARE_DATA extern char *SERVER_LOG_FILE;
CI_DECLARE_DATA extern char *ACCESS_LOG_FILE;

#endif
