#include "c-icap.h"
#include <errno.h>
#include "cfg_param.h"
#include "mem.h"
#include "debug.h"

/*************************************************************************/
/* Memory managment for config parameters definitions and implementation */

#define ALLOCATOR_SIZE 65536

ci_serial_allocator_t *cfg_params_allocator = NULL;

void ci_cfg_lib_init()
{
     cfg_params_allocator = ci_serial_allocator_create(ALLOCATOR_SIZE);
}

void ci_cfg_lib_reset()
{
     ci_serial_allocator_reset(cfg_params_allocator);
}

void *ci_cfg_alloc_mem(int size)
{
     return ci_serial_allocator_alloc(cfg_params_allocator, size);
}

/*********************************************************************/
/* Implementation of a mechanism to keep default values of parameters*/

struct cfg_default_value *default_values = NULL;

/*We does not care about memory managment here.  Default values list created only once at the beggining of c-icap
  and does not needed to free or reallocate memory... I think ...
*/
struct cfg_default_value *ci_cfg_default_value_store(void *param, void *value,
                                                     int size)
{
     struct cfg_default_value *dval, *dval_search;

     if (!(dval = malloc(sizeof(struct cfg_default_value))))
          return 0;
     dval->param = param;       /*Iam sure we can just set it to param_name, but..... */
     dval->size = size;
     if (!(dval->value = malloc(size))) {
          free(dval);
          return NULL;
     }
     memcpy(dval->value, value, size);
     dval->next = NULL;
     if (default_values == NULL) {
          default_values = dval;
          return dval;
     }
     dval_search = default_values;
     while (dval_search->next != NULL)
          dval_search = dval_search->next;
     dval_search->next = dval;
     return dval;
}

struct cfg_default_value *ci_cfg_default_value_replace(void *param, void *value)
{
     struct cfg_default_value *dval;
     dval = default_values;
     while (dval->param != param && dval != NULL)
          dval = dval->next;

     if (!dval)
          return NULL;

     memcpy(dval->value, value, dval->size);
     return dval;
}

void *ci_cfg_default_value_load(void *param, int size)
{
     struct cfg_default_value *dval;
     dval = default_values;
     while (dval->param != param && dval != NULL)
          dval = dval->next;

     if (!dval)
          return NULL;

     memcpy(param, dval->value, size);
     return param;
}

/****************************************************************/
/* Command line options implementation, function and structures */

void ci_args_usage(char *progname, struct options_entry *options)
{
     int i;
     printf("Usage : \n");
     printf("%s", progname);
     for (i = 0; options[i].name != NULL; i++)
          printf(" [%s %s]", options[i].name,
                 (options[i].parameter == NULL ? "" : options[i].parameter));
     printf("\n\n");
     for (i = 0; options[i].name != NULL; i++)
          printf("%s %s\t\t: %s\n", options[i].name,
                 (options[i].parameter == NULL ? "\t" : options[i].parameter),
                 options[i].msg);

}


struct options_entry *search_options_table(char *directive,
                                           struct options_entry *options)
{
     int i;
     for (i = 0; options[i].name != NULL; i++) {
          if (0 == strcmp(directive, options[i].name))
               return &options[i];
     }
     return NULL;
}


int ci_args_apply(int argc, char **argv, struct options_entry *options)
{
     int i;
     struct options_entry *entry;
     for (i = 1; i < argc; i++) {
          if ((entry = search_options_table(argv[i], options)) == NULL)
               return 0;
          if (entry->parameter) {
               if (++i >= argc)
                    return 0;
               (*(entry->action)) (entry->name, argv + i, entry->data, 0);
          }
          else
               (*(entry->action)) (entry->name, NULL, entry->data, 0);
     }
     return 1;
}


/****************************************************************************/
/*Various functions for setting parameters from command line or config file */

int ci_cfg_set_int(char *directive, char **argv, void *setdata, int reset)
{
     int val = 0;
     char *end;

     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(int));
          return 1;
     }

     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }

     errno = 0;
     val = strtoll(argv[0], &end, 10);
     if ((val == 0 && errno != 0))
          return 0;

     ci_cfg_default_value_store(setdata, setdata, sizeof(int));

     *((int *) setdata) = val;

     ci_debug_printf(1, "Setting parameter :%s=%d\n", directive, val);
     return 1;
}

int ci_cfg_set_str(char *directive, char **argv, void *setdata, int reset)
{
     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(char *));
          return 1;
     }

     if (argv == NULL || argv[0] == NULL) {
          return 0;
     }
     if (!(*((char **) setdata) = ci_cfg_alloc_mem(strlen(argv[0]) + 1))) {
          return 0;
     }

     ci_cfg_default_value_store(setdata, setdata, sizeof(char *));
     /*or better keep all string not just the pointer to default value? */

     strcpy(*((char **) setdata), argv[0]);
/*     *((char **) setdata) = (char *) strdup(argv[0]); */
     ci_debug_printf(1, "Setting parameter :%s=%s\n", directive, argv[0]);
     return 1;
}

int ci_cfg_onoff(char *directive, char **argv, void *setdata, int reset)
{
     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(int));
          return 1;
     }

     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }

     ci_cfg_default_value_store(setdata, setdata, sizeof(int));

     if (strcasecmp(argv[0], "on") == 0)
          *((int *) setdata) = 1;
     else if (strcasecmp(argv[0], "off") == 0)
          *((int *) setdata) = 0;
     else
          return 0;

     ci_debug_printf(1, "Setting parameter :%s=%d\n", directive,
                     *((int *) setdata));
     return 1;
}


int ci_cfg_disable(char *directive, char **argv, void *setdata, int reset)
{
     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(int));
          return 1;
     }

     ci_cfg_default_value_store(setdata, setdata, sizeof(int));
     *((int *) setdata) = 0;
     ci_debug_printf(1, "Disabling parameter %s\n", directive);
     return 1;

}

int ci_cfg_enable(char *directive, char **argv, void *setdata, int reset)
{
     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(int));
          return 1;
     }

     ci_cfg_default_value_store(setdata, setdata, sizeof(int));
     *((int *) setdata) = 1;
     ci_debug_printf(1, "Enabling parameter %s\n", directive);
     return 1;
}

int ci_cfg_size_off(char *directive, char **argv, void *setdata, int reset)
{
     ci_off_t val = 0;
     char *end;

     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(ci_off_t));
          return 1;
     }

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

     ci_cfg_default_value_store(setdata, setdata, sizeof(ci_off_t));

     if (val > 0)
          *((ci_off_t *) setdata) = val;
     ci_debug_printf(1, "Setting parameter :%s=%" PRINTF_OFF_T "\n", directive,
                     val);
     return val;
}


int ci_cfg_size_long(char *directive, char **argv, void *setdata, int reset)
{
     long int val = 0;
     char *end;

     if (setdata == NULL)
          return 0;

     if (reset) {
          ci_cfg_default_value_load(setdata, sizeof(int));
          return 1;
     }

     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
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

     ci_cfg_default_value_store(setdata, setdata, sizeof(long int));

     if (val > 0)
          *((long int *) setdata) = val;
     ci_debug_printf(1, "Setting parameter :%s=%ld\n", directive, val);
     return val;
}
