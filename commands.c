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


#include "c-icap.h"
#include "net_io.h"
#include "debug.h"
#include "log.h"
#include "commands.h"
#include "cfg_param.h"

static ci_command_t *commands_list = NULL;
static int commands_list_size = 0;
static int commands_list_num = 0;

/*Must called only in initialization procedure.
  It is not thread-safe!
*/

#define STEP 64

void register_command(char *name, int type,
                      void (*command_action) (char *name, int type,
                                              char **argv))
{
     if (commands_list == NULL) {
          commands_list_size = STEP;
          commands_list = malloc(commands_list_size * sizeof(ci_command_t));
     }
     else if (commands_list_num == commands_list_size) {
          commands_list_size += STEP;
          commands_list =
              realloc(commands_list, commands_list_size * sizeof(ci_command_t));
     }
     if (commands_list == NULL) {
          ci_debug_printf(1,
                          "Error allocating memory for commands list. Exiting......!\n");
          exit(-1);
     }
     commands_list[commands_list_num].name = strdup(name);
     commands_list[commands_list_num].type = type;
     commands_list[commands_list_num++].command_action = command_action;
}

void reset_commands()
{
     int i;
     for (i = 0; i < commands_list_num; i++) {
          free(commands_list[i].name);
     }
     commands_list_num = 0;
}

/*
Currently we are using the following functions which defined in cfg_param.c file
These functions must moved to a utils.c file ...
*/
char **split_args(char *args);
void free_args(char **argv);


ci_command_t *find_command(char *cmd_line)
{
     int len, i;
     char *s;
     s = strchr(cmd_line, ' ');
     if (s)
          len = s - cmd_line;
     else
          len = strlen(cmd_line);

     if (len) {
          for (i = 0; i < commands_list_num; i++) {
               if (strncmp(cmd_line, commands_list[i].name, len) == 0) {
                    return &(commands_list[i]);
               }
          }
     }
     return NULL;
}

int execute_command(ci_command_t * command, char *cmdline, int exec_type)
{
     char **args;

     if (!command)
          return 0;
     args = split_args(cmdline);
     command->command_action(args[0], exec_type, args + 1);
     free_args(args);
     return 1;
}
