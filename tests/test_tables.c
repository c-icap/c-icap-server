#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "commands.h"
#include "dlib.h"
#include "module.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"
#include "ci_threads.h"
#include "util.h"


void init_internal_lookup_tables();

char *path;

struct KeyList{
    char **indx;
    int num;
    int max;
} keys = {
    NULL,
    0,
    0
};

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

common_module_t * ci_common_module_build(const char *name, int (*init_module)(struct ci_server_conf *server_conf), int (*post_init_module)(struct ci_server_conf *server_conf), void (*close_module)(), struct ci_conf_entry *conf_table)
{
    common_module_t *mod = malloc(sizeof(common_module_t));
    mod->name = name;
    mod->init_module = init_module;
    mod->post_init_module = post_init_module;
    mod->close_module = close_module;
    mod->conf_table = conf_table;
    return mod;
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
        common_module_t *(*module_builder)() = NULL;
        if ((module_builder = ci_module_sym(lib, "__ci_module_build"))) {
            ci_debug_printf(2, "New c-icap modules initialization procedure\n");
            module = (*module_builder)();
        }
    }

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

/*
  Some lookup tables implementations uses commands to register initialization
  handler at c-icap kids startup. Implement basic commands for testing.
*/
ci_command_t Commands[128];
int CommandsNum = 0;

void ci_command_register_action(const char *name, int type, void *data, void (*command_action) (const char *name, int type, void *data))
{
    if (CommandsNum >= 128)
        return; /*Do Nothing*/
    strncpy(Commands[CommandsNum].name, name, sizeof(Commands[CommandsNum].name) - 1);
    Commands[CommandsNum].type = type;
    Commands[CommandsNum].command_action_extend = command_action;
    Commands[CommandsNum].data = data;
    CommandsNum++;
}

void execute_commands(int type)
{
    int i;
    for (i = 0; i < CommandsNum; i++) {
        if (!(type & Commands[i].type))
            continue;
        if (type & CI_CMD_CHILD_START) {
            ci_debug_printf(2, "Exec CI_CMD_CHILD_START command: %s\n", Commands[i].name);
            Commands[i].command_action_extend(Commands[i].name, type, Commands[i].data);
        }
    }
}


#define MAX_LIST_SIZE 1024
int cfg_set_str_list(const char *directive, const char **argv, void *setdata)
{
    struct KeyList *list = (struct KeyList*)setdata;
    if (setdata == NULL)
        return 0;

    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    assert(list);
    assert(list->indx);
    if (list->num >= list->max)
        return 0;

    list->indx[list->num] = strdup(argv[0]);
    list->num++;

    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    return 1;
}

void load_keys_from_file(struct KeyList *list, const char *name)
{
    FILE *f;
    if ((f = fopen(name, "r+")) == NULL) {
        ci_debug_printf(1, "Error opening file: %s\n", name);
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        line[sizeof(line) - 1] = '\0';
        ci_str_trim(line);
        printf("add line : %s\n", line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        if (list->num >= list->max)
            break;
        list->indx[list->num++] = strdup(line);
    }
    fclose(f);
    ci_debug_printf(2, "%d keys loaded for testing\n", list->num);
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
    if (!keys.num)
        return;
    do {
        for (k = 0; k < keys.num && queries_num < queries_max_num; ++k) {
            ci_thread_mutex_lock(&mtx);
            int reqId = ++queries_num;
            ci_thread_mutex_unlock(&mtx);

            key = keys.indx[k];
            e = table->search(table, key, &vals);
            if (e) {
                char valuesStr[1024];
                if (vals) {
                    size_t written = 0;
                    for (v = vals[0], i = 0; v != NULL; v = vals[++i]) {
                        if (written < sizeof(valuesStr))
                            written += snprintf(valuesStr + written, sizeof(valuesStr) - written, "%s ", (char *)v);
                    }
                }
                ci_debug_printf(2, "Result %d :\n\t%s: %s\n", reqId, key, valuesStr);
            } else {
                ci_debug_printf(2, "Result %d: Key '%s' is not found\n\n", reqId, key);
            }
        }
    } while (queries_num < queries_max_num);
}

int main(int argc,char *argv[])
{
    ci_cfg_lib_init();
    ci_mem_init();
    init_internal_lookup_tables();

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */

    if (!keys.indx) {
        keys.indx = calloc(MAX_LIST_SIZE, sizeof(char *));
        keys.max = MAX_LIST_SIZE;
        keys.num = 0;
    }

    if (!ci_args_apply(argc, argv, options) || !path || (!keys.indx && !keysfile)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    if (queries_max_num <= 0)
        queries_max_num = keys.num;

    if (keysfile)
        load_keys_from_file(&keys, keysfile);

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
    execute_commands(CI_CMD_CHILD_START);
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
    if (keys.indx) {
        int i;
        for (i = 0; i < keys.num; ++i)
            free(keys.indx[i]);
        free(keys.indx);
    }
    ci_lookup_table_destroy(table);
    ci_thread_mutex_destroy(&mtx);
    return 0;
}
