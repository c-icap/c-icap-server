#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include "cfg_param.h"
#include "ci_threads.h"
#include "mem.h"
#include "debug.h"


int run_allocs()
{
    int l, i, k;
    void *v;
    for (l = 1; l< 50; l++) {
        for (k = 1; k < 133; k++) {
            for( i = k; i < 32768; i = i*2) {
                ci_debug_printf(5, "Alloc buffer for %d bytes\n", i);
                v = ci_buffer_alloc(i);
                memset(v, 0x1, i);
                ci_buffer_free(v);
            }
        }
        
        for (k = 11; k < 73; k++) {
            ci_debug_printf(5, "Alloc buffer for realloc for %d bytes\n", k);
            v = ci_buffer_alloc(k);
            for( i = 17; i < 32768; i = i*2) {
                ci_debug_printf(5, "ReAlloc buffer for %d bytes\n", i);
                v = ci_buffer_realloc(v, i);
                memset(v, 0x1, i);
            }
            ci_buffer_free(v);
        }
    }
    return 1;
}

int threadsnum = 100;

static struct ci_options_entry options[] = {
    {"-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
     "The debug level"},
    {"-t", "threads", &threadsnum, ci_cfg_set_int,
     "The numbers of threads to use"},
    {NULL,NULL,NULL,NULL,NULL}
};

int mem_init();
int main(int argc, char *argv[])
{
    int i;
    ci_thread_t *threads;
    CI_DEBUG_STDOUT = 1;
    
    ci_cfg_lib_init();
    mem_init();
    
    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    
   /* Simple one thread test */
   run_allocs();

   /* Run multithread test */
   threads = malloc(sizeof(ci_thread_t) * threadsnum);
   for (i = 0; i < threadsnum; i++)  threads[i] = 0;
   for (i = 0; i < threadsnum; i++) {
       ci_debug_printf(8, "Thread %d started\n", i);
       ci_thread_create(&(threads[i]),
                        (void *(*)(void *)) run_allocs,
                        (void *) NULL /*data*/);
   }

   for (i = 0; i < threadsnum; i++) {
       ci_thread_join(threads[i]);
       ci_debug_printf(6, "Thread %d exited\n", i);
   }
   
   return 0;
}



