#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "cfg_param.h"
#include "mem.h"
#include "array.h"
#include "debug.h"

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}


static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {NULL,NULL,NULL,NULL,NULL}
};

int print_str(void *data, const char *name, const void *value)
{
    const char *v = (const char *)value;
    ci_debug_printf(2, "\t%s: %s\n", name, v);
    return 0;
}

int mem_init();
int main(int argc,char *argv[])
{
    ci_str_array_t *arr_str;
    ci_ptr_array_t *arr_ptr;
    ci_vector_t *vect_str;
    ci_dyn_array_t *dyn_arr;
    const ci_array_item_t *item;
    int i, j;
    char name[128];
    char value[128];
    void *data;
    const char *strdata;
    ci_cfg_lib_init();
    mem_init();
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    ci_debug_printf(1, "Creating array of strings ... ");
    arr_str = ci_str_array_new(32768);
    for (i = 1; i< 128; i++) {
        sprintf(name, "name%d", i);
        sprintf(value, "value%d", i);
        ci_str_array_add(arr_str, name, value);
    }
    ci_debug_printf(1, "done  ...  test it ... ");

    ci_debug_printf(2, "\n\nArray of strings:\n");
    ci_str_array_iterate(arr_str, NULL, print_str);

    ci_debug_printf(2, "\nTest random access:\n");
    for (i = 0; i< 128; i++) {
        item = ci_str_array_get_item(arr_str, i);
        if (item) {
            ci_debug_printf(2, "\t%s:%s\n", item->name, (char *)item->value);
        }
    }
    ci_debug_printf(1, "done\n");

    ci_debug_printf(1, "Test pop1: \n");
    for (i = 0; i < 64; i++)
        ci_str_array_pop(arr_str);
    for (i = 64; i < 128; i++) {
        sprintf(name, "name%d", i);
        sprintf(value, "value%d", i);
        ci_str_array_add(arr_str, name, value);
    }

    ci_debug_printf(2, "Result: \n");
    ci_str_array_iterate(arr_str, NULL, print_str);

    ci_debug_printf(1, "Test pop 2: \n");
    while ((item = ci_str_array_pop(arr_str)) != NULL) {
        ci_debug_printf(2, " popped : %s %s \n", item->name, (char *)item->value);
    }

    ci_str_array_destroy(arr_str);
    ci_debug_printf(1, "done \n");

    ci_debug_printf(1, "Creating array of pointers ... ");
    arr_ptr = ci_ptr_array_new(32768);
    for (i = 1; i< 128; i++) {
        sprintf(name, "name%d", i);
        sprintf(value, "dynvalue%d", i);
        data = strdup(value);
        ci_ptr_array_add(arr_ptr, name, data);
    }
    ci_debug_printf(1, "done  ...  test it ... ");
    ci_debug_printf(2, "Array of pointers:\n");
    ci_ptr_array_iterate(arr_ptr, NULL, print_str);
    ci_debug_printf(1, "done\n");
    char buf[1024];
    ci_debug_printf(1, "Test pop on array of pointers...");
    while ((data = ci_ptr_array_pop_value(arr_ptr, buf, sizeof(buf))) != NULL) {
        ci_debug_printf(3, "Deleting : %s: %s\n", buf, (char *)data);
        free(data);
    }
    ci_debug_printf(1, "done\n");
    ci_ptr_array_destroy(arr_ptr);

    vect_str = ci_str_vector_create(4096);

    for (j = 1; j < 3; j++) {
        for (i = 1; i< 128; i++) {
            sprintf(value, "value: %d", i);
            strdata = ci_str_vector_add(vect_str, value);
            if (!strdata)
                ci_debug_printf(2, "Can not add: %s\n", value);
        }

        /*Check if casting works*/
        ci_debug_printf(1, "Test casting for vectors:");
        const char **p = ci_str_vector_cast_to_charchar(vect_str);
        const char **s;
        for (s = p; *s!=NULL; s++) {
            ci_debug_printf(2, "from charchar value: %s\n", *s);
        }
        ci_str_vector_t *v = ci_str_vector_cast_from_charchar(p);
        ci_debug_printf(1, "Returned vector max size: %d, itmes %d\n", (int)v->max_size, v->count);

        while ((strdata = ci_str_vector_pop(vect_str)) != NULL) {
            ci_debug_printf(2, "Popped value: %s\n", strdata);
        }
    }

    ci_str_vector_destroy(vect_str);


    ci_debug_printf(1, "\nTest for dynamic arrays\n");
    dyn_arr = ci_dyn_array_new(1024);
    for (i = 0, j = 0; i < 1024; ++i) {
        sprintf(name, "name%d", i);
        sprintf(value, "value%d", i);
        if (ci_dyn_array_add(dyn_arr, name, value, strlen(value) + 1) == NULL) {
            ci_debug_printf(1, "Failed to add : %s/%s!\n", name, value);
        } else
            j += strlen(name) + strlen(value) + 2;
    }
    ci_debug_printf(1, "Size of dynamic array: %d, of key/value pairs size: %d\n", ci_dyn_array_size(dyn_arr), j);
    for (i = 0, j = 0; i < ci_dyn_array_size(dyn_arr); ++i) {
        char *v = ci_dyn_array_value(dyn_arr, i);
        char *n = ci_dyn_array_name(dyn_arr, i);
        j += strlen(n) + strlen(v) + 2;
        ci_debug_printf(5, "%i = %p:%s/%s\n", i, ci_dyn_array_get_item(dyn_arr, i), ci_dyn_array_name(dyn_arr, i), (char *)ci_dyn_array_value(dyn_arr, i));
    }
    ci_debug_printf(1, "%d computed key/value pairs of summary size: %d\n", i, j);

    ci_debug_printf(1, "Search for %s: %s\n", "name123", (char *)ci_dyn_array_search(dyn_arr, "name123"));
    ci_debug_printf(1, "Search for %s: %s\n", "name1023", (char *)ci_dyn_array_search(dyn_arr, "name1023"));
    ci_debug_printf(1, "Search for %s: %s\n", "name0", (char *)ci_dyn_array_search(dyn_arr, "name0"));

    ci_debug_printf(1, "Search for %s: %s\n", "nameNotExist", (char *)ci_dyn_array_search(dyn_arr, "nameNotExist"));

    ci_dyn_array_destroy(dyn_arr);
    ci_debug_printf(1, "\nEnd of dynamic arrays test\n");

    return 0;
}
