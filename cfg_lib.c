#include "c-icap.h"
#include <errno.h>
#include "cfg_param.h"
#include "debug.h"




int ci_cfg_set_int(char *directive,char **argv,void *setdata){
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
 I must create a table that holds the allocated memory blocks for
 paarmeters!!!!!!!
*/
int ci_cfg_set_str(char *directive,char **argv,void *setdata){
     if(argv == NULL || argv[0] == NULL){
	  return 0;
     }
     if(setdata == NULL)
	  return 0;
     *((char **)setdata) = (char *)strdup(argv[0]);
     ci_debug_printf(1,"Setting parameter :%s=%s\n",directive,argv[0]);
     return 1;
}

int ci_cfg_disable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=0;
     ci_debug_printf(1,"Disabling parameter %s\n",directive);
     return 1;

}

int ci_cfg_enable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=1;
     ci_debug_printf(1,"Enabling parameter %s\n",directive);
     return 1;
}

int ci_cfg_size_off(char *directive,char **argv,void *setdata){
     ci_off_t val=0;
     char *end;
     if(argv==NULL || argv[0]==NULL){
          ci_debug_printf(1,"Missing arguments in directive:%s\n",directive);
          return 0;
     }
     
     if(setdata==NULL)
          return 0;
     errno=0;
     val=ci_strto_off_t(argv[0],&end,10);

     if((val==0 && errno!=0) || val <0)
	  return 0;

     if(*end=='k' || *end=='K')
	  val=val*1024;
     else if(*end=='m' || *end=='M')
	  val=val*1024*1024;


     if(val>0)
          *((ci_off_t*)setdata)=val;
     ci_debug_printf(1,"Setting parameter :%s=%"PRINTF_OFF_T"\n",directive,val);
     return val;
}


int ci_cfg_size_long(char *directive,char **argv,void *setdata){
     long int val=0;
     char *end;
     if(argv==NULL || argv[0]==NULL){
          ci_debug_printf(1,"Missing arguments in directive:%s\n",directive);
          return 0;
     }
     
     if(setdata==NULL)
          return 0;
     errno=0;
     val=strtol(argv[0],&end,10);

     if((val==0 && errno!=0) || val <0)
	  return 0;

     if(*end=='k' || *end=='K')
	  val=val*1024;
     else if(*end=='m' || *end=='M')
	  val=val*1024*1024;


     if(val>0)
          *((long int*)setdata)=val;
     ci_debug_printf(1,"Setting parameter :%s=%"PRINTF_OFF_T"\n",directive,val);
     return val;
}
