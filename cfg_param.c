/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "c-icap.h"
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "service.h"
#include "debug.h"
#include "module.h"
#include "cfg_param.h"

#define LINESIZE 512
#define MAX_DIRECTIVE_SIZE 80
#define MAX_ARGS 50


int TIMEOUT=300;
int KEEPALIVE_TIMEOUT=15;
int MAX_SECS_TO_LINGER=5;
int START_CHILDS=5;
int MAX_CHILDS=10;
int START_SERVERS=30;
int MIN_FREE_SERVERS=30;
int MAX_FREE_SERVERS=60;
int MAX_REQUESTS_BEFORE_REALLOCATE_MEM=100;
int PORT=1344;
int BODY_MAX_MEM=131072;
int DAEMON_MODE=1;
int DEBUG_LEVEL=3;
int DEBUG_STDOUT=0;

char *TMPDIR="/var/tmp/";
char *PIDFILE="/var/run/c-icap.pid";
char *RUN_USER=NULL;
char *RUN_GROUP=NULL;
char *cfg_file=CONFFILE;
char *SERVICES_DIR=SERVDIR;
char *MODULES_DIR=MODSDIR;


extern char *SERVER_LOG_FILE;
extern char *ACCESS_LOG_FILE;
/*extern char *LOGS_DIR;*/

extern logger_module_t *default_logger;

int Load_Service(char *directive,char **argv,void *setdata);
int Load_Module(char *directive,char **argv,void *setdata);
int SetLogger(char *directive,char **argv,void *setdata);
int setTmpDir(char *directive,char **argv,void *setdata); 

struct sub_table{
     char *name;
     struct conf_entry *conf_table;
};

static struct conf_entry conf_variables[]={
     {"PidFile",&PIDFILE,setStr,NULL},
     {"Timeout",(void *)(&TIMEOUT),setInt,NULL},
     {"KeepAlive",NULL,NULL,NULL},
     {"MaxKeepAliveRequests",NULL,NULL,NULL},
     {"KeepAliveTimeout",&KEEPALIVE_TIMEOUT,setInt,NULL},
     {"StartServers",&START_CHILDS,setInt,NULL},
     {"MaxServers",&MAX_CHILDS,setInt,NULL},
     {"MinSpareThreads",&MIN_FREE_SERVERS,setInt,NULL},
     {"MaxSpareThreads",&MAX_FREE_SERVERS,setInt,NULL},
     {"ThreadsPerChild",&START_SERVERS,setInt,NULL},
     {"MaxRequestsPerChild",NULL,NULL,NULL},
     {"MaxRequestsReallocateMem",&MAX_REQUESTS_BEFORE_REALLOCATE_MEM,setInt,NULL},
     {"Port",&PORT,setInt,NULL},
     {"User",&RUN_USER,setStr,NULL},
     {"Group",&RUN_GROUP,setStr,NULL},
     {"ServerAdmin",NULL,NULL,NULL},
     {"ServerName",NULL,NULL,NULL},
     {"Logger",&default_logger,SetLogger,NULL},
     {"ServerLog",&SERVER_LOG_FILE,setStr,NULL},
     {"AccessLog",&ACCESS_LOG_FILE,setStr,NULL},
     {"DebugLevel",&DEBUG_LEVEL,setInt,NULL},
     {"ServicesDir",&SERVICES_DIR,setStr,NULL},
     {"ModulesDir",&MODULES_DIR,setStr,NULL},
     {"Service",NULL,Load_Service,NULL},
     {"Module",NULL,Load_Module,NULL},
     {"TmpDir",NULL,setTmpDir,NULL},
     {"Max_mem_object",&BODY_MAX_MEM,setInt,NULL},
     {NULL,NULL,NULL,NULL}
};

#define STEPSIZE 10
static struct sub_table *extra_conf_tables=NULL;
int conf_tables_list_size=0;
int conf_tables_num=0;

struct options_entry{
     char *name;
     char *parameter;
     void *data;
     int (*action)(char *name, char **argv,void *setdata);
     char *msg;
};


#define opt_pre "-" /*For windows will be "/" */

static struct options_entry options[]={
     {opt_pre"f","filename",&cfg_file,setStr,"Specify the configuration file"},
     {opt_pre"N",NULL,&DAEMON_MODE,setDisable,"Do not run as daemon"},
     {opt_pre"d","level",&DEBUG_LEVEL,setInt,"Specify the debug level"},
     {opt_pre"D",NULL,&DEBUG_STDOUT,setEnable,"Print debug info to stdout"},
     {NULL,NULL,NULL,NULL}
};

struct options_entry *search_options_table(char *directive){
     int i;
     for(i=0;options[i].name!=NULL;i++){
	  if(0==strcmp(directive,options[i].name))
	       return &options[i];
     }
     return NULL;
}



struct conf_entry *search_conf_table(struct conf_entry *table,char *varname){
     int i;
     for(i=0;table[i].name!=NULL;i++){
	  if(0==strcmp(varname,table[i].name))
	       return &table[i];
     }
     return NULL;
}

void init_conf_tables(){
     if((extra_conf_tables=malloc(STEPSIZE*sizeof(struct sub_table)))==NULL){
	  debug_printf(1,"Error allocating memory...\n");
	  return;
     }
     conf_tables_list_size=STEPSIZE;
}

int register_conf_table(char *name,struct conf_entry *table){
     struct sub_table *new;
     int newsize;
     if(!extra_conf_tables)
	  return 0;
     if(conf_tables_num==conf_tables_list_size){/*tables list is full reallocating space ......*/
	  if(NULL==(new=realloc(extra_conf_tables,conf_tables_list_size+STEPSIZE)))
	       return 0;
	  extra_conf_tables=new;
	  conf_tables_list_size+=STEPSIZE;
     }
     debug_printf(10,"Registering conf table:%s\n",name);
     extra_conf_tables[conf_tables_num].name=name; /*It works. Points to the modules.name. (????)*/
     extra_conf_tables[conf_tables_num].conf_table=table;
     conf_tables_num++;
     return 1;
}

struct conf_entry *search_variables(char *table,char *varname){
     int i;
     if(table==NULL)
	  return search_conf_table(conf_variables,varname);

     debug_printf(1,"Going to search variable %s in table %s\n",varname,table);

     if(!extra_conf_tables) /*Not really needed........*/
	  return NULL;

     for(i=0;i<conf_tables_num;i++){
	  if(strcmp(table,extra_conf_tables[i].name)==0){
	       return search_conf_table(extra_conf_tables[i].conf_table,varname);
	  }
     }
     return NULL;
}
 
/************************************************************************/
/*  Set variables functions                                             */

int Load_Service(char *directive,char **argv,void *setdata){
     if(argv==NULL || argv[0]==NULL || argv[1]==NULL){
	  debug_printf(1,"Missing arguments in LoadService directive\n");
	  return 0;
     }
     debug_printf(1,"Loading service :%s path %s\n",argv[0],argv[1]);

     if (!register_service(argv[1])){
	  debug_printf(1,"Error loading service\n");
	  return 0;
     } 
     return 1;
}

int Load_Module(char *directive,char **argv,void *setdata){
     if(argv==NULL || argv[0]==NULL || argv[1]==NULL){
	  debug_printf(1,"Missing arguments in LoadModule directive\n");
	  return 0;
     }
     debug_printf(1,"Loading service :%s path %s\n",argv[0],argv[1]);

     if (!register_module(argv[1],argv[0])){
	  debug_printf(1,"Error loading service\n");
	  return 0;
     }
     return 1;
}


int setInt(char *directive,char **argv,void *setdata){
     int val=0;
     char *end;
     if(argv==NULL || argv[0]==NULL){
	  debug_printf(1,"Missing arguments in directive:%s\n",directive);
	  return 0;
     }

     if(setdata==NULL)
	  return 0;
     val=strtoll(argv[0],&end,10);
     if(val>0)
	  *((int*)setdata)=val;
     debug_printf(1,"Setting parameter :%s=%d\n",directive,val);
     return val;
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
     debug_printf(1,"Setting parameter :%s=%s\n",directive,argv[0]);
     return 1;
}

int setDisable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=0;
     debug_printf(1,"Disabling parameter %s\n",directive);
     return 1;

}

int setEnable(char *directive,char **argv,void *setdata){
     if(setdata==NULL)
	  return 0;

     *((int*)setdata)=1;
     debug_printf(1,"Enabling parameter %s\n",directive);
     return 1;
}




int SetLogger(char *directive,char **argv,void *setdata){
     logger_module_t *logger;
     if(argv==NULL || argv[0]==NULL){
	  debug_printf(1,"Missing arguments in directive\n");
	  return 0;
     }

     if(!(logger=find_logger(argv[0])))
	  return 0;
     default_logger=logger;
     debug_printf(1,"Setting parameter :%s=%s\n",directive,argv[0]);
     return 1;
}

int setTmpDir(char *directive,char **argv,void *setdata){
     int len;
     if(argv == NULL || argv[0] == NULL){
	  return 0;
     }

     len=strlen(argv[0]);

     TMPDIR =malloc((len+2)*sizeof(char)); 
     strcpy(TMPDIR,argv[0]);
#ifdef _WIN32
     if(TMPDIR[len]!='\\'){
	  TMPDIR[len]='\\';
	  TMPDIR[len+1]='\0';
     }
#else
     if(TMPDIR[len]!='/'){
	  TMPDIR[len]='/';
	  TMPDIR[len+1]='\0';
     }
#endif
   /*Check if tmpdir exists. If no try to build it , report an error and uses the default...*/
     debug_printf(1,"Setting parameter :%s=%s\n",directive,argv[0]);
     return 1;
}


/**************************************************************************/
/* Parse file functions                                                   */

int fread_line(FILE *f_conf, char *line){
     if(!fgets(line,LINESIZE,f_conf))
	  return 0;
     if(strlen(line)>=LINESIZE-2 && line[LINESIZE-2]!='\n'){ //Size of line > LINESIZE
	  while(!feof(f_conf)){
	       if(fgetc(f_conf)=='\n')
		    return 1;
	  }
	  return 0;
     }
     return 1;
}


struct conf_entry *find_action(char *str, char **arg){
     char *end,*table,*s;
     int i=0,size;
     end=str;
     while(*end!='\0' && !isspace(*end))
	  end++;
     size=end-str;
     *end='\0';/*Mark the end of Variable......*/
     end++; /*... and continue....*/
     while(*end!='\0' && isspace(*end)) /*Find the start of arguments ......*/
	  end++;    
     *arg=end;
     if(s=strchr(str,'.')){
	  table=str;
	  str=s+1;
	  *s='\0';
     }
     else
	  table=NULL;
     
//     return search_conf_table(conf_variables,str);
     return search_variables(table,str);
}

char **split_args(char *args){
     int len,i=0,k;
     char **argv=NULL,*str,*end;
     argv=malloc((MAX_ARGS+1)*sizeof(char*));
     end=args;
     do{
	  str=end;
	  if(*end=='"'){
	       end++;
	       str=end;
	       while(*end!='\0' && *end!='"') 
		    end++;
	  }
	  else
	  {
	       while(*end!='\0' && !isspace(*end)) 
		    end++;
	  }
	  len=end-str;

	  argv[i]=malloc((len+1)*sizeof(char));
	  
	  memcpy(argv[i],str,len);/*copy until len or end of string*/
	  argv[i][len]='\0';
	  ++i;

	  if(i>=MAX_ARGS)
	       break;

	  if(*end=='"')
	       end++;
	  while(*end!='\0' && isspace(*end))
	       end++;

     }while(*end!='\0');
     argv[i]=NULL;

     return argv;
}

void free_args(char **argv){
     int i;
     if(argv==NULL)
	  return;
     for(i=0;argv[i]!=NULL;i++){
	  free(argv[i]);
	  argv[i]=NULL;
     }
     free(argv);
}

int process_line(char *line){
     int i=0;
     char *str,*args,**argv=NULL;
     struct conf_entry *entry;

     str=line;
     while(*str!='\0' && isspace(*str)) /*Eat the spaces in the begging*/
	  str++;
     if(*str=='\0' || *str=='#') /*Empty line or comment*/
	  return 0;
     
     entry=find_action(str,&args);
//     debug_printf(10,"Line %s (Args:%s)\n",entry->name,args);
     
     if(entry && entry->action){
	  argv=split_args(args);
	  (*(entry->action))(entry->name,argv,entry->data);
	  free_args(argv);
	  return 1;/*OK*/
     }
     //Else parse error.......
     //Log an error.....
     return 0;
}


int parse_file(char *conf_file){
     FILE *f_conf;
     char line[LINESIZE];


     if((f_conf=fopen(conf_file,"r"))==NULL){
	  //or log_server better........
	  debug_printf(1,"Can not open configuration file\n");
	  return 0;
     }
     
     while(!feof(f_conf)){
	  fread_line(f_conf,line);
	  process_line(line);
     }

     fclose(f_conf);
     return 1;
}


int check_opts(int argc, char **argv){
     int i;
     struct options_entry *entry;
     for(i=1;i<argc;i++){
	  if((entry=search_options_table(argv[i]))==NULL)
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

void usage(char *progname){
     int i;
     printf("Usage : ");
     printf("%s",progname);
     for(i=0;options[i].name!=NULL;i++)
	  printf(" [%s %s]",options[i].name,(options[i].parameter==NULL?"":options[i].parameter));
     printf("\n\n");
     for(i=0;options[i].name!=NULL;i++)
	  printf("%s %s\t\t: %s\n",options[i].name,(options[i].parameter==NULL?"\t":options[i].parameter),
		 options[i].msg);

}

int config(int argc, char **argv){

     
     if(!check_opts(argc,argv)){
	  debug_printf(1,"Error in command line options");
	  usage(argv[0]);
	  exit(-1);
     }
	  

     if(!parse_file(cfg_file)){
	  debug_printf(1,"Error opening/parsing config file");
	  exit(0);
     }
/*     parse_file("c-icap.conf");*/
     return 1;
}


