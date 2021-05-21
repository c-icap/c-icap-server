#define CI_ATOMICS_INLINE
#include "atomic.h"
#include "cfg_param.h"
#include "ci_threads.h"
#include "debug.h"

#include <unistd.h>


ci_thread_cond_t CND;
ci_thread_mutex_t CNDMTX;

uint64_t at_c = 0;

int run_thread()
{
    ci_thread_mutex_lock(&CNDMTX);
    ci_thread_cond_wait(&CND, &CNDMTX);
    ci_thread_mutex_unlock(&CNDMTX);
    for(int i = 0; i < 10000; i++)
        ci_atomic_add_u64(&at_c, 1);
    return 1;
}

int threadsnum = 100;
int USE_DEBUG_LEVEL = -1;
static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-t", "threads", &threadsnum, ci_cfg_set_int,
        "The numbers of threads to use"
    },
    {NULL,NULL,NULL,NULL,NULL}
};

extern "C" int mem_init();
int main(int argc, char *argv[])
{
    int i;
    ci_thread_t *threads;
    CI_DEBUG_STDOUT = 1;

    ci_cfg_lib_init();
    mem_init();
    ci_atomics_init();

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    ci_thread_mutex_init(&CNDMTX);
    ci_thread_cond_init(&CND);

    threads = new ci_thread_t[threadsnum];
    for (i = 0; i < threadsnum; i++)  threads[i] = 0;
    for (i = 0; i < threadsnum; i++) {
        ci_debug_printf(8, "Thread %d started\n", i);
        ci_thread_create(&(threads[i]),
                         (void *(*)(void *)) run_thread,
                         (void *) NULL /*data*/);
    }

    ci_debug_printf(2, "Threads started, wait some time to be ready\n");
    sleep(1);
    ci_debug_printf(2, "kick threads\n");
    ci_thread_cond_broadcast(&CND);

    for (i = 0; i < threadsnum; i++) {
        ci_thread_join(threads[i]);
        ci_debug_printf(6, "Thread %d exited\n", i);
    }

    delete threads;

    ci_debug_printf(1, "Atomic add result : %lu\n", at_c)

    ci_thread_mutex_destroy(&CNDMTX);
    ci_thread_cond_destroy(&CND);
    return 0;
}

