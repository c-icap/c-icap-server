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
#include "c-icap-conf.h"
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

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_DLFCN_H 
#include <dlfcn.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/*some defines */
#ifdef _WIN32
# define CI_FILENAME_LEN _MAX_PATH
# define CI_MAX_PATH     _MAX_PATH
#else
# if defined(MAXPATHLEN)
#   define CI_MAX_PATH     MAXPATHLEN
# elif defined(PATH_MAX)
#   define CI_MAX_PATH     PATH_MAX
# else
#   define CI_MAX_PATH     256
# endif
# define CI_FILENAME_LEN CI_MAX_PATH
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

/*
  Here we are define the ci_off_t type to support large files
  We must be careful here, the off_t type is unsigned integer.

  -A comment about lfs:
  In Solaris and Linux to have lfs support, if you are using 
  only lseek and open function, you are using off_t type for offsets
  and compile the program with -D_FILE_OFFSET_BITS=64. This flag
  forces the compiler to typedefs the off_t type as an 64bit unsigned  integer,
  uses open64, lseek64, mkstemp64 and fopen64 functions. 
  
  Instead for fseek and ftell the functions fseeko and ftello must be used.
  This functions uses off_t argument's instead of long.
  Currently we are not using fseek and ftell in c-icap. 

  The open's manual page says that the flag O_LARGEFILE must be used. Looks that 
  it does not actually needed for linux and solaris (version 10) (but must be checked again......)
  
*/
typedef off_t ci_off_t;
#if SIZEOF_OFF_T > 4 
#   define PRINTF_OFF_T "llu" 
#   define ci_strto_off_t strtoull
#else
#   define PRINTF_OFF_T "lu" 
#   define ci_strto_off_t strtoul
#endif

#define ICAP_OPTIONS   0x01
#define ICAP_REQMOD    0x02
#define ICAP_RESPMOD   0x04

CI_DECLARE_DATA extern const char *ci_methods[];

#define ci_method_support(METHOD, METHOD_DEF) (METHOD&METHOD_DEF)
#define ci_method_string(method) (method<=ICAP_RESPMOD && method>=ICAP_OPTIONS ?ci_methods[method]:"UNKNOWN")


enum ci_error_codes { EC_100, EC_204, EC_400, 
                   EC_401, EC_403,
		   EC_404, EC_405, EC_408,
		   EC_500, EC_501, EC_502, 
		   EC_503, EC_505};

typedef struct ci_error_code{
  int code;
  char *str;
} ci_error_code_t;

CI_DECLARE_DATA extern const struct ci_error_code ci_error_codes[];
#define ci_error_code(ec) (ec>=EC_100&&ec<=EC_500?ci_error_codes[ec].code:1000)
#define ci_error_code_string(ec) (ec>=EC_100&&ec<=EC_500?ci_error_codes[ec].str:"UNKNOWN ERROR CODE")


#define ICAP_EOL "\r\n"
#define ICAP_EOF "0\r\n\r\n"
#define ICAP_IEOF "0; ieof\r\n\r\n"
#define ISTAG     "CI0001" /*Always length of 6 chars*/
#define ISTAG_SIZE 32

/*The following block defines the base doxygen group (API group)*/
/**
 \defgroup API  API  Documentation
 * Functions, typedefs and structures for use with modules and services
 *
 */

#endif
