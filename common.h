
#ifndef __COMMON_H
#define __COMMON_H

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef HAVE_STRNSTR
#define strnstr(s, find, slen) ci_strnstr(s, find, slen)
#endif

#ifndef HAVE_STRNCASESTR
#define strncasestr(s, find, slen) ci_strncasestr(s, find, slen)
#endif

#ifndef HAVE_STRCASESTR
#define strcasestr(str, find) ci_strcasestr(str, find)
#endif

#endif
