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


#ifndef __C_ICAP_H
#define   __C_ICAP_H


#if defined (_MSC_VER)
#include "c-icap-conf-w32.h"
#else
#include "c-icap-conf.h"
#endif

#ifdef __SYS_TYPES_H_EXISTS
#include <sys/types.h>
#endif

#ifdef __INTTYPES_H_EXISTS
#include <inttypes.h>
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
#   define CI_MAX_PATH     4096 
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
#   define CI_DECLARE_MOD_FUNC(type)   __declspec(dllexport) type
# endif

#else

/*
  
*/
#if defined (USE_VISIBILITY_ATTRIBUTE)
#define CI_DECLARE_FUNC(type) __attribute__ ((visibility ("default"))) type
#define CI_DECLARE_DATA __attribute__ ((visibility ("default")))
#define CI_DECLARE_MOD_DATA __attribute__ ((visibility ("default")))
#else
#define CI_DECLARE_FUNC(type) type
#define CI_DECLARE_DATA
#define CI_DECLARE_MOD_DATA
#endif
#endif

/*
  Here we are define the ci_off_t type to support large files
  We must be careful here, the off_t type is unsigned integer.

  -A comment about lfs:
  In Solaris and Linux to have lfs support, if you are using 
  only lseek and open function, you are using off_t type for offsets
  and compile the program with -D_FILE_OFFSET_BITS=64. This flag
  forces the compiler to typedefs the off_t type as an 64bit integer,
  uses open64, lseek64, mkstemp64 and fopen64 functions. 
  
  Instead for fseek and ftell the functions fseeko and ftello must be used.
  This functions uses off_t argument's instead of long.
  Currently we are not using fseek and ftell in c-icap. 

  The open's manual page says that the flag O_LARGEFILE must be used. Looks that 
  it does not actually needed for linux and solaris (version 10) (but must be checked again......)

  The off_t type in my linux system is a signed integer, but I am not
  sure if it is true for all operating systems
  
*/
typedef off_t ci_off_t;
#if CI_SIZEOF_OFF_T > 4 
#   define PRINTF_OFF_T "lld" 
#   define CAST_OFF_T long long int
#   define ci_strto_off_t strtoull
#else
#   define PRINTF_OFF_T "ld" 
#   define CAST_OFF_T  long int
#   define ci_strto_off_t strtoul
#endif

/*
  Detect n-bytes alignment.
  Old 8 bytes alignment macro:
  #define _CI_ALIGN(val) ((val+7)&~7)
*/
struct _ci_align_test {char n[1]; double d;};
#define _CI_NBYTES_ALIGNMENT ((size_t) &(((struct _ci_align_test *)0)[0].d))
#define _CI_ALIGN(val) ((val+(_CI_NBYTES_ALIGNMENT - 1))&~(_CI_NBYTES_ALIGNMENT - 1))

#define ICAP_OPTIONS   0x01
#define ICAP_REQMOD    0x02
#define ICAP_RESPMOD   0x04

CI_DECLARE_DATA extern const char *ci_methods[];

#define ci_method_support(METHOD, METHOD_DEF) (METHOD&METHOD_DEF)
#define ci_method_string(method) (method<=ICAP_RESPMOD && method>=ICAP_OPTIONS ?ci_methods[method]:"UNKNOWN")


enum ci_error_codes { EC_100, EC_200, EC_204, EC_206, EC_400, 
                   EC_401, EC_403,
		   EC_404, EC_405, EC_407, EC_408,
		   EC_500, EC_501, EC_502, 
		   EC_503, EC_505,
                   EC_MAX
};

typedef struct ci_error_code{
  int code;
  char *str;
} ci_error_code_t;

CI_DECLARE_DATA extern const struct ci_error_code ci_error_codes[];
#define ci_error_code(ec) (ec>=EC_100&&ec<EC_MAX?ci_error_codes[ec].code:1000)
#define ci_error_code_string(ec) (ec>=EC_100&&ec<EC_MAX?ci_error_codes[ec].str:"UNKNOWN ERROR CODE")


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
