#include "common.h"
#define CI_ATOMICS_INLINE
#include "atomic.h"
#include "cfg_param.h"
#include "ci_threads.h"
#include "debug.h"

ci_thread_cond_t CND;
ci_thread_mutex_t CNDMTX;
ci_thread_mutex_t statsMTX;
ci_thread_mutex_t counterMTX;
int waitingForTest2 = 0;
int waitingForTest3 = 0;
int waitingForTest4 = 0;

uint64_t simple_c = 0;
uint64_t at_c = 0;
uint64_t ni_at_c = 0;
uint64_t ni_at_c_gl = 0;
uint64_t simple_spent_time = 0;
uint64_t spent_time = 0;
uint64_t ni_spent_time = 0;
uint64_t ni_spent_time_gl = 0;

int LOOPS = 10000;
int THREADS = 100;
int USE_DEBUG_LEVEL = -1;

#define CLOCK_TIME_DIFF_micro(tsstop, tsstart) ((tsstop.tv_sec - tsstart.tv_sec) * 1000000) + ((tsstop.tv_nsec - tsstart.tv_nsec) / 1000)

int run_thread()
{
    struct timespec start, stop;
    uint64_t tt;

    ci_thread_mutex_lock(&CNDMTX);
    ci_thread_cond_wait(&CND, &CNDMTX);
    ci_thread_mutex_unlock(&CNDMTX);

    clock_gettime (CLOCK_REALTIME, &start);
    for(int i = 0; i < LOOPS; i++) {
        ci_atomic_add_u64(&at_c, 1);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    tt = CLOCK_TIME_DIFF_micro(stop, start);
    ci_thread_mutex_lock(&statsMTX);
    spent_time += tt;
    ci_thread_mutex_unlock(&statsMTX);

    ci_thread_mutex_lock(&CNDMTX);
    waitingForTest2++;
    ci_thread_cond_wait(&CND, &CNDMTX);
    ci_thread_mutex_unlock(&CNDMTX);

    clock_gettime (CLOCK_REALTIME, &start);
    for(int i = 0; i < LOOPS; i++) {
        ci_atomic_add_u64_non_inline(&ni_at_c, 1);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    tt = CLOCK_TIME_DIFF_micro(stop, start);
    ci_thread_mutex_lock(&statsMTX);
    ni_spent_time += tt;
    ci_thread_mutex_unlock(&statsMTX);

    ci_thread_mutex_lock(&CNDMTX);
    waitingForTest3++;
    ci_thread_cond_wait(&CND, &CNDMTX);
    ci_thread_mutex_unlock(&CNDMTX);

    clock_gettime (CLOCK_REALTIME, &start);
    for(int i = 0; i < LOOPS; i++) {
        ci_atomic_add_u64_non_inline_gl(&ni_at_c_gl, 1);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    tt = CLOCK_TIME_DIFF_micro(stop, start);
    ci_thread_mutex_lock(&statsMTX);
    ni_spent_time_gl += tt;
    ci_thread_mutex_unlock(&statsMTX);

    ci_thread_mutex_lock(&CNDMTX);
    waitingForTest4++;
    ci_thread_cond_wait(&CND, &CNDMTX);
    ci_thread_mutex_unlock(&CNDMTX);

    clock_gettime (CLOCK_REALTIME, &start);
    for(int i = 0; i < LOOPS; i++) {
        ci_thread_mutex_lock(&counterMTX);
        simple_c++;
        ci_thread_mutex_unlock(&counterMTX);
    }
    clock_gettime (CLOCK_REALTIME, &stop);
    tt = CLOCK_TIME_DIFF_micro(stop, start);
    ci_thread_mutex_lock(&statsMTX);
    simple_spent_time += tt;
    ci_thread_mutex_unlock(&statsMTX);

    return 1;
}

static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-t", "threads", &THREADS, ci_cfg_set_int,
        "The numbers of threads to use"
    },
    {
        "-l", "loops", &LOOPS, ci_cfg_set_int,
        "The loops per thread to execute"
    },
    {NULL,NULL,NULL,NULL,NULL}
};

int main(int argc, char *argv[])
{
    int i;
    ci_thread_t *threads;
    CI_DEBUG_STDOUT = 1;

    ci_cfg_lib_init();
    ci_mem_init();
    ci_atomics_init();

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    ci_thread_mutex_init(&statsMTX);
    ci_thread_mutex_init(&CNDMTX);
    ci_thread_mutex_init(&counterMTX);
    ci_thread_cond_init(&CND);

    threads = malloc(sizeof(ci_thread_t) * THREADS);
    for (i = 0; i < THREADS; i++)  threads[i] = 0;
    for (i = 0; i < THREADS; i++) {
        ci_debug_printf(8, "Thread %d started\n", i);
        ci_thread_create(&(threads[i]),
                         (void *(*)(void *)) run_thread,
                         (void *) NULL /*data*/);
    }

    ci_debug_printf(2, "Threads started, wait some time to be ready\n");
    sleep(1);
    ci_debug_printf(2, "kick threads\n");
    ci_thread_cond_broadcast(&CND);

    // Waiting for the first test to finish.
    while(waitingForTest2 < THREADS) {
        sleep(1);
    }
    ci_thread_cond_broadcast(&CND);

    // Waiting for the second test to finish.
    while(waitingForTest3 < THREADS) {
        sleep(1);
    }
    ci_thread_cond_broadcast(&CND);

    while(waitingForTest4 < THREADS) {
        sleep(1);
    }
    ci_thread_cond_broadcast(&CND);

    for (i = 0; i < THREADS; i++) {
        ci_thread_join(threads[i]);
        ci_debug_printf(6, "Thread %d exited\n", i);
    }

    free(threads);

    ci_debug_printf(1, "Atomic add result : %lu, expect %lu\n", at_c, (long unsigned)(LOOPS * THREADS));
    ci_debug_printf(1, "Time spent: %" PRIu64" microseconds\n", spent_time);
    ci_debug_printf(1, "Noninline atomic add result : %lu, expect %lu\n", ni_at_c, (long unsigned)(LOOPS * THREADS));
    ci_debug_printf(1, "Noninline time spent: %" PRIu64" microseconds\n", ni_spent_time);
    ci_debug_printf(1, "Noninline inter-process atomic add result : %lu, expect %lu\n", ni_at_c_gl, (long unsigned)(LOOPS * THREADS));
    ci_debug_printf(1, "Noninline inter-process time spent: %" PRIu64" microseconds\n", ni_spent_time_gl);
    ci_debug_printf(1, "Mutex add result : %lu, expect %lu\n", simple_c, (long unsigned)(LOOPS * THREADS));
    ci_debug_printf(1, "Mutex time spent: %" PRIu64" microseconds\n", simple_spent_time);
    ci_thread_mutex_destroy(&CNDMTX);
    ci_thread_cond_destroy(&CND);
    return 0;
}
