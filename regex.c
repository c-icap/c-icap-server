#include "common.h"
#include "debug.h"
#include "array.h"
#include "mem.h"
#include "ci_regex.h"
#if defined(HAVE_PCRE2)
#define PCRE2_CODE_UNIT_WIDTH 8 // TODO: make it configurable via configure
#include <pcre2.h>
#elif defined(HAVE_PCRE)
#include <pcre.h>
#else
#include <regex.h>
#endif


#if defined(HAVE_PCRE2)
pcre2_general_context *pcre2GenContext = NULL;
pcre2_match_context *pcre2MatchContext = NULL;

void *pcre2_malloc(PCRE2_SIZE size, void *udata)
{
    void *p = ci_buffer_alloc(size);
    return p;
}

void pcre2_free(void *p, void *udata)
{
    ci_buffer_free(p);
}

struct {
    char *name;
    uint32_t flag;
} pcre2_flags[] = {
    {"ANCHORED", 0x80000000u},
    {"NO_UTF_CHECK", 0x40000000u},
    {"ENDANCHORED", 0x20000000u},
    {"ALLOW_EMPTY_CLASS", 0x00000001u},
    {"ALT_BSUX", 0x00000002u},
    {"AUTO_CALLOUT", 0x00000004u},
    {"CASELESS", 0x00000008u},
    {"DOLLAR_ENDONLY", 0x00000010u},
    {"DOTALL", 0x00000020u},
    {"DUPNAMES", 0x00000040u},
    {"EXTENDED", 0x00000080u},
    {"FIRSTLINE", 0x00000100u},
    {"MATCH_UNSET_BACKREF", 0x00000200u},
    {"MULTILINE", 0x00000400u},
    {"NEVER_UCP", 0x00000800u},
    {"NEVER_UTF", 0x00001000u},
    {"NO_AUTO_CAPTURE", 0x00002000u},
    {"NO_AUTO_POSSESS", 0x00004000u},
    {"NO_DOTSTAR_ANCHOR", 0x00008000u},
    {"NO_START_OPTIMIZE", 0x00010000u},
    {"UCP", 0x00020000u},
    {"UNGREEDY", 0x00040000u},
    {"UTF", 0x00080000u},
    {"NEVER_BACKSLASH_C", 0x00100000u},
    {"ALT_CIRCUMFLEX", 0x00200000u},
    {"ALT_VERBNAMES", 0x00400000u},
    {"USE_OFFSET_LIMIT", 0x00800000u},
    {"EXTENDED_MORE", 0x01000000u},
    {"LITERAL", 0x02000000u},
    {"MATCH_INVALID_UTF", 0x04000000u},
    {NULL, 0}
};

int pcre2_flag_parse(const char **f)
{
    const char *s = (*f);
    if (*s != '<')
        return 0;
    s++;
    const char *e = strchr(s, '>');
    if (e)
        *f = e;
    else {
        e = s + strlen(s);
        *f = e - 1;
    }
    size_t len = e - s;
    e--;
    if (!len)
        return 0;

    int i;
    for (i = 0; pcre2_flags[i].name != NULL; i++) {
        if (strncmp(pcre2_flags[i].name, s, len) == 0)
            return pcre2_flags[i].flag;
    }
    return 0;
}

#endif

void ci_regex_memory_init()
{
#if defined(HAVE_PCRE2)
    pcre2GenContext = pcre2_general_context_create(pcre2_malloc, pcre2_free,  NULL);
    pcre2MatchContext = pcre2_match_context_create(pcre2GenContext);
#endif
}

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
#if defined(HAVE_PCRE2)
    *flags |= PCRE2_NEWLINE_ANY;
    *flags |= PCRE2_NEWLINE_ANYCRLF;
#elif defined(HAVE_PCRE)
    *flags |= PCRE_NEWLINE_ANY;
    *flags |= PCRE_NEWLINE_ANYCRLF;
#else
    *flags |= REG_EXTENDED;
//    *flags |= REG_NOSUB;
#endif

    while (*e != '\0') {
#if defined(HAVE_PCRE2)
        if (*e == 'i')
            *flags = *flags | PCRE2_CASELESS;
        else if (*e == 'm')
            *flags |= PCRE2_MULTILINE;
        else if (*e == 's')
            *flags |= PCRE2_DOTALL;
        else if (*e == 'x')
            *flags |= PCRE2_EXTENDED;
        else if (*e == 'A')
            *flags |= PCRE2_ANCHORED;
        else if (*e == 'D')
            *flags |= PCRE2_DOLLAR_ENDONLY;
        else if (*e == 'U')
            *flags |= PCRE2_UNGREEDY;
        else if (*e == 'u')
            *flags |= PCRE2_UTF | PCRE2_UCP;
        else if (*e == '<') {
            *flags |= pcre2_flag_parse(&e);
        }
#elif defined(HAVE_PCRE)
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
#if defined(HAVE_PCRE2)
    pcre2_code *re;
    int errcode;
    PCRE2_SIZE erroffset;
    re =  pcre2_compile((PCRE2_SPTR)regex_str, PCRE2_ZERO_TERMINATED, (uint32_t)regex_flags, &errcode, &erroffset, NULL);
    if (re == NULL) {
        PCRE2_UCHAR errbuf[256];
        pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
        ci_debug_printf(2, "PCRE2 compilation of '%s' failed at offset %d: %s\n", regex_str, (int)erroffset, errbuf);
        return NULL;
    }
    return (ci_regex_t)re;
#elif defined(HAVE_PCRE)
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
#if defined(HAVE_PCRE2)
    pcre2_code_free((pcre2_code *)regex);
#elif defined(HAVE_PCRE)
    pcre_free((pcre *)regex);
#else
    regfree((regex_t *)regex);
    free(regex);
#endif
}

#ifdef HAVE_PCRE
#define OVECCOUNT (3*CI_REGEX_SUBMATCHES)    /* should be a multiple of 3 */
#endif

int ci_regex_apply(const ci_regex_t regex, const char *str, int len, int recurs, ci_list_t *matches, const void *user_data)
{
    int count = 0, i;
    ci_regex_replace_part_t parts;

    if (!str || len == 0)
        return 0;

#if defined(HAVE_PCRE2)
    PCRE2_SIZE offset = 0;
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(regex, pcre2GenContext);
    do {
        int str_length = len > 0 ? len : PCRE2_ZERO_TERMINATED;
        int rcaptures = pcre2_match(regex, (PCRE2_SPTR)str, str_length, offset, 0, match_data, pcre2MatchContext);
        if (rcaptures >= 0)
            count++;
        if (rcaptures > 0 && matches) {
            PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            for (i = 0; i < rcaptures && i < CI_REGEX_SUBMATCHES; i++) {
                _CI_ASSERT(ovector[2*i+1] >= ovector[2*i]);
                if (matches) {
                    parts.user_data = user_data;
                    memset(parts.matches, 0, sizeof(ci_regex_matches_t));
                    ci_debug_printf(9, "\t sub-match pattern (pos:%d-%d): '%.*s'\n", (int)ovector[2*i], (int)ovector[2*i+1], (int)(ovector[2*i + 1] - ovector[2*i]), str+ovector[2*i]);
                    parts.matches[i].s = ovector[2*i];
                    parts.matches[i].e = ovector[2*i+1];
                }
                ci_list_push_back(matches, (void *)&parts);
            }
            offset = ovector[1]; /*Assert that it is less than strlen(str)?*/
        } else if (rcaptures < 0) {
            /* maybe check and report the exact error?*/
            offset = 0;
        } /* the rcaptures==0 not possible for our case, but maybe check and warn?*/
    } while (recurs && offset > 0);
    pcre2_match_data_free(match_data);
#elif defined(HAVE_PCRE)
    int ovector[OVECCOUNT];
    int rc;
    int offset = 0;
    int str_length = len > 0 ? len : strlen(str);
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
                for (i = 0; i < CI_REGEX_SUBMATCHES && ovector[2*i+1] > ovector[2*i]; ++i) {
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
    regmatch_t pmatch[CI_REGEX_SUBMATCHES];
    char *tmpS = NULL;
    const char *s;
    if (len > 0 && len < strlen(str)) {
        tmpS = ci_buffer_alloc(len + 1);
        _CI_ASSERT(tmpS);
        memcpy(tmpS, str, len);
        tmpS[len] = '\0';
        s = tmpS;
    } else
        s = str;
    do {
        if ((retcode = regexec(regex, s, CI_REGEX_SUBMATCHES, pmatch, 0)) == 0) {
            ++count;
            ci_debug_printf(9, "Match pattern (pos:%d-%d): '%.*s'\n", (int)pmatch[0].rm_so, (int)pmatch[0].rm_eo, (int)(pmatch[0].rm_eo - pmatch[0].rm_so), s+pmatch[0].rm_so);

            if (matches) {
                parts.user_data = user_data;
                memset(parts.matches, 0, sizeof(ci_regex_matches_t));
                for (i = 0; i < CI_REGEX_SUBMATCHES && pmatch[i].rm_eo > pmatch[i].rm_so; ++i) {
                    ci_debug_printf(9, "\t sub-match pattern (pos:%d-%d): '%.*s'\n", (int)pmatch[i].rm_so, (int)pmatch[i].rm_eo, (int)(pmatch[i].rm_eo - pmatch[i].rm_so), s + pmatch[i].rm_so);
                    parts.matches[i].s = pmatch[i].rm_so;
                    parts.matches[i].e = pmatch[i].rm_eo;
                }
                ci_list_push_back(matches, (void *)&parts);
            }

            if (pmatch[0].rm_so >= 0 && pmatch[0].rm_eo >= 0 && pmatch[0].rm_so != pmatch[0].rm_eo) {
                s += pmatch[0].rm_eo;
                ci_debug_printf(8, "I will check again starting from: %s\n", s);
            } else /*stop here*/
                s = NULL;
        }
    } while (recurs && s && *s != '\0' && retcode == 0);
    if (tmpS)
        ci_buffer_free(tmpS);
#endif

    ci_debug_printf(5, "ci_regex_apply string '%s' matches count: %d\n", str, count);
    return count;
}
