/*
 *  Copyright (C) 2014 Christos Tsantilas
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

#ifndef __CI_REGEX_H
#define __CI_REGEX_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void * ci_regex_t;

struct ci_regex_match {
    size_t s;
    size_t e;
};
typedef struct ci_regex_match ci_regex_matches_t[10];
typedef struct ci_regex_replace_part {
    const void *user_data;
    ci_regex_matches_t matches;
} ci_regex_replace_part_t;

#define ci_regex_create_match_list() ci_list_create(32768, sizeof(ci_regex_replace_part_t))
CI_DECLARE_FUNC(char *) ci_regex_parse(const char *str, int *flags, int *recursive);
CI_DECLARE_FUNC(ci_regex_t) ci_regex_build(const char *regex_str, int regex_flags);
CI_DECLARE_FUNC(void) ci_regex_free(ci_regex_t regex);
CI_DECLARE_FUNC(int) ci_regex_apply(const ci_regex_t regex, const char *str, int len, int recurs, ci_list_t *matches, const void *user_data);

#ifdef __cplusplus
}
#endif

#endif /*__CI_REGEX_H*/
