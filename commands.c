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

#include <assert.h>
#include "common.h"
#include "c-icap.h"
#include "net_io.h"
#include "debug.h"
#include "log.h"
#include "commands.h"
#include "cfg_param.h"
#include "registry.h"

struct schedule_data {
    char name[CMD_NM_SIZE];
    time_t when;
    void *data;
};

/*The list of commands*/
static ci_list_t *COMMANDS_LIST = NULL;;
/* The list of request for ONDEMAND_CMD commands*/
static ci_list_t *COMMANDS_QUEUE = NULL;
ci_thread_mutex_t COMMANDS_MTX;

void commands_init()
{
    ci_thread_mutex_init(&COMMANDS_MTX);
    COMMANDS_LIST = ci_list_create(64, sizeof(ci_command_t));
    COMMANDS_QUEUE = ci_list_create(64, sizeof(struct schedule_data));
}


void register_command(const char *name, int type,
                      void (*command_action) (const char *name, int type,
                              const char **argv))
{
    if (! (type & ALL_PROC_CMD)) {
        ci_debug_printf(1, "Can not register command %s ! Wrong type\n", name );
        return;
    }
    ci_command_t cmd;
    strncpy(cmd.name, name, CMD_NM_SIZE);
    cmd.name[CMD_NM_SIZE - 1] = '\0';
    cmd.type = type;
    cmd.data = NULL;
    cmd.command_action = command_action;
    ci_thread_mutex_lock(&COMMANDS_MTX);
    ci_list_push(COMMANDS_LIST, &cmd);
    ci_thread_mutex_unlock(&COMMANDS_MTX);
    ci_debug_printf(5, "Command %s registered\n", name);
}

void register_command_extend(const char *name, int type, void *data,
                             void (*command_action) (const char *name, int type,
                                     void *data))
{
    if (type != CHILD_START_CMD && type != CHILD_STOP_CMD && type != ONDEMAND_CMD) {
        ci_debug_printf(1, "Can not register extend command %s ! wrong type\n", name );
        return;
    }
    ci_command_t cmd;
    strncpy(cmd.name, name, CMD_NM_SIZE);
    cmd.name[CMD_NM_SIZE - 1] = '\0';
    cmd.type = type;
    cmd.data = data;
    cmd.command_action_extend = command_action;
    ci_thread_mutex_lock(&COMMANDS_MTX);
    ci_list_push(COMMANDS_LIST, &cmd);
    ci_thread_mutex_unlock(&COMMANDS_MTX);
    ci_debug_printf(5, "Extend command %s registered\n", name);
}

void commands_reset()
{
    if (COMMANDS_QUEUE) {
        ci_list_destroy(COMMANDS_QUEUE);
        COMMANDS_QUEUE = ci_list_create(64, sizeof(struct schedule_data));
    }
    if (COMMANDS_LIST) {
        ci_list_destroy(COMMANDS_LIST);
        COMMANDS_LIST = ci_list_create(64, sizeof(ci_command_t));
    }
}

/*
Currently we are using the following functions which defined in cfg_param.c file
These functions must moved to a utils.c file ...
*/
char **split_args(char *args);
void free_args(char **argv);

int cb_check_command(void *data, const void *obj)
{
    const ci_command_t **rcommand = (const ci_command_t **)data;
    const ci_command_t *cur_item = (const ci_command_t *)obj;
    if (*rcommand && strcmp((*rcommand)->name, cur_item->name) == 0) {
        *rcommand = cur_item;
        return 1;
    }

    return 0;
}

ci_command_t *find_command(const char *cmd_line)
{
    int len;
    char *s;
    ci_command_t tmpCmd;
    ci_command_t *cmd;

    if (COMMANDS_LIST == NULL) {
        ci_debug_printf(5, "None command registered\n");
        return NULL;
    }

    s = strchr(cmd_line, ' ');
    if (s)
        len = s - cmd_line;
    else
        len = strlen(cmd_line);

    if (len && len < CMD_NM_SIZE) {
        strncpy(tmpCmd.name, cmd_line, len);
        tmpCmd.name[len] = '\0';
        cmd = &tmpCmd;
        ci_list_iterate(COMMANDS_LIST, &cmd, cb_check_command);
        if (cmd != &tmpCmd)
            /*We found an cmd stored in list. Return it*/
            return cmd;
    }

    return NULL;
}

int execute_command(ci_command_t * command, char *cmdline, int exec_type)
{
    char **args;

    if (!command)
        return 0;
    args = split_args(cmdline);
    command->command_action(args[0], exec_type, (const char **)(args + 1));
    free_args(args);
    return 1;
}

static int exec_cmd_step(void *data, const void *cmd)
{
    int cmd_type = *((int *)data);
    ci_command_t *command = (ci_command_t *)cmd;
    ci_debug_printf(7, "Check command: %s, type: %d \n",
                    command->name, command->type);
    if (command->type == cmd_type) {
        ci_debug_printf(5, "Execute command:%s \n", command->name);
        command->command_action_extend (command->name, command->type, command->data);
    }
    return 0;
}

static int execute_child_commands (int cmd_type)
{
    ci_debug_printf(5, "Going to execute child commands\n");
    if (COMMANDS_LIST == NULL) {
        ci_debug_printf(5, "None command registered\n");
        return 0;
    }
    ci_list_iterate(COMMANDS_LIST, &cmd_type, exec_cmd_step);
    return 1;
}

int commands_execute_start_child()
{
    return execute_child_commands(CHILD_START_CMD);
}

int commands_execute_stop_child()
{
    return execute_child_commands(CHILD_STOP_CMD);
}

void ci_command_register_ctl_cmd(const char *name, int type, void (*command_action)(const char *name,int type, const char **argv))
{
    register_command(name, type, command_action);
}

void ci_command_register_action(const char *name, int type, void *data,
                                void (*command_action) (const char *name, int type, void *data))
{
    register_command_extend(name, type, data, command_action);
}

void ci_command_schedule_on(const char *name, void *data, time_t time)
{
    struct schedule_data sch;
    memset(&sch, 0, sizeof(struct schedule_data));
    strncpy(sch.name, name, CMD_NM_SIZE);
    sch.name[CMD_NM_SIZE - 1] = '\0';
    sch.when = time;
    sch.data = data;
    if (ci_list_search(COMMANDS_QUEUE, &sch)) {
        ci_debug_printf(7, "command %s already scheduled for execution on %ld, ignore\n", name, time);
        return;
    }
    ci_thread_mutex_lock(&COMMANDS_MTX);
    ci_list_push(COMMANDS_QUEUE, &sch);
    ci_thread_mutex_unlock(&COMMANDS_MTX);
    ci_debug_printf(9, "command %s scheduled for execution\n", name);
}

void ci_command_schedule(const char *name, void *data, time_t afterSecs)
{
    time_t tm;
    time(&tm);
    tm += afterSecs;
    ci_command_schedule_on(name, data, tm);
}

static int cb_check_queue(void *data, const void *item)
{
    struct schedule_data *sch = (struct schedule_data *)item;
    time_t tm = *((time_t *)data);
    if (sch->when < tm) {
        ci_command_t *cmd = find_command(sch->name);
        if (cmd) {
            ci_debug_printf(9, "Execute command:%s \n", cmd->name);
            cmd->command_action_extend (cmd->name, cmd->type, (sch->data ? sch->data : cmd->data));
        }
        ci_thread_mutex_lock(&COMMANDS_MTX);
        ci_list_remove(COMMANDS_QUEUE, sch);
        ci_thread_mutex_unlock(&COMMANDS_MTX);
    }
    return 0;
}

void commands_exec_scheduled()
{
    time_t tm;
    ci_debug_printf(10, "Going to execute child commands\n");
    if (COMMANDS_LIST == NULL) {
        ci_debug_printf(10, "None command registered\n");
    }

    if (!COMMANDS_QUEUE)
        return;

    time(&tm);
    ci_list_iterate(COMMANDS_QUEUE, &tm, cb_check_queue);
}
