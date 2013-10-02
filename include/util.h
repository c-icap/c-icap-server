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

#ifndef __UTIL_H
#define __UTIL_H

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

#ifdef __cplusplus
}
#endif

#endif
