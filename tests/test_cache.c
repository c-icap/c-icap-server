#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "mem.h"
#include "lookup_table.h"
#include "cache.h"
#include "debug.h"

void log_errors(void *unused, const char *format, ...)
{                                                     
     va_list ap;                                      
     va_start(ap, format);                            
     vfprintf(stderr, format, ap);                    
     va_end(ap);                                      
}


int main(int argc,char *argv[]) {
    struct ci_cache_table *cache;
    ci_mem_allocator_t *allocator;
    char *s;
    printf("Hi re\n");

    CI_DEBUG_LEVEL = 10;
    ci_cfg_lib_init();
    
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */                                                    
    
    allocator = ci_create_os_allocator();
    cache = ci_cache_build(3, 10, 0, &ci_str_ops, NULL, allocator);

    s=ci_str_ops.dup("test1", allocator);
    ci_cache_update(cache, s, s);

    s=ci_str_ops.dup("test2", allocator);
    ci_cache_update(cache, s, s);

    s=ci_str_ops.dup("test3", allocator);
    ci_cache_update(cache, s, s);


    s=ci_str_ops.dup("test4", allocator);
    ci_cache_update(cache, s, s);


    s=ci_cache_search(cache,"test2");
    printf("Found : %s\n", s);

    s=ci_cache_search(cache,"test21");
    printf("Found : %s\n", s);

    s=ci_cache_search(cache,"test1");
    printf("Found : %s\n", s);

    s=ci_cache_search(cache,"test4");
    printf("Found : %s\n", s);

    return 0;
}
