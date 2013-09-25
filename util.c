/*
 *  Copyright (C) 2004-2011 Christos Tsantilas
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

#include "common.h"
#include "c-icap.h"
#include "util.h"
#include <ctype.h>

#ifndef HAVE_STRNSTR
const char *strnstr(const char *s, const char *find, size_t slen)
{
    size_t len = strlen(find);

    if (len == 0)
        return NULL;

    while(len <= slen) {
        if (*s == *find && strncmp(s, find, len) == 0 )
            return s;
        s++,slen--;
    }
    return NULL;
}
#endif

#ifndef HAVE_STRCASESTR
const char *strcasestr(const char *str, const char *find)
{
    const char *s, *c, *f;
    for (s = str; *s != '\0'; ++s) {
        for (f = find, c = s; ; ++f, ++c) {
            if (*f == '\0') /*find matched s*/
                return s;
            if (*c == '\0') /*find is longer than the remaining string */
                return NULL;
            if (tolower(*c) != tolower(*f))
                break;
        }
    }
    return NULL;
}
#endif

#ifndef HAVE_STRNCASESTR
const char *strncasestr(const char *s, const char *find, size_t slen)
{
    size_t len = strlen(find);

    if (len == 0)
        return NULL;

    while(len <= slen) {
        if (tolower(*s) == tolower(*find) && strncasecmp(s, find, len) == 0 )
            return s;
        s++,slen--;
    }
    return NULL;
}
#endif
