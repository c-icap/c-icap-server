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
#include "proc_threads_queues.h"

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

#define CI_CMD_NULL              0
#define CI_CMD_MONITOR_PROC      1 /* on master process before children commands are executed*/
#define CI_CMD_CHILDS_PROC       2 /* on children*/
#define CI_CMD_MONITOR_PROC_POST 4 /*on master process after children commands are executed*/
#define CI_CMD_ALL_PROC          7
#define CI_CMD_CHILD_START       8
#define CI_CMD_CHILD_STOP       16
#define CI_CMD_ONDEMAND         32
#define CI_CMD_CHILD_CLEANUP  64 /* On master process after a child exit */
#define CI_CMD_POST_CONFIG    128 /*on master process after configuration file is read*/
#define CI_CMD_MONITOR_START    256
#define CI_CMD_MONITOR_STOP     512
#define CI_CMD_MONITOR_ONDEMAND 1024

#define CMD_NM_SIZE 128
typedef struct ci_command {
    char name[CMD_NM_SIZE];
    int type;
    void *data;
    union {
        void (*command_action)(const char *name, int type,const char **argv);
        void (*command_action_extend)(const char *name, int type, void *data);
        void (*command_child_cleanup)(const char *name, process_pid_t pid, int reason, void *data);
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

enum {CI_PROC_TERMINATED = 0, CI_PROC_CRASHED};
/**
   Register a handler for cleanup stopped children.
   \var name A name for this handler
   \var data Pointer to the user data to be passed as handler parameter
   \var child_cleanup_handler A pointer to the handler.

   The handler will be executed on master process after a child is
   terminated, normally or after a crash.
   The handler will be executed with the handler name as name parameter,
   the terminated process pid as pid parameter, with a non zero integer
   as reason parameter if child process terminated abnormally and the
   user data as data parameter.
   The handler will be executed immediately after the master process
   informed, maybe after a new child-process started to replace the killed one.
*/
CI_DECLARE_FUNC(void) ci_command_register_child_cleanup(const char *name,
                                                        void *data,
                                                        void (*child_cleanup_handler) (const char *name, process_pid_t pid, int reason, void *data));

/*For internal use only*/
void commands_init();
void commands_reset();
void commands_destroy();
int execute_command(ci_command_t *command, char *cmdline, int exec_type);
ci_command_t *find_command(const char *cmd_line);
int commands_execute_start_child();
int commands_execute_stop_child();
int execute_commands_no_lock (int cmd_type);
void commands_exec_scheduled(int cmd_type);
void commands_exec_child_cleanup(process_pid_t pid, int reason);

#ifdef __cplusplus
}
#endif

#endif /*__COMMANDS_H*/
