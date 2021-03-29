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

//#include "common.h"
//#include "c-icap.h"
#include <stdio.h>
#include <w32api/windows.h>

char *logformat_fmt(const char *name)
{
  typedef char* (*LF_FMT)(const char *);
  LF_FMT fn;
  fn = (LF_FMT)GetProcAddress(GetModuleHandle(NULL), "logformat_fmt");
  if (fn)
    return (*fn)(name);

  fprintf(stderr, "Can not execute logformat_fmt\n");
  return NULL;
}

void ci_command_register_action(const char *name, int type, void *data,
				void (*command_action) (const char *name, int type, void *data))
{
  typedef void (*RA)(const char *, int, void *, void(*)(const char *, int, void *));
  RA fn;
  fn = (RA)GetProcAddress(GetModuleHandle(NULL), "ci_command_register_action");
  if (fn)
    (*fn)(name, type, data, command_action);
  else
    fprintf(stderr, "Can not execute ci_command_register_action\n");
}

void ci_command_register_ctl_cmd(const char *name, int type, void (*command_action)(const char *name,int type, const char **argv))
{
   typedef void (*RC)(const char *, int, void(*)(const char *, int, const char **));
   RC fn;
   fn = (RC)GetProcAddress(GetModuleHandle(NULL), "ci_command_register_ctl_cmd");
   if (fn)
     (*fn)(name, type, command_action);
   else
     fprintf(stderr, "Can not execute ci_command_register_ctl_cmd\n");
}

void ci_command_schedule_on(const char *name, void *data, time_t time)
{
   typedef void (*CS)(const char *, void *, time_t);
   CS fn;
   fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_command_schedule_on");
   if (fn)
     (*fn)(name, data, time);
   else
     fprintf(stderr, "Can not execute ci_command_schedule_on\n");
}

void ci_command_schedule(const char *name, void *data, time_t time)
{
   typedef void (*CS)(const char *, void *, time_t);
   CS fn;
   fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_command_schedule");
   if (fn)
     (*fn)(name, data, time);
   else
     fprintf(stderr, "Can not execute ci_command_schedule\n");
}

void ci_command_register_child_cleanup(const char *name, void *data, void (*child_cleanup_handler) (const char *name, process_pid_t pid, int reason, void *data))
{
    typedef void (*RC)(const char *, void *, void(*)(const char *, process_pid_t, int, void *));
    RC fn;
    fn = (RC)GetProcAddress(GetModuleHandle(NULL), "ci_command_register_child_cleanup");
    if (fn)
        (*fn)(name, data, child_cleanup_handler);
    else
        fprintf(stderr, "Can not execute ci_command_register_child_cleanup\n");
}
