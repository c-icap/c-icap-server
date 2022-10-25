/*
 *  Copyright (C) 2004-2010 Christos Tsantilas
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
#include <errno.h>
#include "cfg_param.h"
#include "mem.h"
#include "debug.h"
#if defined(HAVE_GNU_LIBC_VERSION_H)
#include <gnu/libc-version.h>
#endif

/*************************************************************************/
/* Memory managment for config parameters definitions and implementation */

#define ALLOCATOR_SIZE 65536

ci_mem_allocator_t *cfg_params_allocator = NULL;

void ci_cfg_lib_init()
{
    cfg_params_allocator = ci_create_serial_allocator(ALLOCATOR_SIZE);
}

void ci_cfg_lib_reset()
{
    cfg_params_allocator->reset(cfg_params_allocator);
}

void ci_cfg_lib_destroy()
{
    ci_mem_allocator_destroy(cfg_params_allocator);
}

void *ci_cfg_alloc_mem(int size)
{
    return cfg_params_allocator->alloc(cfg_params_allocator, size);
}

const char *ci_lib_version_string() {
    return VERSION;
}

/*ci_conf_table functions*/
ci_conf_entry_t *ci_cfg_conf_table_allocate(int entries)
{
    ci_conf_entry_t *table = (ci_conf_entry_t *)calloc(entries + 1, sizeof(ci_conf_entry_t));
    return table;
}

void ci_cfg_conf_table_release(ci_conf_entry_t *table)
{
    if (table)
        free(table);
}

void ci_cfg_conf_table_push(ci_conf_entry_t *table, int entries, const char *name, void *data, int (*action)(const char *, const char **argv, void *), const char *msg )
{
    int i;
    if (!table)
        return;
    for(i = 0; i < entries; i++) {
        if (table[i].name == NULL) {
            table[i].name = name;
            table[i].data = data;
            table[i].action = action;
            table[i].msg = msg;
            return;
        }
    }
}

int ci_cfg_conf_table_configure(ci_conf_entry_t *table, const char *table_name, const char *varname, const char **argv)
{
    ci_conf_entry_t *entry = NULL;
    int i;
    for (i = 0; table[i].name != NULL; i++) {
        if (0 == strcmp(varname, table[i].name))
            entry = &table[i];
    }

    if (!entry) {
        ci_debug_printf(1, "Variable %s%s%s not found!\n", table_name, ((table_name && table_name[0]) ? "." : "" ),varname);
        return 0;
    }
    return entry->action(entry->name, argv, entry->data);
}

/****************************************************************/
/* Command line options implementation, function and structures */

void ci_args_usage(const char *progname, struct ci_options_entry *options)
{
    int i;
    printf("Usage : \n");
    printf("%s", progname);
    for (i = 0; options[i].name != NULL; i++) {
        if (options[i].name[0] == '$')
            printf(" [file1] [file2] ...");
        else
            printf(" [%s %s]", options[i].name,
                   (options[i].parameter == NULL ? "" : options[i].parameter));
    }
    printf("\n\n");
    for (i = 0; options[i].name != NULL; i++)
        if (options[i].name[0] == '$')
            printf(" [file1] [file2] ...\t: %s\n", options[i].msg);
        else
            printf("%s %s\t\t: %s\n", options[i].name,
                   (options[i].parameter == NULL ? "\t" : options[i].parameter),
                   options[i].msg);
}


struct ci_options_entry *search_options_table(const char *directive,
        struct ci_options_entry *options)
{
    int i;
    const char *option_search;
    if (directive[0] != '-')
        option_search = "$$";
    else
        option_search = directive;

    for (i = 0; options[i].name != NULL; i++) {
        if (0 == strcmp(option_search, options[i].name))
            return &options[i];
    }
    return NULL;
}


int ci_args_apply(int argc, char *argv[], struct ci_options_entry *options)
{
    int i;
    struct ci_options_entry *entry;
    const char *act_args[2];
    act_args[1] = NULL;
    for (i = 1; i < argc; i++) {
        if ((entry = search_options_table(argv[i], options)) == NULL)
            return 0;
        if (entry->parameter) {
            if (++i >= argc)
                return 0;
            act_args[0] = argv[i];
            (*(entry->action)) (entry->name, act_args, entry->data);
        } else {
            /*maybe is the "$$" directive ....*/
            if (strcmp(entry->name, "$$") == 0) {
                act_args[0] = argv[i];
                (*(entry->action)) (entry->name, act_args, entry->data);
            } else
                (*(entry->action)) (entry->name, NULL, entry->data);
        }
    }
    return 1;
}


/****************************************************************************/
/*Various functions for setting parameters from command line or config file */

int ci_cfg_set_int(const char *directive, const char **argv, void *setdata)
{
    int val = 0;
    char *end;

    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }

    errno = 0;
    val = strtoll(argv[0], &end, 10);
    if ((val == 0 && errno != 0))
        return 0;

    *((int *) setdata) = val;

    ci_debug_printf(2, "Setting parameter: %s=%d\n", directive, val);
    return 1;
}

int ci_cfg_set_str(const char *directive, const char **argv, void *setdata)
{
    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    const size_t str_size = strlen(argv[0]) + 1;
    if (!(*((char **) setdata) = ci_cfg_alloc_mem(str_size))) {
        return 0;
    }

    strncpy(*((char **) setdata), argv[0], str_size);
    (*((char **) setdata))[str_size - 1] = '\0';
    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    return 1;
}

int ci_cfg_onoff(const char *directive, const char **argv, void *setdata)
{
    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }

    if (strcasecmp(argv[0], "on") == 0)
        *((int *) setdata) = 1;
    else if (strcasecmp(argv[0], "off") == 0)
        *((int *) setdata) = 0;
    else
        return 0;

    ci_debug_printf(2, "Setting parameter: %s=%d\n", directive,
                    *((int *) setdata));
    return 1;
}


int ci_cfg_disable(const char *directive, const char **argv, void *setdata)
{
    if (setdata == NULL)
        return 0;

    *((int *) setdata) = 0;
    ci_debug_printf(2, "Disabling parameter %s\n", directive);
    return 1;

}

int ci_cfg_enable(const char *directive, const char **argv, void *setdata)
{
    if (setdata == NULL)
        return 0;

    *((int *) setdata) = 1;
    ci_debug_printf(2, "Enabling parameter %s\n", directive);
    return 1;
}

int ci_cfg_size_off(const char *directive, const char **argv, void *setdata)
{
    ci_off_t val = 0;
    char *end;

    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }

    errno = 0;
    val = ci_strto_off_t(argv[0], &end, 10);

    if ((val == 0 && errno != 0) || val < 0)
        return 0;

    if (*end == 'k' || *end == 'K')
        val = val * 1024;
    else if (*end == 'm' || *end == 'M')
        val = val * 1024 * 1024;

    if (val > 0)
        *((ci_off_t *) setdata) = val;
    ci_debug_printf(2, "Setting parameter: %s=%" PRINTF_OFF_T "\n", directive,
                    (CAST_OFF_T) val);
    return 1;
}


int ci_cfg_size_long(const char *directive, const char **argv, void *setdata)
{
    long int val = 0;
    char *end;

    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive: %s\n", directive);
        return 0;
    }

    errno = 0;
    val = strtol(argv[0], &end, 10);

    if ((val == 0 && errno != 0) || val < 0)
        return 0;

    if (*end == 'k' || *end == 'K')
        val = val * 1024;
    else if (*end == 'm' || *end == 'M')
        val = val * 1024 * 1024;

    if (val > 0)
        *((long int *) setdata) = val;
    ci_debug_printf(2, "Setting parameter: %s=%ld\n", directive, val);
    return 1;
}

int ci_cfg_set_octal(const char *directive, const char **argv, void *setdata)
{
    int val = 0;
    char *end;

    if (!setdata)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }

    errno = 0;
    val = strtoll(argv[0], &end, 8);
    if ((val == 0 && errno != 0))
        return 0;

    *((int *) setdata) = val;

    ci_debug_printf(2, "Setting parameter: %s=0%.3o\n", directive, val);
    return 1;
}

int ci_cfg_set_float(const char *directive,const char **argv,void *setdata)
{
    float val = 0;
    char *end;

    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive: %s\n", directive);
        return 0;
    }

    errno = 0;
    val = strtof(argv[0], &end);

    if ((val == 0 && errno != 0) || val < 0)
        return 0;

    *((float *) setdata) = val;
    ci_debug_printf(2, "Setting parameter: %s=%f\n", directive, (double)val);
    return 1;

}

int ci_cfg_set_double(const char *directive,const char **argv,void *setdata)
{
    double val = 0;
    char *end;

    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive: %s\n", directive);
        return 0;
    }

    errno = 0;
    val = strtod(argv[0], &end);

    if ((val == 0 && errno != 0) || val < 0)
        return 0;

    *((double *) setdata) = val;
    ci_debug_printf(2, "Setting parameter: %s=%f\n", directive, val);
    return 1;

}

int ci_cfg_set_int_range(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;

    struct ci_cfg_int_range *range = (struct ci_cfg_int_range *)setdata;
    if (!range->data)
        return 0;

    int tmpVal;
    if (!ci_cfg_set_int(directive, argv, (void *)&tmpVal))
        return 0;
    if (tmpVal < range->start || tmpVal > range->end) {
        ci_debug_printf(1, "Please use an integer value between %d and %d for directive '%s'\n", range->start, range->end, directive);
        return 0;
    }
    *range->data = tmpVal;
    return 1;
}

int ci_cfg_version(const char *directive, const char **argv, void *setdata)
{
    if (setdata)
        *((int *) setdata) = 1;
    printf("%s\n", VERSION);
    return 1;
}

int ci_cfg_build_info(const char *directive, const char **argv, void *setdata)
{
    if (setdata)
        *((int *) setdata) = 1;
    printf("c-icap version: %s\n"
           "Configure script options: %s\n"
           "Configured for host: %s\n"
#if defined(__clang__) /* Clang also reports gcc-4.2.1*/
           "Compiled with: clang version %s\n"
#define __SUBGCC
#endif
#if defined(__MINGW32__)
           "Compiled with: mingw-w32-%d.%d\n"
#define __SUBGCC
#endif
#if defined(__MINGW64__)
           "Compiled with: mingw-w64-%d.%d\n"
#define __SUBGCC
#endif
#if defined(__GNUC__)
#if defined(__SUBGCC)
           "With extensions for: "
#else
           "Compiled with: "
#endif
           "gcc-%d.%d.%d\n"
#endif

#if defined(__GLIBC__)
            "Compiled with: glibc-%d.%d\n"
#if defined(HAVE_GNU_LIBC_VERSION_H)
            "Running with: glibc-%s %s\n"
#endif
#endif
            "%s\n",
            VERSION,
            C_ICAP_CONFIGURE_OPTIONS,
            C_ICAP_CONFIG_HOST_TYPE,
#if defined(__clang__)
            __clang_version__,
#endif
#if defined(__MINGW32__)
            __MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION,
#endif
#if defined(__MINGW64__)
            __MINGW64_VERSION_MAJOR, __MINGW64_VERSION_MINOR,
#endif
#if defined(__GNUC__)
             __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
#endif

#if defined(__GLIBC__)
            __GLIBC__, __GLIBC_MINOR__,
#if defined(HAVE_GNU_LIBC_VERSION_H)
            gnu_get_libc_version(), gnu_get_libc_release(),
#endif
#endif
            "");
    return 1;
}
