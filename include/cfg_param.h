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
#include "filetype.h"
#include "body.h"


struct icap_server_conf{
     int  PORT;
     int  PROTOCOL_FAMILY;
     char *TMPDIR;
     char *PIDFILE;
     char *RUN_USER;
     char *RUN_GROUP;
     char *cfg_file;
     char *magics_file;
     struct ci_magics_db *MAGIC_DB;   
     char *SERVICES_DIR;
     char *MODULES_DIR;
};



struct conf_entry{
     char *name;
     void *data;
     int (*action)(char *name, char **argv,void *setdata);
     char *msg;
};

#ifndef CI_BUILD_LIB
extern struct icap_server_conf CONF;
#endif

int register_conf_table(char *name,struct conf_entry *table);

CI_DECLARE_FUNC(int) setStr(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setInt(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setDisable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) setEnable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) config(int argc, char **argv);


#endif
