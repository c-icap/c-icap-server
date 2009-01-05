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
    struct ci_lookup_table *table;
    char *path,*key;
    void *e,*v,**vals;
    int i;

    CI_DEBUG_LEVEL = 10;
    ci_cfg_lib_init();
    init_internal_lookup_tables();

    if(argc<3) {
	printf("usage:\n\t%s path/to/table key\n",argv[0]);
	return -1;
    }
    path=argv[1];
    key=argv[2];

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */                                                    
    
    
    table = ci_lookup_table_create(path);
    if(!table) {
	printf("Error opening table\n");
	return -1;
    }
    table->open(table);

    e = table->search(table,key,&vals);
    if(e) {
	printf("Result :\n\t%s:",key);
	for(v=vals[0],i=0;v!=NULL;v=vals[i++]) {
	    printf("%s ",(char *)v);
	}
	printf("\n");
    }
    else {
	printf("Not found\n");
    }

    ci_lookup_table_destroy(table);
    return 0;
}
