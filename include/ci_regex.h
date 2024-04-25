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

#ifndef __C_ICAP_CI_REGEX_H
#define __C_ICAP_CI_REGEX_H

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
#define CI_REGEX_SUBMATCHES 10
typedef struct ci_regex_match ci_regex_matches_t[CI_REGEX_SUBMATCHES];
typedef struct ci_regex_replace_part {
    const void *user_data;
    ci_regex_matches_t matches;
} ci_regex_replace_part_t;

/**
 \brief Builds a regex match list to store regex matches.
 * The regex match list is a ci_list_t object which stores ci_regex_replace_part
 * objects as items.
 \ingroup UTILITY
 */
#define ci_regex_create_match_list() ci_list_create(1024, sizeof(ci_regex_replace_part_t))

/**
  \brief Parses a regex expresion having the form /regex/flags
  \ingroup UTILITY
  *
 */
CI_DECLARE_FUNC(char *) ci_regex_parse(const char *str, int *flags, int *recursive);

/**
  \brief Compiles a regex expresion into an internal form.
  \param regex_str The regex string normally returned by ci_regex_parse function
  \param regex_flags The regex flags built using the ci_regex_parse
  \ingroup UTILITY
 */
CI_DECLARE_FUNC(ci_regex_t) ci_regex_build(const char *regex_str, int regex_flags);

/**
 * Releases objects built using the ci_regex_build
 \ingroup UTILITY
 */
CI_DECLARE_FUNC(void) ci_regex_free(ci_regex_t regex);

/**
 * Matchs a compiled regex expresion of type ci_regex_t  against the given string.
 \param regex The compiled regex expresion
 \param str The string to match against
 \param len The str string length. For '\0' terminated strings set it to -1
 \param recurs If it is not NULL matches recursivelly
 \param matches The regex match list to store matches. It can be NULL
 \param user_data pointer to user data to store with matches in rege match list.
 \ingroup UTILITY
 */
CI_DECLARE_FUNC(int) ci_regex_apply(const ci_regex_t regex, const char *str, int len, int recurs, ci_list_t *matches, const void *user_data);


CI_DECLARE_FUNC(void) ci_regex_memory_init();
CI_DECLARE_FUNC(void) ci_regex_memory_destroy();

#ifdef __cplusplus
}
#endif

#endif /*__C_ICAP_CI_REGEX_H*/
