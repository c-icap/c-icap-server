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
#include "array.h"
#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

const char *ci_strnstr(const char *s, const char *find, size_t slen)
{
    size_t len = strlen(find);

    if (len == 0)
        return NULL;

    while (len <= slen) {
        if (*s == *find && strncmp(s, find, len) == 0 )
            return s;
        s++,slen--;
    }
    return NULL;
}

const char *ci_strcasestr(const char *str, const char *find)
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

const char *ci_strncasestr(const char *s, const char *find, size_t slen)
{
    size_t len = strlen(find);

    if (len == 0)
        return NULL;

    while (len <= slen) {
        if (tolower(*s) == tolower(*find) && strncasecmp(s, find, len) == 0 )
            return s;
        s++,slen--;
    }
    return NULL;
}

static const char *atol_err_erange = "ERANGE";
static const char *atol_err_conversion = "CONVERSION_ERROR";
static const char *atol_err_nonumber = "NO_DIGITS_ERROR";

long int ci_atol_ext(const char *str, const char **error)
{
    char *e;

    long int val;
    errno = 0;
    val = strtol(str, &e, 10);

    if (error) {
        *error = NULL;

        if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
            *error = atol_err_erange;
        else if (errno != 0 && val == 0)
            *error = atol_err_conversion;
        else if (e == str)
            *error = atol_err_nonumber;

        if (*error)
            return 0;
    }

    if (val) {
        if (*e == 'k' || * e == 'K')
            val = val * 1024;
        else if (*e == 'm' || * e == 'M')
            val = val * 1024 * 1024;
    }
    return val;
}

void ci_str_trim(char *str)
{
    char *s, *e;

    if (!str)
        return;

    s = str;
    e = NULL;
    while (isspace((int)*s)) {
        e = s;
        while (*e != '\0') {
            *e = *(e+1);
            e++;
        }
    }
    if (*s == '\0')
        return;
    e = str+strlen(str);
    e--;
    while (isspace((int)*e) && e >= str) {*e = '\0'; --e;};
}

char *ci_str_trim2(char *s)
{
    char *e;

    if (!s)
        return NULL;

    while (isspace((int)*s)) ++s;
    if (*s == '\0')
        return s;
    e = s + strlen(s);
    e--;
    while (isspace((int)*e) && e >= s) {*e = '\0'; --e;};
    return s;
}

char * ci_strerror(int error, char *buf, size_t buflen)
{
#if defined(STRERROR_R_CHAR_P)
    return strerror_r(error, buf, buflen);
#elif defined(HAVE_STRERROR_R)
    if (strerror_r(error,  buf, buflen) == 0)
        return buf;
#else
    snprintf(buf, buflen, "%d", error);
    return buf;

#endif
   return NULL; /* not reached */
}

/*
  TODO: support escaped chars,
*/
ci_dyn_array_t *ci_parse_key_value_list(const char *str, char sep)
{
    char *s, *e, *k, *v;
    ci_dyn_array_t *args_array;
    s = strdup(str);
    if (!s)
        return NULL;

    args_array = ci_dyn_array_new(1024);
    k = s;
    while (k) {
        if ((e = strchr(k, sep))) {
            *e = '\0';
            e++;
        }
        if ((v = strchr(k, '='))) {
            *v = '\0';
            ++v;
        }
        k = ci_str_trim2(k);
        if (v)
            v = ci_str_trim2(v);
        if (*k) {
            ci_dyn_array_add(args_array, k, v ? v : "", v ? strlen(v) + 1 : 1);
        }
        k = (e && *e) ? e : NULL;
    }
    free(s);
    return args_array;
}

int ci_parse_key_mvalues(const char *line, char key_sep, char vals_sep, const ci_type_ops_t *key_type, const ci_type_ops_t *val_type, void **key, size_t *keysize,  ci_vector_t **values)
{
    ci_mem_allocator_t *allocator = ci_os_allocator;
    int row_cols = 0;
    const char *s, *e, *v;

    *key = NULL;
    *values = NULL;
    *keysize = 0;

    s = line;
    while (isspace(*s)) s++;
    if (*s == '#') /*it is a comment*/
        return 1;

    if (*s == '\0') /*it is a blank line*/
        return 1;

    if (!(e = strchr(s, key_sep))) {
        row_cols = 1;
        e = s + strlen(s);
    } else {
        row_cols = 2;
        const char *cs = e;
        while ((cs = strchr(cs, vals_sep))) row_cols++, cs++;
        if (row_cols > 256)
            return -1; /*Too many columns*/
    }
    v = e; /*save the pos of key separator*/
    e--; /*points at the end of key*/
    while(isspace(*e)) e--;
    e++;
    char tmp[32768];
    if ((e - s) > sizeof(tmp) - 1)
        return -2;
    memcpy(tmp, s, e - s);
    tmp[e - s] = '\0';
    (*key) = key_type->dup(tmp, allocator);
    (*keysize) = key_type->size(*key);
    if (row_cols <= 1)
        return 1; /*done, only key without values*/

    /*Allocate a enough mem for storing the values*/
    (*values) = ci_vector_create(65535);
    s = v + 1; /*point after the key separator*/
    int i;
    for (i = 0; *s != '\0' && i< row_cols-1; i++) { /*we have vals*/
        while (isspace(*s)) s++; /*find the start of the string*/
        v = s;
        e = s;
        while (*e != vals_sep && *e != '\0') e++;
        s = (*e == '\0' ? e : e + 1); /*Next start or the end*/
        e--;
        while (isspace(*e)) e--;
        if ((e - v) > (sizeof(tmp) - 1)) {
            ci_vector_destroy(*values);
            *values = NULL;
            allocator->free(allocator, *key);
            *key = NULL;
            return -2;
        }
        e++;
        memcpy(tmp, v, e - v);
        tmp[e - v] = '\0';
        void *avalue = val_type->dup(tmp, allocator);
        size_t avalue_size = val_type->size(avalue);
        if (!ci_vector_add((*values), avalue, avalue_size)) {
            ci_vector_destroy(*values);
            *values = NULL;
            allocator->free(allocator, *key);
            *key = NULL;
            allocator->free(allocator, avalue);
            return -3;
        }
        allocator->free(allocator, avalue);
    }
    return 1;
}
