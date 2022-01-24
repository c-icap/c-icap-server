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
int THREADS = 1;
int USE_DEBUG_LEVEL = -1;
int EMULATE_CRASHES = 0;
const char *SCHEME = "pthread";

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
        "The number of loops per thread (default is 10000)"
    },
    {
        "-p", "processes", &PROCS, ci_cfg_set_int,
        "The number of children to start (default is 50)"
    },
    {
        "-t", "threads", &THREADS, ci_cfg_set_int,
        "The number of threads per process to start (default is 1)"
    },
    {
        "-c", NULL, &EMULATE_CRASHES, ci_cfg_enable,
        "Emulate crashes on children processes"
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
struct stats *Stats = NULL;
int KidId = -1;
ci_thread_mutex_t mtx;
ci_thread_cond_t cnd;
int Started = 0;

#define CLOCK_TIME_DIFF_micro(tsstop, tsstart) ((tsstop.tv_sec - tsstart.tv_sec) * 1000000) + ((tsstop.tv_nsec - tsstart.tv_nsec) / 1000)

void thread()
{
    int i;
    struct timespec start, stop;
    if (THREADS > 1) {
        /*Wait to synchronize*/
        ci_thread_mutex_lock(&mtx);
        Started++;
        ci_thread_cond_wait(&cnd, &mtx);
        ci_thread_mutex_unlock(&mtx);
    }
    clock_gettime (CLOCK_REALTIME, &start);
    for(i = 0; i < LOOPS; ++i) {
        assert(ci_proc_mutex_lock(&stats_mutex));
        Stats->c += 1;
        if (EMULATE_CRASHES) {
            /*Some kids will be crashed leaving locked the mutex to
              check recovery after crash.
             */
            if ((KidId == (PROCS * 0.25) && i > (LOOPS * 0.25)) ||
                (KidId == (PROCS * 0.5) && i > (LOOPS * 0.5)) ||
                (KidId == (PROCS * 0.75) && i > (LOOPS *0.75))) {
                ci_debug_printf(1, "Crashing kid %d at loop step %d\n", KidId, i);
                assert(0);
            }

        }
        ci_proc_mutex_unlock(&stats_mutex);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    ci_thread_mutex_lock(&mtx);
    Stats->times[KidId] += CLOCK_TIME_DIFF_micro(stop, start);
    ci_thread_mutex_unlock(&mtx);
}

void run_child(int id) {
    KidId = id;
    Stats = ci_shared_mem_attach(&sid);
    Stats->times[KidId] = 0;
    ci_thread_mutex_init(&mtx);
    ci_thread_cond_init(&cnd);
    if (THREADS <= 1)
        thread();
    else {
        ci_thread_t *threads;
        int i;
        threads = malloc(sizeof(ci_thread_t) * THREADS);
        for (i = 0; i < THREADS; i++)  threads[i] = 0;
        for (i = 0; i < THREADS; i++) {
            ci_debug_printf(8, "Thread %d started\n", i);
            ci_thread_create(&(threads[i]),
                             (void *(*)(void *)) thread,
                             (void *) NULL /*data*/);
        }
        while(Started < THREADS) usleep(100);
        usleep(1000);
        ci_thread_cond_broadcast(&cnd);
        for (i = 0; i < THREADS; i++) {
            ci_thread_join(threads[i]);
            ci_debug_printf(6, "Thread %d exited\n", i);
        }
    }
    ci_debug_printf(2, "Loops took %d microsecs\n", Stats->times[id]);
    ci_thread_mutex_destroy(&mtx);
    ci_thread_cond_destroy(&cnd);
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
        if (!WIFEXITED(status)) {
            ci_debug_printf(1, "Child %d abnormal termination with status %d\nCheck mutex states\n", pid, status);
            ci_proc_mutex_recover_after_crash();
        } else {
            ci_debug_printf(4, "Child %d terminated with status %d\n", pid, status);
        }
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
           (stats->c ? allTime/stats->c : 0),
           (PROCS ? allTime/PROCS : 0),
           allTime
        );

    ci_shared_mem_destroy(&sid);
}
