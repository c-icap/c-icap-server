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


#ifndef __C_ICAP_H
#define   __C_ICAP_H


#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined (_MSC_VER)
#include "config-w32.h"
#endif


#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_DLFCN_H 
#include <dlfcn.h>
#endif

#ifdef _WIN32
# if defined(CI_BUILD_LIB) 

#   define CI_DECLARE_FUNC(type) __declspec(dllexport) type
#   define CI_DECLARE_DATA       __declspec(dllexport)

# else

#   define CI_DECLARE_FUNC(type) __declspec(dllimport) type
#   define CI_DECLARE_DATA       __declspec(dllimport)

# endif

# if defined (CI_BUILD_MODULE)
#   define CI_DECLARE_MOD_DATA   __declspec(dllexport)
# endif

#else

#define CI_DECLARE_FUNC(type) type
#define CI_DECLARE_DATA
#define CI_DECLARE_MOD_DATA
#endif


//enum icap_methods {ICAP_REQMOD, ICAP_RESPMOD, ICAP_OPTIONS };
//
#define ICAP_OPTIONS   0x01
#define ICAP_REQMOD    0x02
#define ICAP_RESPMOD   0x04

CI_DECLARE_DATA extern const char *CI_Methods[];

#define ci_method_support(METHOD, METHOD_DEF) (METHOD&METHOD_DEF)
#ifdef __CYGWIN__
const char *ci_method_string(int method);
#else
#define ci_method_string(method) (method<=ICAP_RESPMOD && method>=ICAP_OPTIONS ?CI_Methods[method]:"UNKNOWN")
#endif



enum ci_error_codes { EC_100, EC_204, EC_400, 
		   EC_404, EC_405, EC_408,
		   EC_500, EC_501, EC_502, 
		   EC_503, EC_505};

typedef struct ci_error_code{
  int code;
  char *str;
} ci_error_code_t;

CI_DECLARE_DATA extern const struct ci_error_code CI_ErrorCodes[];
#ifdef __CYGWIN__
int ci_error_code(int ec);
const char *ci_error_code_string(int ec);
#else
#define ci_error_code(ec) (ec>=EC_100&&ec<=EC_500?CI_ErrorCodes[ec].code:1000)
#define ci_error_code_string(ec) (ec>=EC_100&&ec<=EC_500?CI_ErrorCodes[ec].str:"UNKNOWN ERROR CODE")
#endif

#define ICAP_EOL "\r\n"
#define ICAP_EOF "0\r\n\r\n"
#define ICAP_IEOF "0; ieof\r\n\r\n"


#endif
