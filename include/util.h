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

#ifndef __C_ICAP_UTIL_H
#define __C_ICAP_UTIL_H

#include "array.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define STR_TIME_SIZE 64

CI_DECLARE_FUNC(void) ci_strtime(char *buf);
CI_DECLARE_FUNC(void) ci_strtime_rfc822(char *buf);
CI_DECLARE_FUNC(int)  ci_mktemp_file(char*dir,char *name_template,char *filename);
CI_DECLARE_FUNC(int)  ci_usleep(unsigned long usec);


#ifdef _WIN32
CI_DECLARE_FUNC(int) mkstemp(char *filename);
CI_DECLARE_FUNC(struct tm*) localtime_r(const time_t *t, struct tm *tm);
CI_DECLARE_FUNC(struct tm*) gmtime_r(const time_t *t, struct tm *tm);
#endif

CI_DECLARE_FUNC(const char *) ci_strnstr(const char *s, const char *find, size_t slen);

CI_DECLARE_FUNC(const char *) ci_strncasestr(const char *s, const char *find, size_t slen);

CI_DECLARE_FUNC(const char *) ci_strcasestr(const char *str, const char *find);

/*Handle M/m/k/K suffixes and try to detect errors*/
CI_DECLARE_FUNC(long int) ci_atol_ext(const char *str, const char **error);

CI_DECLARE_FUNC(void) ci_str_trim(char *str);

CI_DECLARE_FUNC(char *) ci_str_trim2(char *s);

CI_DECLARE_FUNC(char *) ci_strerror(int error, char *buf, size_t buflen);

CI_DECLARE_FUNC(ci_dyn_array_t *) ci_parse_key_value_list(const char *str, char sep);
#ifdef __cplusplus
}
#endif

#endif
