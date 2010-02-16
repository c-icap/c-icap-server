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

#ifndef __COMMANDS_H
#define __COMMANDS_H

#include "c-icap.h"

#define COMMANDS_BUFFER_SIZE 128

#define NULL_CMD              0
#define MONITOR_PROC_CMD      1
#define CHILDS_PROC_CMD       2
#define MONITOR_PROC_POST_CMD 4
#define ALL_PROC_CMD          7
#define CHILD_START_CMD       8

typedef struct ci_command{
     char *name;
     int type;
     void *data;
     union {
         void (*command_action)(char *name,int type,char **argv);
         void (*command_action_extend)(char *name, int type, void *data);
     };
} ci_command_t;


CI_DECLARE_FUNC(void) register_command(char *name,int type, void (*command_action)(char *name,int type, char **argv));
CI_DECLARE_FUNC(void) register_command_extend(char *name, int type, void *data,
                                              void (*command_action) (char *name, int type, void *data));
CI_DECLARE_FUNC(void) reset_commands();
CI_DECLARE_FUNC(int) execute_command(ci_command_t *command,char *cmdline,int exec_type);
CI_DECLARE_FUNC(ci_command_t) *find_command(char *cmd_line);
CI_DECLARE_FUNC(int) execure_start_child_commands();

#endif /*__COMMANDS_H*/
