#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "cache.h"
#include "mem.h"
#include "lookup_table.h"
#include "debug.h"
#include "cfg_param.h"

void log_errors(void *unused, const char *format, ...)
{                                                     
     va_list ap;                                      
     va_start(ap, format);                            
     vfprintf(stderr, format, ap);                    
     va_end(ap);                                      
}

void *copy_to_str(void *val, int *val_size, ci_mem_allocator_t *allocator)
{
    return (void *)ci_str_ops.dup((char *)val, allocator);
}

void *copy_from_str(void *val, int val_size, ci_mem_allocator_t *allocator)
{
    return (void *)ci_str_ops.dup((char *)val, allocator);
}

int mem_init();
int main(int argc,char *argv[]) {
    int i;
    struct ci_cache *cache;
    ci_mem_allocator_t *allocator;
    char *s;
    const char *str;
    printf("Hi re\n");

    CI_DEBUG_LEVEL = 10;
    ci_cfg_lib_init();
    mem_init();

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */                                                    
    
    allocator = ci_create_os_allocator();
    cache = ci_cache_build(65536, /*cache_size*/
                           512, /*max_key_size*/
			   1024, /*max_object_size*/ 
			   0, /*ttl*/
			   &ci_str_ops, /*key_ops*/
			   &copy_to_str, /*copy_to*/
			   &copy_from_str /*copy_from*/
	);

    ci_cache_update(cache, "test1", "A test1 val");

    ci_cache_update(cache, "test2", "A test2 val");

    ci_cache_update(cache, "test3", "A test 3 val");

    ci_cache_update(cache, "test4", "A test 4 val");


    if(ci_cache_search(cache,"test2", (void **)&s, allocator)) {
	printf("Found : %s\n", s);
	allocator->free(allocator,s);
    }

    if(ci_cache_search(cache,"test21", (void **)&s, allocator)) {
	printf("Found : %s\n", s);
	allocator->free(allocator, s);
    }

    if(ci_cache_search(cache,"test1", (void **)&s, allocator)) {
	printf("Found : %s\n", s);
	allocator->free(allocator,s);
    }

    if(ci_cache_search(cache,"test4", (void **)&s, allocator)) {
	printf("Found : %s\n", s);
	allocator->free(allocator,s);
    }

    ci_cache_destroy(cache);

    cache = ci_cache_build(65536, /*cache_size*/
                           512, /*max_key_size*/
			   1024, /*max_object_size*/ 
			   0, /*ttl*/
			   &ci_str_ops, /*key_ops*/
			   &ci_cache_store_vector_val, /*copy_to*/
			   &ci_cache_read_vector_val /*copy_from*/
	);
    ci_str_vector_t *vect_str = ci_str_vector_create(4096);
    str = ci_str_vector_add(vect_str, "1_val1");
    str = ci_str_vector_add(vect_str, "1_val2");
    ci_cache_update(cache, "vect1", vect_str);
    ci_str_vector_destroy(vect_str);

    vect_str = ci_str_vector_create(4096);
    str = ci_str_vector_add(vect_str, "2_val1");
    str = ci_str_vector_add(vect_str, "2_val2");
    str = ci_str_vector_add(vect_str, "2_val3");
    ci_cache_update(cache, "vect2", vect_str);
    ci_str_vector_destroy(vect_str);

    if (ci_cache_search(cache, "vect1", (void **)&vect_str,  NULL)) {
        for (i=0; vect_str && vect_str->items[i] != NULL; i++)
            printf("Vector item %d:%s \n", i, (char *)vect_str->items[i]);
        ci_str_vector_destroy(vect_str);
    }

    if (ci_cache_search(cache, "vect2", (void **)&vect_str, NULL)) {
        for (i=0; vect_str && vect_str->items[i] != NULL; i++)
            printf("Vector item %d:%s \n", i, (char *)vect_str->items[i]);
        ci_str_vector_destroy(vect_str);
    }

    ci_cache_destroy(cache);
    return 0;
}
