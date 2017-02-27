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

#ifdef __cplusplus
extern "C"
{
#endif

#define COMMANDS_BUFFER_SIZE 128

#define NULL_CMD              0
#define MONITOR_PROC_CMD      1
#define CHILDS_PROC_CMD       2
#define MONITOR_PROC_POST_CMD 4
#define ALL_PROC_CMD          7
#define CHILD_START_CMD       8
#define CHILD_STOP_CMD       16
#define ONDEMAND_CMD         32

#define CMD_NM_SIZE 128
typedef struct ci_command{
     char name[CMD_NM_SIZE];
     int type;
     void *data;
     union {
         void (*command_action)(const char *name, int type,const char **argv);
         void (*command_action_extend)(const char *name, int type, void *data);
     };
} ci_command_t;


/* Backward compatible function for ci_command_register_ctl */
CI_DECLARE_FUNC(void) register_command(const char *name, int type, void (*command_action)(const char *name,int type, const char **argv));

/* backward compatible function for ci_command_register_action */
CI_DECLARE_FUNC(void) register_command_extend(const char *name, int type, void *data,
                                              void (*command_action) (const char *name, int type, void *data));

CI_DECLARE_FUNC(void) ci_command_register_ctl_cmd(const char *name, int type, void (*command_action)(const char *name,int type, const char **argv));
CI_DECLARE_FUNC(void) ci_command_register_action(const char *name, int type, void *data,
                                                 void (*command_action) (const char *name, int type, void *data));
CI_DECLARE_FUNC(void) ci_command_schedule_on(const char *name, void *data, time_t time);
CI_DECLARE_FUNC(void) ci_command_schedule(const char *name, void *data, time_t afterSecs);

void commands_init();
void commands_reset();
int execute_command(ci_command_t *command, char *cmdline, int exec_type);
ci_command_t *find_command(const char *cmd_line);
int commands_execute_start_child();
int commands_execute_stop_child();
void commands_exec_scheduled();

#ifdef __cplusplus
}
#endif

#endif /*__COMMANDS_H*/
