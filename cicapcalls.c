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

#include "common.h"
#include "c-icap.h"
#include "proc_threads_queues.h"
#include "request.h"
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

void ci_command_register_child_cleanup(const char *name, void *data, void (*child_cleanup_handler) (const char *name, HANDLE pid, int reason, void *data))
{
    typedef void (*RC)(const char *, void *, void(*)(const char *, HANDLE, int, void *));
    RC fn;
    fn = (RC)GetProcAddress(GetModuleHandle(NULL), "ci_command_register_child_cleanup");
    if (fn)
        (*fn)(name, data, child_cleanup_handler);
    else
        fprintf(stderr, "Can not execute ci_command_register_child_cleanup\n");
}

ci_kbs_t ci_server_stat_kbs_get_running(int id)
{
    typedef ci_kbs_t (*CS)(int id);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_kbs_get_running");
    if (fn)
        return (*fn)(id);
    fprintf(stderr, "Can not execute ci_server_stat_kbs_get_running\n");
    return ci_kbs_zero();
}

uint64_t ci_server_stat_uint64_get_running(int id)
{
    typedef uint64_t (*CS)(int id);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_uint64_get_running");
    if (fn)
        return (*fn)(id);
    fprintf(stderr, "Can not execute ci_server_stat_uint64_get_running\n");
    return 0;
}

ci_kbs_t ci_server_stat_kbs_get_global(int id)
{
    typedef ci_kbs_t (*CS)(int id);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_kbs_get_global");
    if (fn)
        return (*fn)(id);
    fprintf(stderr, "Can not execute ci_server_stat_kbs_get_global\n");
    return ci_kbs_zero();
}

uint64_t ci_server_stat_uint64_get_global(int id)
{
    typedef uint64_t (*CS)(int id);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_uint64_get_global");
    if (fn)
        return (*fn)(id);
    fprintf(stderr, "Can not execute ci_server_stat_uint64_get_global\n");
    return 0;
}

ci_stat_memblock_t *ci_server_stat_get_all_stats(uint32_t flags)
{
    typedef ci_stat_memblock_t *(*CS)(uint32_t);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_get_all_stats");
    if (fn)
        return (*fn)(flags);

    fprintf(stderr, "Can not execute ci_server_stat_get_all_stats\n");
    return -1;
}

void ci_server_stat_free_all_stats(ci_stat_memblock_t *blk)
{
    typedef void (*CS)(ci_stat_memblock_t *);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_free_all_stats");
    if (fn)
        return (*fn)(blk);

    fprintf(stderr, "Can not execute ci_server_stat_free_all_stats\n");
}

const ci_stat_memblock_t *ci_server_stat_get_child_stats(process_pid_t pid, uint32_t flags)
{
    typedef void (*CS)(process_pid_t, uint32_t);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_get_child_stats");
    if (fn)
        return (*fn)(pid, flags);

    fprintf(stderr, "Can not execute ci_server_stat_get_child_stats\n");
    return NULL;
}

const ci_stat_memblock_t *ci_server_stat_get_history_stats(uint32_t flags)
{
    typedef void (*CS)(uint32_t);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_stat_get_history_stats");
    if (fn)
        return (*fn)(flags);

    fprintf(stderr, "Can not execute ci_server_stat_get_history_stats\n");
    return NULL;
}

int ci_server_shared_memblob_register(const char *name, size_t size)
{
    typedef int (*CS)(const char *, size_t);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_shared_memblob_register");
    if (fn)
        return (*fn)(name, size);

    fprintf(stderr, "Can not execute ci_server_shared_memblob_register\n");
    return -1;
}

void *ci_server_shared_memblob(int ID)
{
    typedef void * (*CS)(int ID);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_shared_memblob");
    if (fn)
        return (*fn)(ID);

    fprintf(stderr, "Can not execute ci_server_shared_memblob\n");
    return NULL;
}

void * ci_server_shared_memblob_byname(const char *name)
{
    typedef void * (*CS)(const char *);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_server_shared_memblob_byname");
    if (fn)
        return (*fn)(name);

    fprintf(stderr, "Can not execute ci_server_shared_memblob_byname\n");
    return NULL;
}

void ci_http_server_register_service(const char *path, const char *descr, int (*handler)(ci_request_t *req), unsigned flags)
{
    typedef void (*CS)(const char *, const char *, int (*)(ci_request_t *req), unsigned);
    CS fn;
    fn = (CS)GetProcAddress(GetModuleHandle(NULL), "ci_http_server_register_service");
    if (fn)
        return (*fn)(path, descr, handler, flags);

    fprintf(stderr, "Can not execute ci_http_server_register_service\n");
}
