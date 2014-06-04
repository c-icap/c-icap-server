#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "cache.h"
#include "mem.h"
#include "lookup_table.h"
#include "proc_mutex.h"
#include "ci_threads.h"
#include "debug.h"
#include "cfg_param.h"

void log_errors(void *unused, const char *format, ...)
{                                                     
     va_list ap;                                      
     va_start(ap, format);                            
     vfprintf(stderr, format, ap);                    
     va_end(ap);                                      
}

int mem_init();
int main(int argc,char *argv[]) {
    int i;
    struct ci_cache *cache;
    char *s;
    const char *str;
    size_t v_size;

    CI_DEBUG_LEVEL = 10;
    ci_cfg_lib_init();
    mem_init();

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */                                                    
    
    cache = ci_cache_build("local_cache",
                           65536, /*cache_size*/
			   2048, /*max_object_size*/ 
			   0, /*ttl*/
			   &ci_str_ops /*key_ops*/
	);

    ci_cache_update(cache, "test1", "A test1 val", strlen("A test1 val") + 1, NULL);

    ci_cache_update(cache, "test2", "A test2 val", strlen("A test2 val") + 1, NULL);

    ci_cache_update(cache, "test3", "A test 3 val", strlen("A test 3 val") + 1, NULL);

    ci_cache_update(cache, "test4", "A test 4 val", strlen("A test 4 val") + 1, NULL);


    if(ci_cache_search(cache,"test2", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }

    if(ci_cache_search(cache,"test21", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }

    if(ci_cache_search(cache,"test1", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }

    if(ci_cache_search(cache,"test4", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }

    ci_cache_destroy(cache);

    cache = ci_cache_build("local_cache",
                           65536, /*cache_size*/
			   2048, /*max_object_size*/ 
			   0, /*ttl*/
			   &ci_str_ops /*key_ops*/
	);
    ci_str_vector_t *vect_str = ci_str_vector_create(4096);
    str = ci_str_vector_add(vect_str, "1_val1");
    str = ci_str_vector_add(vect_str, "1_val2");
    v_size = ci_cache_store_vector_size(vect_str);
    ci_cache_update(cache, "vect1", vect_str, v_size, &ci_cache_store_vector_val);
    ci_str_vector_destroy(vect_str);

    vect_str = ci_str_vector_create(4096);
    str = ci_str_vector_add(vect_str, "2_val1");
    str = ci_str_vector_add(vect_str, "2_val2");
    str = ci_str_vector_add(vect_str, "2_val3");
    v_size = ci_cache_store_vector_size(vect_str);
    ci_cache_update(cache, "vect2", vect_str, v_size, &ci_cache_store_vector_val);
    ci_str_vector_destroy(vect_str);

    if (ci_cache_search(cache, "vect1", (void **)&vect_str,  NULL, &ci_cache_read_vector_val)) {
        for (i=0; vect_str && vect_str->items[i] != NULL; i++)
            printf("Vector item %d:%s \n", i, (char *)vect_str->items[i]);
        ci_str_vector_destroy(vect_str);
    }

    if (ci_cache_search(cache, "vect2", (void **)&vect_str, NULL, &ci_cache_read_vector_val)) {
        for (i=0; vect_str && vect_str->items[i] != NULL; i++)
            printf("Vector item %d:%s \n", i, (char *)vect_str->items[i]);
        ci_str_vector_destroy(vect_str);
    }

    ci_cache_destroy(cache);
    return 0;
}
