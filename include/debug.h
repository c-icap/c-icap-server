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


#ifndef __DEBUG_H
#define __DEBUG_H

#include "c-icap.h"
#include <stdio.h>
#include <stdarg.h>


CI_DECLARE_DATA extern int CI_DEBUG_LEVEL;
CI_DECLARE_DATA extern int CI_DEBUG_STDOUT;


#ifdef _MSC_VER
CI_DECLARE_DATA extern void (*__vlog_error)(void *req, const char *format, va_list ap);
CI_DECLARE_FUNC(void) __ldebug_printf(int i,const char *format, ...);
#define ci_debug_printf __ldebug_printf

#else
 extern void (*__log_error)(void *req, const char *format,... );
#define ci_debug_printf(i, args...) if(i<=CI_DEBUG_LEVEL){ if(__log_error) (*__log_error)(NULL,args); if(CI_DEBUG_STDOUT) printf(args);}
#endif


#endif /*__DEBUG_H*/
