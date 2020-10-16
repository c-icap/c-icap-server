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


#ifndef __C_ICAP_DEBUG_H
#define __C_ICAP_DEBUG_H

#include "c-icap.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

CI_DECLARE_DATA extern int CI_DEBUG_LEVEL;
CI_DECLARE_DATA extern int CI_DEBUG_STDOUT;


#ifdef _MSC_VER
CI_DECLARE_DATA extern void (*__vlog_error)(void *req, const char *format, va_list ap);
CI_DECLARE_FUNC(void) __ldebug_printf(int i,const char *format, ...);
#define ci_debug_printf __ldebug_printf

#else
CI_DECLARE_DATA extern void (*__log_error)(void *req, const char *format,... );
#define ci_debug_printf(i, args...) if(i<=CI_DEBUG_LEVEL){ if(__log_error) (*__log_error)(NULL,args); if(CI_DEBUG_STDOUT) printf(args);}
#endif

CI_DECLARE_DATA extern void (*__ci_debug_abort)(const char *file, int line, const char *function, const char *mesg);
#define _CI_ASSERT(expression) {if (!(expression)) (*__ci_debug_abort)(__FILE__, __LINE__, __func__, #expression);}


#ifdef __cplusplus
}
#endif

#endif /*__C_ICAP_DEBUG_H*/
