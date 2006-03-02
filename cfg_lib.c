#include "c-icap.h"
#include <errno.h>
#include "cfg_param.h"
#include "debug.h"


/* Command line options implementation, function and structures */

void ci_args_usage(char *progname,struct options_entry *options){
     int i;
     printf("Usage : \n");
     printf("%s",progname);
     for(i=0;options[i].name!=NULL;i++)
	  printf(" [%s %s]",options[i].name,(options[i].parameter==NULL?"":options[i].parameter));
     printf("\n\n");
     for(i=0;options[i].name!=NULL;i++)
	  printf("%s %s\t\t: %s\n",options[i].name,(options[i].parameter==NULL?"\t":options[i].parameter),
		 options[i].msg);

}


struct options_entry *search_options_table(char *directive,struct options_entry *options){
     int i;
     for(i=0;options[i].name!=NULL;i++){
	  if(0==strcmp(directive,options[i].name))
	       return &options[i];
     }
     return NULL;
}


int ci_args_apply(int argc, char **argv,struct options_entry *options){
     int i;
     struct options_entry *entry;
     for(i=1;i<argc;i++){
	  if((entry=search_options_table(argv[i],options))==NULL)
	       return 0;
	  if(entry->parameter){
	       if(++i>=argc)
		    return 0;
	       (*(entry->action))(entry->name,argv+i,entry->data);
	  }
	  else
	       (*(entry->action))(entry->name,NULL,entry->data);
     }
     return 1;
}


/*Various functions for setting parameters from command line or config file */

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
