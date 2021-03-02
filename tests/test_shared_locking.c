#include "common.h"
#include "cfg_param.h"
#include "ci_threads.h"
#include "mem.h"
#include "debug.h"
#include "client.h"
#include "proc_mutex.h"
#include "shared_mem.h"

#include <sys/wait.h>

int LOOPS = 10000;
int PROCS = 50;
int USE_DEBUG_LEVEL = -1;
const char *SCHEME = "posix";

static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-s", "locking_scheme", &SCHEME, ci_cfg_set_str,
        "posix|sysv|file"
    },
    {
        "-l", "loops", &LOOPS, ci_cfg_set_int,
        "The number of loops per kid"
    },
    {
        "-p", "processes", &PROCS, ci_cfg_set_int,
        "The number of children to start"
    },
    {NULL,NULL,NULL,NULL,NULL}

};

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

ci_proc_mutex_t stats_mutex;
ci_shared_mem_id_t sid;
struct stats {
    uint64_t c;
    int times[];
};

#define CLOCK_TIME_DIFF_micro(tsstop, tsstart) ((tsstop.tv_sec - tsstart.tv_sec) * 1000000) + ((tsstop.tv_nsec - tsstart.tv_nsec) / 1000)

void run_child(int id) {
    struct stats *stats = ci_shared_mem_attach(&sid);
    int i;
    struct timespec start, stop;
    clock_gettime (CLOCK_REALTIME, &start);
    for(i = 0; i < LOOPS; ++i) {
        ci_proc_mutex_lock(&stats_mutex);
        stats->c += 1;
        ci_proc_mutex_unlock(&stats_mutex);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    stats->times[id] = CLOCK_TIME_DIFF_micro(stop, start);
    ci_debug_printf(2, "Loops took %d microsecs\n", stats->times[id]);
}

int main(int argc, char *argv[])
{
    ci_client_library_init();

    __log_error = (void (*)(void *, const char *, ...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    if (!ci_proc_mutex_set_scheme(SCHEME)) {
        ci_debug_printf(1, "Wrong locking scheme: %s\n", SCHEME);
        exit(-1);
    }

    void *mem = ci_shared_mem_create(&sid, "test_shared_locking", sizeof(struct stats) + PROCS * sizeof(int));
    if (!mem) {
        ci_debug_printf(1, "Can not create shared memory\n");
        exit(-1);
    }
    ci_proc_mutex_init(&stats_mutex, "stats");
    int i;
    for(i = 0; i < PROCS; i++) {
        if (fork() == 0) {
            run_child(i);
            exit(0);
        }
    }
    for(i = 0; i < PROCS; i++) {
        int pid, status;
        pid = wait(&status);
        ci_debug_printf(4, "Child %d terminated with status %d\n", pid, status);
    }

    ci_proc_mutex_destroy(&stats_mutex);
    struct stats *stats = mem;
    uint64_t allTime = 0;
    for (i = 0; i < PROCS; ++i)
        allTime += stats->times[i];
    printf("Scheme: %s\n"
           "Loops: %"PRIu64"\n"
           "PROCESSES: %d\n"
           "Mean time (microsecs): %"PRIu64"\n"
           "Processes mean time (microsecs): %"PRIu64"\n"
           "Sum time (microsecs): %"PRIu64"\n",
           SCHEME,
           stats->c,
           PROCS,
           allTime/stats->c,
           allTime/PROCS,
           allTime);

    ci_shared_mem_destroy(&sid);
}
