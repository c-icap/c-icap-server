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
    {"-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
     "The debug level"},
    {NULL,NULL,NULL,NULL,NULL}
};

int print_str(void *data, const char *name, const void *value)
{
    const char *v = (const char *)value;
    ci_debug_printf(2, "\t%s: %s\n", name, v);
    return 0;
}

int mem_init();
struct obj {
    char c;
    char buf[64];
};

void fill_obj(struct obj *o, char c)
{
    o->c = c;
    memset(o->buf, c, 64);
    o->buf[63] = '\0';
}

int check_obj(void *data, const void *obj)
{
    int i;
    struct obj *o = (struct obj *)obj;
    int *k = (int *)data;
    (*k)++;
    if (!o) {
        ci_debug_printf(1, "Empty data stored in list?\n");
        return -1;
    }
    for (i=0; i < 62; i++) {
        if (o->c != o->buf[i]) {
            ci_debug_printf(1, "Not valid data stored in list?\n");
            return -1;
        }
    }
    return 0;
}

struct cb_rm_data {
    ci_list_t *list;
    char item;
};

int cb_remove_anobj(void *data, const void *obj)
{
   struct obj *o = (struct obj *)obj;
   struct cb_rm_data *rd = (struct cb_rm_data *)data;
   if (o->c == rd->item) {
       ci_list_remove(rd->list, o);
   }
   ci_debug_printf(5, "item->%c %s\n", o->c, (o->c == rd->item ? "rm" : ""));
   return 0;
}

int main(int argc,char *argv[])
{
    ci_list_t *list;
    struct obj o;
    int i, k, l;
    char c;

    ci_cfg_lib_init();
    mem_init();
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    
    list = ci_list_create(4096, sizeof(struct obj));
    if (!list) {
        ci_debug_printf(1, "Error creating list\n");
        exit(-1);
    }

    for (l = 0; l < 2; l++) {

        for (i = 0, k =0; i < 1024; i++) {
            for (c = 'a'; c < 'z'; c++) {
                fill_obj(&o, c);
                if (c % 2)
                    ci_list_push(list, &o);
                else
                    ci_list_push_back(list, &o);
                k++;
            }
        }
        ci_debug_printf(1, "OK added %d items\n", k);
        k = 0;
        ci_debug_printf(1, "Check list...\n");
        ci_list_iterate(list, (void *)&k, check_obj);
        ci_debug_printf(1, "Counted %d valid items\n", k);

        ci_debug_printf(1, "Remove all 's' objects...\n");
        k = 0;
        fill_obj(&o, 's');
        while(ci_list_remove(list, &o)) {
            k++;
        }

        fill_obj(&o, 'k');
        while(ci_list_remove(list, &o)) {
            k++;
        }
        ci_debug_printf(1, "Removed %d objects\n", k);

        ci_debug_printf(1, "Check obj removal on iterate\n");
        struct cb_rm_data rd;
        rd.list = list;
        rd.item = 'l';
        ci_list_iterate(list, (void *)&rd, cb_remove_anobj);
        ci_debug_printf(1, "done\n");

        k = 0;
        ci_debug_printf(1, "Check list...\n");
        ci_list_iterate(list, (void *)&k, check_obj);
        ci_debug_printf(1, "Counted %d valid items\n", k);


        ci_debug_printf(1, "Add back the removed objects\n");
        k = 0;
        fill_obj(&o, 's');
        for (i=0; i < 1024; i++) {
            ci_list_push(list, &o);
            k++;
        }
        fill_obj(&o, 'k');
        for (i=0; i < 1024; i++) {
            ci_list_push_back(list, &o);
            k++;
        }
        ci_debug_printf(1, "Add %d objects\n", k);

        k = 0;
        ci_debug_printf(1, "Check list...\n");
        ci_list_iterate(list, (void *)&k, check_obj);
        ci_debug_printf(1, "Counted %d valid items\n", k);

        ci_debug_printf(1, "Remove 1024 from the head and 1024 from the tail\n");
        for (i=0; i < 1024; i++) {
            if (!ci_list_pop(list, &o))
                ci_debug_printf(1, "Not enough objects in list!\n");
        }
        for (i=0; i < 1024; i++) {
            if (!ci_list_pop_back(list, &o))
                ci_debug_printf(1, "Not enough objects in list!\n");
            k++;
        }
        k = 0;
        ci_debug_printf(1, "Check list...\n");
        ci_list_iterate(list, (void *)&k, check_obj);
        ci_debug_printf(1, "Counted %d valid items\n", k);

        fill_obj(&o, 'l');
        ci_debug_printf(1, "Find one object of '%c'\n", 'l');
        if (!ci_list_search(list, &o)) {
            ci_debug_printf(1, "\t Not Found (correct)\n");
        } else {
            ci_debug_printf(1, "\t Found! (wrong!)\n");
        }

        fill_obj(&o, 's');
        ci_debug_printf(1, "Find one object of '%c'\n", 's');
        if (!ci_list_search(list, &o)) {
            ci_debug_printf(1, "\t Not Found (correct)\n");
        } else {
            ci_debug_printf(1, "\t Found! (wrong!)\n");
        }

        fill_obj(&o, 'k');
        ci_debug_printf(1, "Find one object of '%c'\n", 'k');
        if (!ci_list_search(list, &o)) {
            ci_debug_printf(1, "\t Not Found (correct)\n");
        } else {
            ci_debug_printf(1, "\t Found!(wrong!)\n");
        }

        fill_obj(&o, 'd');
        ci_debug_printf(1, "Find one object of '%c'\n", 'd');
        if (!ci_list_search(list, &o)) {
            ci_debug_printf(1, "\t Not Found (wrong)\n");
        } else {
            ci_debug_printf(1, "\t Found! (correct)\n");
        }

        k = 0;
        while (ci_list_pop(list, &o)) k++;
        ci_debug_printf(1, "Removed %d items\n", k);
    }

    ci_list_destroy(list);
    ci_debug_printf(1, "Test finished!\n");
    return 0;
}
