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

/* Command line options implementation structure */
struct options_entry{
     char *name;
     char *parameter;
     void *data;
     int (*action)(char *name, char **argv,void *setdata);
     char *msg;
};

#define MAIN_TABLE 1
#define ALIAS_TABLE 2

int register_conf_table(char *name,struct conf_entry *table,int type);

CI_DECLARE_FUNC(int) ci_cfg_set_str(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_set_int(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_onoff(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_disable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_enable(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_size_off(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) ci_cfg_size_long(char *directive,char **argv,void *setdata);
CI_DECLARE_FUNC(int) config(int argc, char **argv);


CI_DECLARE_FUNC(void) ci_args_usage(char *progname,struct options_entry *options);
CI_DECLARE_FUNC(int)  ci_args_apply(int argc, char **argv,struct options_entry *options);


#endif
