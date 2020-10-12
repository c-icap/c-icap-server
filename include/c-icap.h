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
  Here we are define the ci_off_t type to support large files.

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
  it is not actually needed for linux and solaris (version 10) (but must be checked again......)

  The off_t type is a signed integer. It must be on all POSIX/POSIX-like
  systems as fseek has to be able to seek forward and backward from SEEK_CUR
  and SEEK_END.

*/
typedef off_t ci_off_t;
#if CI_SIZEOF_OFF_T > CI_SIZEOF_LONG
#   define PRINTF_OFF_T "lld"
#   define CAST_OFF_T long long int
#   define ci_strto_off_t strtoll
#   define CI_STRTO_OFF_T_MAX LLONG_MAX
#   define CI_STRTO_OFF_T_MIN LLONG_MIN
#else
#   define PRINTF_OFF_T "ld"
#   define CAST_OFF_T  long int
#   define ci_strto_off_t strtol
#   define CI_STRTO_OFF_T_MAX LONG_MAX
#   define CI_STRTO_OFF_T_MIN LONG_MIN
#endif

/*
  Detect n-bytes alignment.
  Old 8 bytes alignment macro:
  #define _CI_ALIGN(val) ((val+7)&~7)
*/
struct _ci_align_test {char n[1]; double d;};
#define _CI_NBYTES_ALIGNMENT ((size_t) &(((struct _ci_align_test *)0)[0].d))
#define _CI_ALIGN(val) ((val+(_CI_NBYTES_ALIGNMENT - 1))&~(_CI_NBYTES_ALIGNMENT - 1))

/*The following block defines the base doxygen group (API group)*/
/**
 \defgroup API  API  Documentation
 * Functions, typedefs and structures for use with modules and services
 *
 */

#endif
