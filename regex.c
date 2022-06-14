#include "common.h"
#include "debug.h"
#include "array.h"
#include "ci_regex.h"

#ifdef HAVE_PCRE
#include <pcre.h>
#else
#include <regex.h>
#endif


char *ci_regex_parse(const char *str, int *flags, int *recursive)
{
    int slen;
    const char *e;
    char *s;
    if (*str != '/')
        return NULL;
    ++str;
    slen = strlen(str);
    e = str + slen;
    while (*e != '/' && e != str) --e;
    if (*e != '/')
        return NULL;
    slen = e - str;
    s = malloc( (slen + 1) * sizeof(char));
    strncpy(s, str, slen);
    s[slen] = '\0';

    *flags = 0;
#ifdef HAVE_PCRE
    *flags |= PCRE_NEWLINE_ANY;
    *flags |= PCRE_NEWLINE_ANYCRLF;
#else
    *flags |= REG_EXTENDED;
//    *flags |= REG_NOSUB;
#endif

    while (*e != '\0') {
#ifdef HAVE_PCRE
        if (*e == 'i')
            *flags = *flags | PCRE_CASELESS;
        else if (*e == 'm')
            *flags |= PCRE_MULTILINE;
        else if (*e == 's')
            *flags |= PCRE_DOTALL;
        else if (*e == 'x')
            *flags |= PCRE_EXTENDED;
        else if (*e == 'A')
            *flags |= PCRE_ANCHORED;
        else if (*e == 'D')
            *flags |= PCRE_DOLLAR_ENDONLY;
        else if (*e == 'U')
            *flags |= PCRE_UNGREEDY;
        else if (*e == 'X')
            *flags |= PCRE_EXTRA;
        else if (*e == 'D')
            *flags |= PCRE_DOLLAR_ENDONLY;
        else if (*e == 'u')
            *flags |= PCRE_UTF8;
#else
        if (*e == 'i')
            *flags = *flags | REG_ICASE;
        else if (*e == 'm')
            *flags |= REG_NEWLINE;
#endif
        else if (*e == 'g')
            *recursive = 1;
        ++e;
    }
    return s;
}

ci_regex_t ci_regex_build(const char *regex_str, int regex_flags)
{
#ifdef HAVE_PCRE
    pcre *re;
    const char *error;
    int erroffset;

    re = pcre_compile(regex_str, regex_flags, &error, &erroffset, NULL);

    if (re == NULL) {
        ci_debug_printf(2, "PCRE compilation failed at offset %d: %s\n", erroffset, error);
        return NULL;
    }
    return re;

#else
    int retcode;
    regex_t *regex = malloc(sizeof(regex_t));
    /*reset regex_struct*/
    memset(regex, 0, sizeof(regex_t));
    retcode = regcomp(regex, regex_str, regex_flags);
    if (retcode) {
        free(regex);
        regex = NULL;
    }
    return regex;
#endif
}

void ci_regex_free(ci_regex_t regex)
{
#ifdef HAVE_PCRE
    pcre_free((pcre *)regex);
#else
    regfree((regex_t *)regex);
    free(regex);
#endif
}

#ifdef HAVE_PCRE
#define OVECCOUNT 30    /* should be a multiple of 3 */
#endif

int ci_regex_apply(const ci_regex_t regex, const char *str, int len, int recurs, ci_list_t *matches, const void *user_data)
{
    int count = 0, i;
    ci_regex_replace_part_t parts;

    if (!str)
        return 0;

#ifdef HAVE_PCRE
    int ovector[OVECCOUNT];
    int rc;
    int offset = 0;
    int str_length = len >=0 ? len : strlen(str);
    do {
        memset(ovector, 0, sizeof(ovector));
        rc = pcre_exec(regex, NULL, str, str_length, offset, 0, ovector, OVECCOUNT);
        if (rc >= 0 && ovector[0] != ovector[1]) {
            ++count;
            ci_debug_printf(9, "Match pattern (pos:%d-%d): '%.*s'\n",
                            ovector[0], ovector[1], ovector[1]-ovector[0], str+ovector[0]);
            offset = ovector[1];
            if (matches) {
                parts.user_data = user_data;
                memset(parts.matches, 0, sizeof(ci_regex_matches_t));
                for (i = 0; i < 10 && ovector[2*i+1] > ovector[2*i]; ++i) {
                    ci_debug_printf(9, "\t sub-match pattern (pos:%d-%d): '%.*s'\n", ovector[2*i], ovector[2*i+1], ovector[2*i + 1] - ovector[2*i], str+ovector[2*i]);
                    parts.matches[i].s = ovector[2*i];
                    parts.matches[i].e = ovector[2*i+1];
                }
                ci_list_push_back(matches, (void *)&parts);
            }
        }
    } while (recurs && rc >=0  && offset < str_length);

#else
    int retcode;
    regmatch_t pmatch[10];
    do {
        if ((retcode = regexec(regex, str, 10, pmatch, 0)) == 0) {
            ++count;
            ci_debug_printf(9, "Match pattern (pos:%ld-%ld): '%.*s'\n", pmatch[0].rm_so, pmatch[0].rm_eo, (int)(pmatch[0].rm_eo - pmatch[0].rm_so), str+pmatch[0].rm_so);

            if (matches) {
                parts.user_data = user_data;
                memset(parts.matches, 0, sizeof(ci_regex_matches_t));
                for (i = 0; i < 10 && pmatch[i].rm_eo > pmatch[i].rm_so; ++i) {
                    ci_debug_printf(9, "\t sub-match pattern (pos:%ld-%ld): '%.*s'\n", pmatch[i].rm_so, pmatch[i].rm_eo, (int)(pmatch[i].rm_eo - pmatch[i].rm_so), str+pmatch[i].rm_so);
                    parts.matches[i].s = pmatch[i].rm_so;
                    parts.matches[i].e = pmatch[i].rm_eo;
                }
                ci_list_push_back(matches, (void *)&parts);
            }

            if (pmatch[0].rm_so >= 0 && pmatch[0].rm_eo >= 0 && pmatch[0].rm_so != pmatch[0].rm_eo) {
                str += pmatch[0].rm_eo;
                ci_debug_printf(8, "I will check again starting from: %s\n", str);
            } else /*stop here*/
                str = NULL;
        }
    } while (recurs && str && *str != '\0' && retcode == 0);
#endif

    ci_debug_printf(5, "ci_regex_apply matches count: %d\n", count);
    return count;
}
