#include "c-icap.h"
#include <errno.h>
#include "cfg_param.h"
#include "debug.h"




int setInt(char *directive,char **argv,void *setdata){
     int val=0;
     char *end;
     if(argv==NULL || argv[0]==NULL){
	  ci_debug_printf(1,"Missing arguments in directive:%s\n",directive);
	  return 0;
     }

     if(setdata==NULL)
	  return 0;
     errno=0;
     val=strtoll(argv[0],&end,10);

     if((val==0 && errno!=0))
           return 0;

     *((int*)setdata)=val;

     ci_debug_printf(1,"Setting parameter :%s=%d\n",directive,val);
     return 1;
}

/*
 I must create a table that holds the allocating memory blocks for
 paarmeters!!!!!!!
*/
int setStr(char *directive,char **argv,void *setdata){
     if(argv == NULL || argv[0] == NULL){
	  return 0;
     }
     if(setdata == NULL)
	  return 0;
     *((char **)setdata) = (char *)strdup(argv[0]);
     ci_debug_printf(1,"Setting parameter :%s=%s\n",directive,argv[0]);
     return 1;
}

int setDisable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=0;
     ci_debug_printf(1,"Disabling parameter %s\n",directive);
     return 1;

}

int setEnable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=1;
     ci_debug_printf(1,"Enabling parameter %s\n",directive);
     return 1;
}

