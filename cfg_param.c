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
#include <errno.h>
#include <assert.h>
#include "service.h"
#include "debug.h"
#include "module.h"
#include "filetype.h"
#include "cfg_param.h"

#define LINESIZE 512
#define MAX_DIRECTIVE_SIZE 80
#define MAX_ARGS 50
int ARGC;
char **ARGV;


struct icap_server_conf CONF = {
     1344, /*PORT*/ AF_INET,    /*SOCK_FAMILY */
#ifdef _WIN32
     "c:\\TEMP", /*TMPDIR*/ "c:\\TEMP\\c-icap.pid", /*PIDFILE*/ "\\\\.\\pipe\\c-icap",  /*COMMANDS_SOCKET; */
#else
     "/var/tmp/", /*TMPDIR*/ "/var/run/c-icap.pid", /*PIDFILE*/ "/var/run/c-icap/c-icap.ctl",   /*COMMANDS_SOCKET; */
#endif
     NULL,                      /* RUN_USER */
     NULL,                      /* RUN_GROUP */
#ifdef _WIN32
     CONFDIR "\\c-icap.conf",   /*cfg_file */
     CONFDIR "\\c-icap.magic",  /*magics_file */
#else
     CONFDIR "/c-icap.conf",    /*cfg_file */
     CONFDIR "/c-icap.magic",   /*magics_file */
#endif
     NULL,                      /*MAGIC_DB */
     SERVDIR,                   /*SERVICES_DIR */
     MODSDIR,                   /*MODULES_DIR */
};


int TIMEOUT = 300;
int KEEPALIVE_TIMEOUT = 15;
int MAX_KEEPALIVE_REQUESTS = 100;
int MAX_SECS_TO_LINGER = 5;
int START_CHILDS = 5;
int MAX_CHILDS = 10;
int START_SERVERS = 30;
int MIN_FREE_SERVERS = 30;
int MAX_FREE_SERVERS = 60;
int MAX_REQUESTS_BEFORE_REALLOCATE_MEM = 100;
int MAX_REQUESTS_PER_CHILD = 0;
int DAEMON_MODE = 1;

extern char *SERVER_LOG_FILE;
extern char *ACCESS_LOG_FILE;
/*extern char *LOGS_DIR;*/

extern logger_module_t *default_logger;
extern access_control_module_t **used_access_controllers;

/*Functions declaration */
int parse_file(char *conf_file);

/*config table functions*/
int cfg_load_magicfile(char *directive, char **argv, void *setdata);
int cfg_load_service(char *directive, char **argv, void *setdata);
int cfg_service_alias(char *directive, char **argv, void *setdata);
int cfg_load_module(char *directive, char **argv, void *setdata);
int cfg_set_logger(char *directive, char **argv, void *setdata);
int cfg_set_debug_level(char *directive, char **argv, void *setdata);
int cfg_set_debug_stdout(char *directive, char **argv, void *setdata);
int cfg_set_body_maxmem(char *directive, char **argv, void *setdata);
int cfg_set_tmp_dir(char *directive, char **argv, void *setdata);
int cfg_set_acl_controllers(char *directive, char **argv, void *setdata);
int cfg_set_auth_method(char *directive, char **argv, void *setdata);
int cfg_include_config_file(char *directive, char **argv, void *setdata);

/*The following 2 functions defined in access.c file*/
int cfg_acl_add(char *directive, char **argv, void *setdata);
int cfg_acl_access(char *directive, char **argv, void *setdata);
/****/

struct sub_table {
     char *name;
     int type;
     struct conf_entry *conf_table;
};

static struct conf_entry conf_variables[] = {
     {"PidFile", &CONF.PIDFILE, intl_cfg_set_str, NULL},
     {"CommandsSocket", &CONF.COMMANDS_SOCKET, intl_cfg_set_str, NULL},
     {"Timeout", (void *) (&TIMEOUT), intl_cfg_set_int, NULL},
     {"KeepAlive", NULL, NULL, NULL},
     {"MaxKeepAliveRequests", &MAX_KEEPALIVE_REQUESTS, intl_cfg_set_int, NULL},
     {"KeepAliveTimeout", &KEEPALIVE_TIMEOUT, intl_cfg_set_int, NULL},
     {"StartServers", &START_CHILDS, intl_cfg_set_int, NULL},
     {"MaxServers", &MAX_CHILDS, intl_cfg_set_int, NULL},
     {"MinSpareThreads", &MIN_FREE_SERVERS, intl_cfg_set_int, NULL},
     {"MaxSpareThreads", &MAX_FREE_SERVERS, intl_cfg_set_int, NULL},
     {"ThreadsPerChild", &START_SERVERS, intl_cfg_set_int, NULL},
     {"MaxRequestsPerChild", &MAX_REQUESTS_PER_CHILD, intl_cfg_set_int, NULL},
     {"MaxRequestsReallocateMem", &MAX_REQUESTS_BEFORE_REALLOCATE_MEM,
      intl_cfg_set_int, NULL},
     {"Port", &CONF.PORT, intl_cfg_set_int, NULL},
     {"User", &CONF.RUN_USER, intl_cfg_set_str, NULL},
     {"Group", &CONF.RUN_GROUP, intl_cfg_set_str, NULL},
     {"ServerAdmin", NULL, NULL, NULL},
     {"ServerName", NULL, NULL, NULL},
     {"LoadMagicFile", NULL, cfg_load_magicfile, NULL},
     {"Logger", &default_logger, cfg_set_logger, NULL},
     {"ServerLog", &SERVER_LOG_FILE, intl_cfg_set_str, NULL},
     {"AccessLog", &ACCESS_LOG_FILE, intl_cfg_set_str, NULL},
     {"DebugLevel", NULL, cfg_set_debug_level, NULL},   /*Set library's debug level */
     {"ServicesDir", &CONF.SERVICES_DIR, intl_cfg_set_str, NULL},
     {"ModulesDir", &CONF.MODULES_DIR, intl_cfg_set_str, NULL},
     {"Service", NULL, cfg_load_service, NULL},
     {"ServiceAlias", NULL, cfg_service_alias, NULL},
     {"Module", NULL, cfg_load_module, NULL},
     {"TmpDir", NULL, cfg_set_tmp_dir, NULL},
     {"MaxMemObject", NULL, cfg_set_body_maxmem, NULL}, /*Set library's body max mem */
     {"AclControllers", NULL, cfg_set_acl_controllers, NULL},
     {"acl", NULL, cfg_acl_add, NULL},
     {"icap_access", NULL, cfg_acl_access, NULL},
     {"AuthMethod", NULL, cfg_set_auth_method, NULL},
     {"Include", NULL, cfg_include_config_file, NULL},
     {NULL, NULL, NULL, NULL}
};

#define STEPSIZE 10
static struct sub_table *extra_conf_tables = NULL;
int conf_tables_list_size = 0;
int conf_tables_num = 0;


struct conf_entry *search_conf_table(struct conf_entry *table, char *varname)
{
     int i;
     for (i = 0; table[i].name != NULL; i++) {
          if (0 == strcmp(varname, table[i].name))
               return &table[i];
     }
     return NULL;
}

void init_conf_tables()
{
     if ((extra_conf_tables =
          malloc(STEPSIZE * sizeof(struct sub_table))) == NULL) {
          ci_debug_printf(1, "Error allocating memory...\n");
          return;
     }
     conf_tables_list_size = STEPSIZE;
}

void reset_conf_tables()
{
     conf_tables_num = 0;
}

int register_conf_table(char *name, struct conf_entry *table, int type)
{
     struct sub_table *new;
     if (!extra_conf_tables)
          return 0;
     if (conf_tables_num == conf_tables_list_size) {    /*tables list is full, reallocating space ...... */
          if (NULL ==
              (new =
               realloc(extra_conf_tables, conf_tables_list_size + STEPSIZE)))
               return 0;
          extra_conf_tables = new;
          conf_tables_list_size += STEPSIZE;
     }
     ci_debug_printf(10, "Registering conf table:%s\n", name);
     extra_conf_tables[conf_tables_num].name = name;    /*It works. Points to the modules.name. (????) */
     extra_conf_tables[conf_tables_num].type = type;
     extra_conf_tables[conf_tables_num].conf_table = table;
     conf_tables_num++;
     return 1;
}

struct conf_entry *search_variables(char *table, char *varname)
{
     int i;
     if (table == NULL)
          return search_conf_table(conf_variables, varname);

     ci_debug_printf(1, "Going to search variable %s in table %s\n", varname,
                     table);

     if (!extra_conf_tables)    /*Not really needed........ */
          return NULL;

     for (i = 0; i < conf_tables_num; i++) {
          if (strcmp(table, extra_conf_tables[i].name) == 0) {
               return search_conf_table(extra_conf_tables[i].conf_table,
                                        varname);
          }
     }
     ci_debug_printf(1, "Variable %s or table %s not found!\n", varname, table);
     return NULL;
}

void print_conf_variables(struct conf_entry *table)
{
     int i;
     for (i = 0; table[i].name != NULL; i++) {
          ci_debug_printf(9, "%s=", table[i].name);
          if (!table[i].data) {
               ci_debug_printf(9, "\n");
          }
          else if (table[i].action == intl_cfg_set_str) {
               if (*(char *) table[i].data) {
                    ci_debug_printf(9, "%s\n", *(char **) table[i].data)
               }
               else {
                    ci_debug_printf(9, "\n");
               }
          }
          else if (table[i].action == intl_cfg_set_int) {
               ci_debug_printf(9, "%d\n", *(int *) table[i].data);
          }
          else if (table[i].action == intl_cfg_size_off) {
               ci_debug_printf(9, "%" PRINTF_OFF_T "\n",
                               *(ci_off_t *) table[i].data);
          }
          else if (table[i].action == intl_cfg_size_long) {
               ci_debug_printf(9, "%ld\n", *(long *) table[i].data);
          }
          else if (table[i].action == intl_cfg_onoff) {
               ci_debug_printf(9, "%d\n", *(int *) table[i].data);
          }
          else if (table[i].action == intl_cfg_enable) {
               ci_debug_printf(9, "%d\n", *(int *) table[i].data);
          }
          else if (table[i].action == intl_cfg_disable) {
               ci_debug_printf(9, "%d\n", *(int *) table[i].data);
          }
          else if (table[i].data) {
               ci_debug_printf(9, "%p\n", table[i].data);
          }
     }
}

int print_variables()
{
     int i;
     ci_debug_printf(9, "\n\nPrinting variables\n");
     print_conf_variables(conf_variables);
     if (!extra_conf_tables)    /*Not really needed........ */
          return 1;

     for (i = 0; i < conf_tables_num; i++) {
          ci_debug_printf(9, "Printing variables in table %s\n",
                          extra_conf_tables[i].name);
          print_conf_variables(extra_conf_tables[i].conf_table);
     }
     return 1;
}


/************************************************************************/
/*  Set variables functions                                             */
/* 
   The following tree functions refered to non constant variables so
   the compilers in Win32 have problem to appeared in static arrays
*/
int cfg_set_debug_level(char *directive, char **argv, void *setdata)
{
     return intl_cfg_set_int(directive, argv, &CI_DEBUG_LEVEL);
}

int cfg_set_debug_stdout(char *directive, char **argv, void *setdata)
{
     return intl_cfg_enable(directive, argv, &CI_DEBUG_STDOUT);
}

int cfg_set_body_maxmem(char *directive, char **argv, void *setdata)
{
     return intl_cfg_size_long(directive, argv, &CI_BODY_MAX_MEM);
}

int cfg_load_service(char *directive, char **argv, void *setdata)
{
     service_module_t *service = NULL;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in LoadService directive\n");
          return 0;
     }
     ci_debug_printf(1, "Loading service :%s path %s\n", argv[0], argv[1]);

     if (!(service = register_service(argv[1]))) {
          ci_debug_printf(1, "Error loading service\n");
          return 0;
     }
     add_service_alias(argv[0], service->mod_name, NULL);
     return 1;
}

int cfg_service_alias(char *directive, char **argv, void *setdata)
{
     char *service_args = NULL;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in ServiceAlias directive\n");
          return 0;
     }
     if ((service_args = strchr(argv[1], '?'))) {
          *service_args = '\0';
          service_args++;
     }

     ci_debug_printf(1, "Alias:%s of service %s\n", argv[0], argv[1]);
     add_service_alias(argv[0], argv[1], service_args);
     return 1;
}

int cfg_load_module(char *directive, char **argv, void *setdata)
{
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in LoadModule directive\n");
          return 0;
     }
     ci_debug_printf(1, "Loading service :%s path %s\n", argv[0], argv[1]);

     if (!register_module(argv[1], argv[0])) {
          ci_debug_printf(1, "Error loading service\n");
          return 0;
     }
     return 1;
}

int cfg_load_magicfile(char *directive, char **argv, void *setdata)
{
     char *db_file;
     if (argv == NULL || argv[0] == NULL) {
          return 0;
     }

     db_file = argv[0];
     ci_debug_printf(1, "Going to load magic file %s\n", db_file);
     if (!ci_magics_db_file_add(CONF.MAGIC_DB, db_file)) {
          ci_debug_printf(1, "Can not load magic file %s!!!\n", db_file);
          return 0;
     }

     return 1;
}

extern logger_module_t file_logger;
int cfg_set_logger(char *directive, char **argv, void *setdata)
{
     logger_module_t *logger;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive\n");
          return 0;
     }

     if (!(logger = find_logger(argv[0])))
          return 0;
     default_logger = logger;
     ci_debug_printf(1, "Setting parameter :%s=%s\n", directive, argv[0]);
     return 1;
}

int cfg_set_tmp_dir(char *directive, char **argv, void *setdata)
{
     int len;
     if (argv == NULL || argv[0] == NULL) {
          return 0;
     }

     cfg_default_value_store(&CONF.TMPDIR, &CONF.TMPDIR, sizeof(char *));
     len = strlen(argv[0]);

     CONF.TMPDIR = ci_cfg_alloc_mem((len + 2) * sizeof(char));
     strcpy(CONF.TMPDIR, argv[0]);
#ifdef _WIN32
     if (CONF.TMPDIR[len] != '\\') {
          CONF.TMPDIR[len] = '\\';
          CONF.TMPDIR[len + 1] = '\0';
     }
#else
     if (CONF.TMPDIR[len] != '/') {
          CONF.TMPDIR[len] = '/';
          CONF.TMPDIR[len + 1] = '\0';
     }
#endif
     /*Check if tmpdir exists. If no try to build it , report an error and uses the default... */
     CI_TMPDIR = CONF.TMPDIR;   /*Sets the library's temporary dir to .... */
     ci_debug_printf(1, "Setting parameter :%s=%s\n", directive, argv[0]);
     return 1;
}

int cfg_set_acl_controllers(char *directive, char **argv, void *setdata)
{
     int i, k, argc, ret;
     access_control_module_t *acl_mod;
     if (argv == NULL || argv[0] == NULL) {
          return 0;
     }

     if (strncasecmp(argv[0], "none", 4) == 0) {
          used_access_controllers = NULL;
          return 1;
     }

     for (argc = 0; argv[argc] != NULL; argc++);        /*Find the number of acl controllers */
     used_access_controllers =
         ci_cfg_alloc_mem(argc * sizeof(access_control_module_t *) + 1);
     k = 0;
     ret = 1;
     for (i = 0; i < argc; i++) {
          if ((acl_mod = find_access_controller(argv[i])) != NULL) {
               used_access_controllers[k++] = acl_mod;
          }
          else {
               ci_debug_printf(1, "No access controller with name :%s\n",
                               argv[i]);
               ret = 0;
          }
     }
     used_access_controllers[k] = NULL;
     return ret;

}


int cfg_set_auth_method(char *directive, char **argv, void *setdata)
{
     char *method = NULL;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          return 0;
     }
     method = argv[0];

     if (strncasecmp(argv[1], "none", 4) == 0) {
          return set_method_authenticators(method, NULL);
     }
     return set_method_authenticators(method, argv + 1);
}


int cfg_include_config_file(char *directive, char **argv, void *setdata)
{
     char path[CI_MAX_PATH], *cfg_file;

     if (argv == NULL || argv[0] == NULL) {
          return 0;
     }
     cfg_file = argv[0];
     if (cfg_file[0] != '/') {  /*Win32 code? */
          snprintf(path, CI_MAX_PATH, CONFDIR "/%s", cfg_file);
          path[CI_MAX_PATH - 1] = '\0';
          cfg_file = path;
     }

     ci_debug_printf(1, "\n*** Going to open config file %s ***\n", cfg_file);
     return parse_file(cfg_file);
}

/**************************************************************************/
/* Parse file functions                                                   */

int fread_line(FILE * f_conf, char *line)
{
     if (!fgets(line, LINESIZE, f_conf))
          return 0;
     if (strlen(line) >= LINESIZE - 2 && line[LINESIZE - 2] != '\n') {  //Size of line > LINESIZE
          while (!feof(f_conf)) {
               if (fgetc(f_conf) == '\n')
                    return 1;
          }
          return 0;
     }
     return 1;
}


struct conf_entry *find_action(char *str, char **arg)
{
     char *end, *table, *s;
     int size;
     end = str;
     while (*end != '\0' && !isspace(*end))
          end++;
     size = end - str;
     *end = '\0';               /*Mark the end of Variable...... */
     end++;                     /*... and continue.... */
     while (*end != '\0' && isspace(*end))      /*Find the start of arguments ...... */
          end++;
     *arg = end;
     if ((s = strchr(str, '.')) != NULL) {
          table = str;
          str = s + 1;
          *s = '\0';
     }
     else
          table = NULL;

//     return search_conf_table(conf_variables,str);
     return search_variables(table, str);
}

char **split_args(char *args)
{
     int len, i = 0;
     char **argv = NULL, *str, *end;
     argv = malloc((MAX_ARGS + 1) * sizeof(char *));
     end = args;
     do {
          str = end;
          if (*end == '"') {
               end++;
               str = end;
               while (*end != '\0' && *end != '"')
                    end++;
          }
          else {
               while (*end != '\0' && !isspace(*end))
                    end++;
          }
          len = end - str;

          argv[i] = malloc((len + 1) * sizeof(char));

          memcpy(argv[i], str, len);    /*copy until len or end of string */
          argv[i][len] = '\0';
          ++i;

          if (i >= MAX_ARGS)
               break;

          if (*end == '"')
               end++;
          while (*end != '\0' && isspace(*end))
               end++;

     } while (*end != '\0');
     argv[i] = NULL;

     return argv;
}

void free_args(char **argv)
{
     int i;
     if (argv == NULL)
          return;
     for (i = 0; argv[i] != NULL; i++) {
          free(argv[i]);
          argv[i] = NULL;
     }
     free(argv);
}

int process_line(char *line)
{
     char *str, *args, **argv = NULL;
     struct conf_entry *entry;

     str = line;
     while (*str != '\0' && isspace(*str))      /*Eat the spaces in the begging */
          str++;
     if (*str == '\0' || *str == '#')   /*Empty line or comment */
          return 0;

     entry = find_action(str, &args);
//   ci_debug_printf(10,"Line %s (Args:%s)\n",entry->name,args);

     if (entry && entry->action) {
          argv = split_args(args);
          (*(entry->action)) (entry->name, argv, entry->data);
          free_args(argv);
          return 1;
     }
      /*OK*/
         /*Else parse error.......
            Log an error..... */
         return 0;
}

static int PARSE_LEVEL = 0;

int parse_file(char *conf_file)
{
     FILE *f_conf;
     char line[LINESIZE];

     if (PARSE_LEVEL >= 3) {
          ci_debug_printf(1, "Parse level >3. I will not parse file:%s\n",
                          conf_file);
          return 0;
     }

     if ((f_conf = fopen(conf_file, "r")) == NULL) {
          //or log_server better........
          ci_debug_printf(1, "Can not open configuration file %s\n", conf_file);
          return 0;
     }
     PARSE_LEVEL++;
     while (!feof(f_conf)) {
          fread_line(f_conf, line);
          process_line(line);
     }

     fclose(f_conf);
     PARSE_LEVEL--;
     return 1;
}


/****************************************************************/
/* Command line options implementation, function and structures */


/* #ifdef _WIN32 */
/* #define opt_pre "/"  */
/* #else */
#define opt_pre "-"
/* #endif */

static struct options_entry options[] = {
     {opt_pre "f", "filename", &CONF.cfg_file, ci_cfg_set_str,
      "Specify the configuration file"},
     {opt_pre "N", NULL, &DAEMON_MODE, ci_cfg_disable, "Do not run as daemon"},
     {opt_pre "d", "level", NULL, cfg_set_debug_level,
      "Specify the debug level"},
     {opt_pre "D", NULL, NULL, cfg_set_debug_stdout,
      "Print debug info to stdout"},
     {NULL, NULL, NULL, NULL}
};



int config(int argc, char **argv)
{
     ARGC = argc;
     ARGV = argv;
     ci_cfg_lib_init();
     if (!ci_args_apply(argc, argv, options)) {
          ci_debug_printf(1, "Error in command line options\n");
          ci_args_usage(argv[0], options);
          exit(-1);
     }
     if (!parse_file(CONF.cfg_file)) {
          ci_debug_printf(1, "Error opening/parsing config file\n");
          exit(0);
     }
     return 1;
}

void cfg_default_value_restore_all();
int reconfig()
{
     /*reseting all parameters to default values ... */
     cfg_default_value_restore_all();
     /*reseting cfg_lib */
     ci_cfg_lib_reset();
     if (!ci_args_apply(ARGC, ARGV, options)) {
          ci_debug_printf(1,
                          "Error in command line options, while reconfiguring!\n");
          return 0;
     }
     if (!parse_file(CONF.cfg_file)) {
          ci_debug_printf(1,
                          "Error opening/parsing config file, while reconfiguring!\n");
          return 0;
     }
     return 1;

}


int init_server(int port, int *family);
void release_modules();
void ci_dlib_closeall();
int log_open();

void system_reconfigure()
{
     int old_port;
     ci_debug_printf(1, "Going to reconfigure system!\n");
     /*
        - reset commands table
      */
     reset_commands();
     /*
        - close/release services and modules
      */
     release_services();
     release_modules();
     ci_dlib_closeall();
     reset_conf_tables();
     ci_debug_printf(1, "All resources released. Going to reload!\n");
     init_modules();

     /*
        - Freeing all memory and resources used by configuration parameters (is it possible???)
        - reopen and read config file. Now the monitor process has now the new config parameters.
      */
     old_port = CONF.PORT;
     reconfig();

     /*
        - reinit listen socket if needed
      */
     if (old_port != CONF.PORT) {
          init_server(CONF.PORT, &(CONF.PROTOCOL_FAMILY));
     }

     log_open();

     /*
        - post_init services and modules
      */
     post_init_modules();
     post_init_services();
}

/**************************************************************************/
/*         Library functions                                              */

/*********************************************************************/
/* Implementation of a mechanism to keep default values of parameters*/

struct cfg_default_value *default_values = NULL;

/*We does not care about memory managment here.  Default values list created only 
  once at the beggining of c-icap and does not needed to free or reallocate memory... I think ...
*/
struct cfg_default_value *cfg_default_value_store(void *param, void *value,
                                                  int size)
{
     struct cfg_default_value *dval, *dval_search;

     if ((dval = cfg_default_value_search(param)))
          return dval;

     if (!(dval = malloc(sizeof(struct cfg_default_value))))
          return 0;
     dval->param = param;       /*Iam sure we can just set it to param_name, but..... */
     dval->size = size;
     if (!(dval->value = malloc(size))) {
          free(dval);
          return NULL;
     }
     memcpy(dval->value, value, size);
     dval->next = NULL;
     if (default_values == NULL) {
          default_values = dval;
          return dval;
     }
     dval_search = default_values;
     while (dval_search->next != NULL)
          dval_search = dval_search->next;
     dval_search->next = dval;
     return dval;
}

struct cfg_default_value *cfg_default_value_replace(void *param, void *value)
{
     struct cfg_default_value *dval;
     dval = default_values;
     while (dval != NULL && dval->param != param)
          dval = dval->next;

     if (!dval)
          return NULL;

     memcpy(dval->value, value, dval->size);
     return dval;
}

struct cfg_default_value *cfg_default_value_search(void *param)
{
     struct cfg_default_value *dval;
     dval = default_values;
     ci_debug_printf(8, "Searching %p for default valuen", param);
     while (dval != NULL && dval->param != param)
          dval = dval->next;
     return dval;
}

void *cfg_default_value_restore(void *param)
{
     struct cfg_default_value *dval;
     dval = default_values;
     ci_debug_printf(8, "Geting default value for %p\n", param);
     while (dval != NULL && dval->param != param)
          dval = dval->next;

     if (!dval)
          return NULL;
     ci_debug_printf(8, "Found: %p\n", dval->value);
     memcpy(param, dval->value, dval->size);
     return param;
}

void cfg_default_value_restore_all()
{
     struct cfg_default_value *dval;
     dval = default_values;
     while (dval != NULL) {
          memcpy(dval->param, dval->value, dval->size);
          dval = dval->next;
     }
}

/********************************************************************/
/* functions for setting parameters and saving the default values   */

int intl_cfg_set_str(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(char *));
     /*or better keep all string not just the pointer to default value? */
     return ci_cfg_set_str(directive, argv, setdata);
}

int intl_cfg_set_int(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(int));
     return ci_cfg_set_int(directive, argv, setdata);
}

int intl_cfg_onoff(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(int));
     return ci_cfg_onoff(directive, argv, setdata);
}

int intl_cfg_disable(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(int));
     return ci_cfg_disable(directive, argv, setdata);
}

int intl_cfg_enable(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(int));
     return ci_cfg_enable(directive, argv, setdata);
}

int intl_cfg_size_off(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(ci_off_t));
     return ci_cfg_size_off(directive, argv, setdata);
}

int intl_cfg_size_long(char *directive, char **argv, void *setdata)
{
     if (!setdata)
          return 0;
     cfg_default_value_store(setdata, setdata, sizeof(long int));
     return ci_cfg_size_long(directive, argv, setdata);
}
