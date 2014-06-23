#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "cache.h"
#include "dlib.h"
#include "mem.h"
#include "module.h"
#include "lookup_table.h"
#include "proc_mutex.h"
#include "ci_threads.h"
#include "debug.h"
#include "cfg_param.h"

int load_module(const char *directive,const char **argv,void *setdata)
{
    CI_DLIB_HANDLE lib;
    common_module_t *module;
    
    if(argv== NULL || argv[0]== NULL)
        return 0;

    lib = ci_module_load(argv[0],"./");
    
    if(!lib) {
        printf("Error opening module :%s\n",argv[0]);
        return 0;
    }

    module = ci_module_sym(lib, "module");

    if(!module) {
        printf("Error opening module %s: can not find symbol module\n",argv[0]);
        return 0;
    }

    if (module->init_module)
        module->init_module(NULL);
    if (module->post_init_module)
        module->post_init_module(NULL);

    return 1;
}

void log_errors(void *unused, const char *format, ...)
{                                                     
     va_list ap;                                      
     va_start(ap, format);                            
     vfprintf(stderr, format, ap);                    
     va_end(ap);                                      
}

char *CACHE_TYPE = NULL;
static struct ci_options_entry options[] = {
    {"-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
     "The debug level"},
    {"-m", "module", NULL, load_module,
     "The path of the table"},
    {"-c", "cache", &CACHE_TYPE, ci_cfg_set_str,
     "The type of cache to use"},
    {NULL,NULL,NULL,NULL,NULL}
};

int mem_init();
int main(int argc,char *argv[]) {
    int i;
    struct ci_cache *cache;
    char *s;
    const char *str;
    size_t v_size;

    ci_cfg_lib_init();
    mem_init();

    __log_error = (void (*)(void *, const char *,...)) log_errors; /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }
    if (!CACHE_TYPE)
        CACHE_TYPE = "local";

    cache = ci_cache_build("test1", CACHE_TYPE,
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
	printf("Found : %s (correct is NULL!)\n", s);
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

    cache = ci_cache_build("test2", CACHE_TYPE,
                           65536, /*cache_size*/
			   2048, /*max_object_size*/ 
			   0, /*ttl*/
			   &ci_str_ops /*key_ops*/
	);
    ci_str_vector_t *vect_str = ci_str_vector_create(4096);
    str = ci_str_vector_add(vect_str, "1_val1");
    printf("Found 1_val1: %s\n", str);
    str = ci_str_vector_add(vect_str, "1_val2");
    printf("Found 1_val2: %s\n", str);
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

    cache = ci_cache_build("test3", CACHE_TYPE,
                           65536, /*cache_size*/
			   2048, /*max_object_size*/ 
			   0, /*ttl*/
			   NULL /*key_ops*/
	);

    ci_cache_update(cache, "nulkey1", NULL, 0, NULL);
    ci_cache_update(cache, "nulkey2", NULL, 0, NULL);
    if(ci_cache_search(cache,"nulkey1", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }
    if(ci_cache_search(cache,"nulkey2", (void **)&s, NULL, NULL)) {
	printf("Found : %s\n", s);
	ci_buffer_free(s);
    }

    ci_cache_destroy(cache);
    return 0;
}
