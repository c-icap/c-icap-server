#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "dlib.h"
#include "module.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"
#include "ci_threads.h"


void init_internal_lookup_tables();

char *path;
char **keys = NULL;
int threadsnum = 0;
int queries_max_num = 0;
char *keysfile;
int USE_DEBUG_LEVEL = -1;
struct ci_lookup_table *table = NULL;

int queries_num;
ci_thread_mutex_t mtx;

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}


int load_module(const char *directive,const char **argv,void *setdata)
{
    CI_DLIB_HANDLE lib;
    common_module_t *module;

    if (argv == NULL || argv[0] == NULL)
        return 0;

    lib = ci_module_load(argv[0],"./");

    if (!lib) {
        printf("Error opening module :%s\n",argv[0]);
        return 0;
    }

    module = ci_module_sym(lib, "module");

    if (!module) {
        printf("Error opening module %s: can not find symbol module\n",argv[0]);
        return 0;
    }

    if (module->init_module)
        module->init_module(NULL);
    if (module->post_init_module)
        module->post_init_module(NULL);

    return 1;
}

int cfg_set_str_list(const char *directive, const char **argv, void *setdata)
{
    int i;
    char ***list = (char ***)setdata;
    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    if (!*list)
        *list = calloc(1024, sizeof(char *));

    for (i = 0; i < 1023 && (*list)[i]; ++i);
    if ((*list)[i] == NULL)
        (*list)[i] = strdup(argv[0]);
    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    return 1;
}

static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-m", "module", NULL, load_module,
        "The path of the table"
    },
    {
        "-p", "table_path", &path, ci_cfg_set_str,
        "The path of the table"
    },
    {
        "-k", "key     ", &keys, cfg_set_str_list,
        "The the key to search. It can used multiple times"
    },
    {
        "-K", "keys_file", &keysfile, ci_cfg_set_str,
        "A file to load the keys"
    },
    {
        "-t", "threads_num", &threadsnum, ci_cfg_set_int,
        "The threads number to start"
    },
    {
        "-n", "queries_num", &queries_max_num, ci_cfg_set_int,
        "The number of queries to run"
    },

    {NULL,NULL,NULL,NULL,NULL}
};

void run_test()
{
    void *e,*v,**vals;
    char *key;
    int i, k;
    do {
        for (k = 0; keys[k] != NULL && k < 1024 && queries_num < queries_max_num; ++k) {
            ci_thread_mutex_lock(&mtx);
            int reqId = ++queries_num;
            ci_thread_mutex_unlock(&mtx);

            key = keys[k];
            e = table->search(table,key,&vals);
            if (e) {
                printf("Result %d :\n\t%s:",reqId, key);
                if (vals) {
                    for (v = vals[0], i = 0; v != NULL; v = vals[++i]) {
                        printf("%s ",(char *)v);
                    }
                }
                printf("\n\n");
            } else {
                printf("Result %d: Key '%s' not found\n\n", reqId, key);
            }
        }
    } while (queries_num < queries_max_num);
}

int mem_init();
int main(int argc,char *argv[])
{
    ci_cfg_lib_init();
    mem_init();
    init_internal_lookup_tables();

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options) || !path || !keys) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    if (queries_max_num <= 0)
        while(keys[queries_max_num] !=0) queries_max_num++;

    ci_thread_mutex_init(&mtx);

    table = ci_lookup_table_create(path);
    if (!table) {
        printf("Error creating table\n");
        return -1;
    }

    if (!table->open(table)) {
        printf("Error opening table\n");
        return -1;
    }
    if (threadsnum <= 1) {
        run_test();
    } else {
        int i;
        ci_thread_t *threads;
        threads = malloc(sizeof(ci_thread_t) * threadsnum);
        for (i = 0; i < threadsnum; i++)  threads[i] = 0;
        for(i = 0; i < threadsnum; i++) {
            ci_debug_printf(8, "Thread %d started\n", i);
            ci_thread_create(&(threads[i]),
                             (void *(*)(void *)) run_test,
                             (void *) NULL /*data*/);

        }

        for (i = 0; i < threadsnum; i++) {
            ci_thread_join(threads[i]);
            ci_debug_printf(6, "Thread %d exited\n", i);
        }
        free(threads);
    }
    int i;
    for (i = 0; keys[i] != NULL; ++i)
        free(keys[i]);
    free(keys);
    ci_lookup_table_destroy(table);
    ci_thread_mutex_destroy(&mtx);
    return 0;
}
