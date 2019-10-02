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

void *ci_cfg_alloc_mem(int size)
{
    return cfg_params_allocator->alloc(cfg_params_allocator, size);
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
    printf("c-icap version: %s\nConfigure script options: %s\nConfigured for host: %s\n", VERSION, C_ICAP_CONFIGURE_OPTIONS, C_ICAP_CONFIG_HOST_TYPE);
    return 1;
}
