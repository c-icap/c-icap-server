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

#ifndef __UTIL_H
#define __UTIL_H

#define STR_TIME_SIZE 64

CI_DECLARE_FUNC(void) ci_strtime(char *buf);
CI_DECLARE_FUNC(void) ci_strtime_rfc822(char *buf);
CI_DECLARE_FUNC(int)  ci_mktemp_file(char*dir,char *template,char *filename);


#ifdef _WIN32
CI_DECLARE_FUNC(int) strcasecmp(const char *s1, const char *s2);
CI_DECLARE_FUNC(int) strncasecmp(const char *s1, const char *s2, size_t n);
CI_DECLARE_FUNC(int) mkstemp(char *filename);
#endif

#endif
