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

#ifndef __COMMANDS_H
#define __COMMANDS_H

#include "c-icap.h"

#define NAMED_PIPES_DIR "/tmp/"
#define COMMANDS_BUFFER_SIZE 128

#define MONITOR_PROC_CMD      1
#define CHILDS_PROC_CMD       2
#define MONITOR_PROC_POST_CMD 4
#define ALL_PROC_CMD          7

typedef struct ci_command{
     char *name;
     int type;
     void (*command_action)(char *name,int type,char **argv);
} ci_command_t;


CI_DECLARE_FUNC(void) register_command(char *name,int type, void (*command_action)(char *name,int type, char **argv));
CI_DECLARE_FUNC(void) reset_commands();
CI_DECLARE_FUNC(int) execute_command(ci_command_t *command,char *cmdline,int exec_type);
CI_DECLARE_FUNC(ci_command_t) *find_command(char *cmd_line);

#endif /*__COMMANDS_H*/
