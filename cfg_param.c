/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */

#include "common.h"
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
#include "commands.h"
#include "acl.h"
#include "txtTemplate.h"
#include "proc_mutex.h"
#include "port.h"
#include "registry.h"
#include "shared_mem.h"
#ifdef USE_OPENSSL
#include "net_io_ssl.h"
#endif

#define MAX_INCLUDE_LEVEL 5
#define LINESIZE 8192
#define MAX_DIRECTIVE_SIZE 80
#define MAX_ARGS 50
int ARGC;
char **ARGV;

struct ci_server_conf CI_CONF = {
    NULL,
#ifdef _WIN32
    "c:\\TEMP", /*TMPDIR*/ "c:\\TEMP\\c-icap.pid", /*PIDFILE*/ "\\\\.\\pipe\\c-icap",  /*COMMANDS_SOCKET; */
#else
    "/var/tmp/", /*TMPDIR*/ "/var/run/c-icap/c-icap.pid", /*PIDFILE*/ "/var/run/c-icap/c-icap.ctl",   /*COMMANDS_SOCKET; */
#endif
    NULL,                      /* RUN_USER */
    NULL,                      /* RUN_GROUP */
#ifdef _WIN32
    CI_CONFDIR "\\c-icap.conf",   /*cfg_file */
    CI_CONFDIR "\\c-icap.magic",  /*magics_file */
#else
    CI_CONFDIR "/c-icap.conf",    /*cfg_file */
    CI_CONFDIR "/c-icap.magic",   /*magics_file */
#endif
    NULL,                      /*MAGIC_DB */
    CI_SERVDIR,                   /*SERVICES_DIR */
    CI_MODSDIR,                   /*MODULES_DIR */
    NULL,                      /*SERVER_ADMIN*/
    NULL,                      /*SERVER_NAME*/
    5,                         /*START_SERVERS*/
    10,                        /*MAX_SERVERS*/
    30,                        /*THREADS_PER_CHILD*/
    30,                        /*MIN_SPARE_THREADS*/
    60                         /*MAX_SPARE_THREADS*/

#ifdef USE_OPENSSL
    ,
    0                         /*TLS_ENABLED, set by TLSPort*/
#endif
};


int TIMEOUT = 300;
int KEEPALIVE_TIMEOUT = 15;
int MAX_KEEPALIVE_REQUESTS = 100;
int MAX_SECS_TO_LINGER = 5;
int MAX_REQUESTS_BEFORE_REALLOCATE_MEM = 100;
int MAX_REQUESTS_PER_CHILD = 0;
int DAEMON_MODE = 1;
int VERSION_MODE = 0;
int HELP_MODE = 0;
int DebugLevelSetFromCmd = 0;
const char *DEFAULT_SERVICE = NULL; /*Default service if not defined in ICAP URI*/
int PIPELINING = 1;
int CHECK_FOR_BUGGY_CLIENT = 0;
int ALLOW204_AS_200OK_ZERO_ENCAPS = 0;
int FAKE_ALLOW204 = 1;

extern char *SERVER_LOG_FILE;
extern char *ACCESS_LOG_FILE;
extern char *ACCESS_LOG_FORMAT;
/*extern char *LOGS_DIR;*/

extern logger_module_t *default_logger;
extern access_control_module_t **used_access_controllers;

extern char *REMOTE_PROXY_USER_HEADER;
extern int ALLOW_REMOTE_PROXY_USERS;
extern int REMOTE_PROXY_USER_HEADER_ENCODED;


/*Functions declaration */
int parse_file(const char *conf_file);

/*config table functions*/
int cfg_load_magicfile(const char *directive, const char **argv, void *setdata);
int cfg_load_service(const char *directive, const char **argv, void *setdata);
int cfg_service_alias(const char *directive, const char **argv, void *setdata);
int cfg_load_module(const char *directive, const char **argv, void *setdata);
int cfg_set_logformat(const char *directive, const char **argv, void *setdata);
int cfg_set_logger(const char *directive, const char **argv, void *setdata);
int cfg_set_accesslog(const char *directive, const char **argv, void *setdata);
int cfg_set_debug_level(const char *directive, const char **argv, void *setdata);
int cfg_set_debug_stdout(const char *directive, const char **argv, void *setdata);
int cfg_set_body_maxmem(const char *directive, const char **argv, void *setdata);
int cfg_set_tmp_dir(const char *directive, const char **argv, void *setdata);
int cfg_set_acl_controllers(const char *directive, const char **argv, void *setdata);
int cfg_set_auth_method(const char *directive, const char **argv, void *setdata);
int cfg_include_config_file(const char *directive, const char **argv, void *setdata);
int cfg_group_source_by_group(const char *directive, const char **argv, void *setdata);
int cfg_group_source_by_user(const char *directive, const char **argv, void *setdata);
int cfg_shared_mem_scheme(const char *directive, const char **argv, void *setdata);
int cfg_proc_lock_scheme(const char *directive, const char **argv, void *setdata);
int cfg_set_port(const char *directive, const char **argv, void *setdata);

int cfg_set_template_dir(const char *directive, const char **argv, void *setdata);
int cfg_set_template_default_lang(const char *directive, const char **argv, void *setdata);
int cfg_set_template_reload_time(const char *directive, const char **argv, void *setdata);
int cfg_set_template_cache_size(const char *directive, const char **argv, void *setdata);
int cfg_set_template_membuf_size(const char *directive, const char **argv, void *setdata);

/*The following 2 functions defined in access.c file*/
int cfg_acl_add(const char *directive, const char **argv, void *setdata);
int cfg_default_acl_access(const char *directive, const char **argv, void *setdata);
/****/

struct sub_table {
    const char *name;
    int type;
    struct ci_conf_entry *conf_table;
};

static struct ci_conf_entry conf_variables[] = {
//     {"ListenAddress", &CI_CONF.ADDRESS, intl_cfg_set_str, NULL},
    {"PidFile", &CI_CONF.PIDFILE, intl_cfg_set_str, NULL},
    {"CommandsSocket", &CI_CONF.COMMANDS_SOCKET, intl_cfg_set_str, NULL},
    {"Timeout", (void *) (&TIMEOUT), intl_cfg_set_int, NULL},
    {"KeepAlive", NULL, NULL, NULL},
    {"MaxKeepAliveRequests", &MAX_KEEPALIVE_REQUESTS, intl_cfg_set_int, NULL},
    {"KeepAliveTimeout", &KEEPALIVE_TIMEOUT, intl_cfg_set_int, NULL},
    {"StartServers", &CI_CONF.START_SERVERS, intl_cfg_set_int, NULL},
    {"MaxServers", &CI_CONF.MAX_SERVERS, intl_cfg_set_int, NULL},
    {"MinSpareThreads", &CI_CONF.MIN_SPARE_THREADS, intl_cfg_set_int, NULL},
    {"MaxSpareThreads", &CI_CONF.MAX_SPARE_THREADS, intl_cfg_set_int, NULL},
    {"ThreadsPerChild", &CI_CONF.THREADS_PER_CHILD, intl_cfg_set_int, NULL},
    {"MaxRequestsPerChild", &MAX_REQUESTS_PER_CHILD, intl_cfg_set_int, NULL},
    {"MaxRequestsReallocateMem", &MAX_REQUESTS_BEFORE_REALLOCATE_MEM, intl_cfg_set_int, NULL},
    {"Port", &CI_CONF.PORTS, cfg_set_port, NULL},
#ifdef USE_OPENSSL
    {"TlsPort", &CI_CONF.PORTS, cfg_set_port, NULL},
    {"TlsPassphrase", &CI_CONF.TLS_PASSPHRASE, intl_cfg_set_str, NULL},
    /*The Ssl* alias of Tls* cfg params*/
    {"SslPort", &CI_CONF.PORTS, cfg_set_port, NULL},
    {"SslPassphrase", &CI_CONF.TLS_PASSPHRASE, intl_cfg_set_str, NULL},
#endif
    {"User", &CI_CONF.RUN_USER, intl_cfg_set_str, NULL},
    {"Group", &CI_CONF.RUN_GROUP, intl_cfg_set_str, NULL},
    {"ServerAdmin", &CI_CONF.SERVER_ADMIN, intl_cfg_set_str, NULL},
    {"ServerName", &CI_CONF.SERVER_NAME, intl_cfg_set_str, NULL},
    {"LoadMagicFile", NULL, cfg_load_magicfile, NULL},
    {"Logger", &default_logger, cfg_set_logger, NULL},
    {"ServerLog", &SERVER_LOG_FILE, intl_cfg_set_str, NULL},
    {"AccessLog", NULL, cfg_set_accesslog, NULL},
    {"LogFormat", NULL, cfg_set_logformat, NULL},
    {"DebugLevel", NULL, cfg_set_debug_level, NULL},   /*Set library's debug level */
    {"ServicesDir", &CI_CONF.SERVICES_DIR, intl_cfg_set_str, NULL},
    {"ModulesDir", &CI_CONF.MODULES_DIR, intl_cfg_set_str, NULL},
    {"Service", NULL, cfg_load_service, NULL},
    {"ServiceAlias", NULL, cfg_service_alias, NULL},
    {"Module", NULL, cfg_load_module, NULL},
    {"TmpDir", NULL, cfg_set_tmp_dir, NULL},
    {"MaxMemObject", NULL, cfg_set_body_maxmem, NULL}, /*Set library's body max mem */
    {"AclControllers", NULL, cfg_set_acl_controllers, NULL},
    {"acl", NULL, cfg_acl_add, NULL},
    {"icap_access", NULL, cfg_default_acl_access, NULL},
    {"client_access", NULL, cfg_default_acl_access, NULL},
    {"AuthMethod", NULL, cfg_set_auth_method, NULL},
    {"Include", NULL, cfg_include_config_file, NULL},
    {"RemoteProxyUserHeader", &REMOTE_PROXY_USER_HEADER, intl_cfg_set_str, NULL},
    {"RemoteProxyUserHeaderEncoded", &REMOTE_PROXY_USER_HEADER_ENCODED, intl_cfg_onoff, NULL},
    {"RemoteProxyUsers", &ALLOW_REMOTE_PROXY_USERS, intl_cfg_onoff, NULL},
    {"TemplateDir", NULL, cfg_set_template_dir, NULL},
    {"TemplateDefaultLanguage", NULL, cfg_set_template_default_lang, NULL},
    {"TemplateReloadTime", NULL, cfg_set_template_reload_time, NULL},
    {"TemplateCacheSize", NULL, cfg_set_template_cache_size, NULL},
    {"TemplateMemBufSize", NULL, cfg_set_template_membuf_size, NULL},
    {"GroupSourceByGroup", NULL, cfg_group_source_by_group, NULL},
    {"GroupSourceByUser", NULL, cfg_group_source_by_user, NULL},
    {"InterProcessSharedMemScheme", NULL, cfg_shared_mem_scheme, NULL},
    {"InterProcessLockingScheme", NULL, cfg_proc_lock_scheme, NULL},
    {"DefaultService", &DEFAULT_SERVICE, intl_cfg_set_str, NULL},
    {"Pipelining", &PIPELINING, intl_cfg_onoff, NULL},
    {"SupportBuggyClients", &CHECK_FOR_BUGGY_CLIENT, intl_cfg_onoff, NULL},
    {"Allow204As200okZeroEncaps", &ALLOW204_AS_200OK_ZERO_ENCAPS, intl_cfg_enable, NULL},
    {"FakeAllow204", &FAKE_ALLOW204, intl_cfg_onoff, NULL},
    {NULL, NULL, NULL, NULL}
};

#define STEPSIZE 10
static struct sub_table *extra_conf_tables = NULL;
int conf_tables_list_size = 0;
int conf_tables_num = 0;


struct ci_conf_entry *search_conf_table(struct ci_conf_entry *table, char *varname)
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

int register_conf_table(const char *name, struct ci_conf_entry *table, int type)
{
    struct sub_table *new;
    int i, insert_pos;
    if (!extra_conf_tables)
        return 0;

    insert_pos = -1;
    for (i = 0; insert_pos < 0 && i < conf_tables_num; i++) {
        if (extra_conf_tables[i].name && strcmp(name, extra_conf_tables[i].name) == 0) {
            ci_debug_printf(1,"Config table :%s already exists!\n", name);
            return 0;
        } else if (extra_conf_tables[i].name == NULL) { /*empty pos use this one*/
            insert_pos = i;
        }
    }

    if (insert_pos < 0) { /*if not empry pos found add it to the end*/
        insert_pos = conf_tables_num;

        if (conf_tables_num == conf_tables_list_size) {
            /*tables list is full, reallocating space ...... */
            if (NULL ==
                    (new =
                         realloc(extra_conf_tables, sizeof(struct sub_table)*(conf_tables_list_size + STEPSIZE))))
                return 0;
            extra_conf_tables = new;
            conf_tables_list_size += STEPSIZE;
        }
        conf_tables_num++;
    }

    ci_debug_printf(10, "Registering conf table: %s\n", name);

    extra_conf_tables[insert_pos].name = name;    /*It works. Points to the modules.name. (????) */
    extra_conf_tables[insert_pos].type = type;
    extra_conf_tables[insert_pos].conf_table = table;
    return 1;
}

struct ci_conf_entry *unregister_conf_table(const char *name)
{
    int i;
    struct ci_conf_entry *table;

    if (extra_conf_tables) {   /*Not really needed........ */
        for (i = 0; i < conf_tables_num; i++) {
            if (extra_conf_tables[i].name && strcmp(name, extra_conf_tables[i].name) == 0) {
                table = extra_conf_tables[i].conf_table;
                extra_conf_tables[i].name = NULL;
                extra_conf_tables[i].type = 0;
                extra_conf_tables[i].conf_table = NULL;
                return table;
            }
        }
    }
    ci_debug_printf(1, "Table %s not found!\n", name);
    return NULL;
}

struct ci_conf_entry *search_variables(char *table, char *varname)
{
    int i;
    if (table == NULL)
        return search_conf_table(conf_variables, varname);

    ci_debug_printf(3, "Going to search variable %s in table %s\n", varname,
                    table);

    if (!extra_conf_tables)    /*Not really needed........ */
        return NULL;

    for (i = 0; i < conf_tables_num; i++) {
        if (extra_conf_tables[i].name && strcmp(table, extra_conf_tables[i].name) == 0) {
            return search_conf_table(extra_conf_tables[i].conf_table,
                                     varname);
        }
    }
    ci_debug_printf(1, "Variable %s or table %s not found!\n", varname, table);
    return NULL;
}

void print_conf_variables(struct ci_conf_entry *table)
{
    int i;
    for (i = 0; table[i].name != NULL; i++) {
        ci_debug_printf(9, "%s=", table[i].name);
        if (!table[i].data) {
            ci_debug_printf(9, "\n");
        } else if (table[i].action == intl_cfg_set_str) {
            if (*(char *) table[i].data) {
                ci_debug_printf(9, "%s\n", *(char **) table[i].data);
            } else {
                ci_debug_printf(9, "\n");
            }
        } else if (table[i].action == intl_cfg_set_int) {
            ci_debug_printf(9, "%d\n", *(int *) table[i].data);
        } else if (table[i].action == intl_cfg_size_off) {
            ci_debug_printf(9, "%" PRINTF_OFF_T "\n",
                            (CAST_OFF_T) *(ci_off_t *) table[i].data);
        } else if (table[i].action == intl_cfg_size_long) {
            ci_debug_printf(9, "%ld\n", *(long *) table[i].data);
        } else if (table[i].action == intl_cfg_onoff) {
            ci_debug_printf(9, "%d\n", *(int *) table[i].data);
        } else if (table[i].action == intl_cfg_enable) {
            ci_debug_printf(9, "%d\n", *(int *) table[i].data);
        } else if (table[i].action == intl_cfg_disable) {
            ci_debug_printf(9, "%d\n", *(int *) table[i].data);
        } else if (table[i].data) {
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
        if ( extra_conf_tables[i].name) {
            ci_debug_printf(9, "Printing variables in table %s\n",
                            extra_conf_tables[i].name);
            print_conf_variables(extra_conf_tables[i].conf_table);
        }
    }
    return 1;
}


/************************************************************************/
/*  Set variables functions                                             */
/*
   The following tree functions refered to non constant variables so
   the compilers in Win32 have problem to appeared in static arrays
*/
int cfg_set_port(const char *directive, const char **argv, void *setdata)
{
    int i;
    char *s, *addr, *connect_port;
    ci_port_t *pcfg = NULL;
    ci_port_t tmpP;
    ci_vector_t **port_configs = (ci_vector_t **)setdata;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in %s directive\n", directive);
        return 0;
    }
    if (!*port_configs)
        *port_configs = ci_vector_create(2048);

    memset(&tmpP, 0, sizeof(ci_port_t));

    pcfg = (ci_port_t *)ci_vector_add(*port_configs, (void *)&tmpP, sizeof(ci_port_t));
    if (!pcfg) {
        ci_debug_printf(1, "Maximum number of configured ports is reached\n");
        return 0;
    }

    connect_port = strdup(argv[0]);
    if ((s = strrchr(connect_port, ':'))) {
        *s = '\0';
        addr = connect_port;
        if (*addr == '[') {
            if (addr[strlen(addr) - 1] != ']') {
                free(connect_port);
                ci_debug_printf(1, "Failed to parse listen address: %s\n", addr);
                return 0;
            }
            ++addr;
            addr[strlen(addr) - 1] = '\0';
        }
        pcfg->address = strdup(addr);
        s++;
    } else
        s = connect_port;

    pcfg->port = atoi(s);
    free(connect_port);
    connect_port = s = NULL;
    if (pcfg->port <= 0) {
        ci_debug_printf(1, "Failed to parse %s (parsed port number:%d)\n", directive, pcfg->port);
        return 0;
    }

    if (!argv[1])
        return 1;

#ifdef USE_OPENSSL
    int isTls = (strcmp(directive, "TlsPort") == 0 || strcmp(directive, "SslPort") == 0) ? 1 : 0;
    CI_CONF.TLS_ENABLED = 1;
    pcfg->tls_enabled = 1;
#endif

    for (i = 1; argv[i] != NULL; ++i) {
#ifdef USE_OPENSSL
        if (isTls && icap_port_tls_option(argv[i], pcfg, CI_CONFDIR)) {
        } else
#endif
        {
            ci_debug_printf(1, "Unknown %s option: '%s'", directive, argv[i]);
            return 0;
        }
    }

    return 1;
}

int cfg_set_debug_level(const char *directive, const char **argv, void *setdata)
{
    if (!DebugLevelSetFromCmd)
        return intl_cfg_set_int(directive, argv, &CI_DEBUG_LEVEL);
    /*else ignore ....*/
    return 1;
}

int cfg_set_debug_level_cmd(const char *directive, const char **argv, void *setdata)
{
    DebugLevelSetFromCmd = 1;
    return intl_cfg_set_int(directive, argv, &CI_DEBUG_LEVEL);
}

int cfg_set_debug_stdout(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_enable(directive, argv, &CI_DEBUG_STDOUT);
}

int cfg_set_body_maxmem(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_size_long(directive, argv, &CI_BODY_MAX_MEM);
}

int cfg_load_service(const char *directive, const char **argv, void *setdata)
{
    ci_service_module_t *service = NULL;
    if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        ci_debug_printf(1, "Missing arguments in LoadService directive\n");
        return 0;
    }
    ci_debug_printf(2, "Loading service: %s path %s\n", argv[0], argv[1]);

    if (!(service = register_service(argv[1], argv + 2))) {
        ci_debug_printf(1, "Error loading service %s\n", argv[1]);
        return 0;
    }
    add_service_alias(argv[0], service->mod_name, NULL);
    return 1;
}

int cfg_service_alias(const char *directive, const char **argv, void *setdata)
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

    ci_debug_printf(2, "Alias: %s of service %s\n", argv[0], argv[1]);
    add_service_alias(argv[0], argv[1], service_args);
    return 1;
}

int cfg_load_module(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        ci_debug_printf(1, "Missing arguments in LoadModule directive\n");
        return 0;
    }
    ci_debug_printf(2, "Loading service: %s path %s\n", argv[0], argv[1]);

    if (!register_module(argv[1], argv[0], argv + 2)) {
        ci_debug_printf(1, "Error loading module %s, module path %s\n", argv[1], argv[0]);
        return 0;
    }
    return 1;
}

int cfg_load_magicfile(const char *directive, const char **argv, void *setdata)
{
    const char *db_file;
    struct ci_magics_db *ndb;
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    db_file = argv[0];
    if (strcmp(CI_CONF.magics_file, db_file) == 0) {
        ci_debug_printf(2, "The db file %s is the same as default. Ignoring...\n", db_file);
        return 1;
    }
    ci_debug_printf(2, "Going to load magic file %s\n", db_file);
    ndb = ci_magic_db_load(db_file);
    if (!ndb) {
        ci_debug_printf(1, "Can not load magic file %s!!!\n", db_file);
        return 0;
    }
    if (!CI_CONF.MAGIC_DB)
        CI_CONF.MAGIC_DB = ndb;

    return 1;
}

int logformat_add(const char *name, const char *format);
int cfg_set_logformat(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive %s\n", directive);
        return 0;
    }
    ci_debug_printf(2, "Adding the logformat %s: %s\n",argv[0],argv[1]);
    return logformat_add(argv[0], argv[1]);
}

int file_log_addlogfile(const char *file, const char *format, const char **acls);
int cfg_set_accesslog(const char *directive, const char **argv, void *setdata)
{
    const char **acls = NULL;
    if (argv == NULL || argv[0] == NULL ) {
        ci_debug_printf(1, "Missing arguments in directive %s\n", directive);
        return 0;
    }
    if (argv[1] != NULL && argv[2] !=NULL) {
        acls = argv+2;
    }
    ci_debug_printf(2, "Adding the access logfile %s\n",argv[0]);
    return file_log_addlogfile(argv[0], argv[1], acls);
}


extern logger_module_t file_logger;
int cfg_set_logger(const char *directive, const char **argv, void *setdata)
{
    logger_module_t *logger;
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive\n");
        return 0;
    }

    if (!(logger = find_logger(argv[0])))
        return 0;
    default_logger = logger;
    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    return 1;
}

int cfg_set_tmp_dir(const char *directive, const char **argv, void *setdata)
{
    int len;
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    cfg_default_value_store(&CI_CONF.TMPDIR, &CI_CONF.TMPDIR, sizeof(char *));
    len = strlen(argv[0]);

    CI_CONF.TMPDIR = ci_cfg_alloc_mem((len + 2) * sizeof(char));
    strcpy(CI_CONF.TMPDIR, argv[0]);
#ifdef _WIN32
    if (CI_CONF.TMPDIR[len] != '\\') {
        CI_CONF.TMPDIR[len] = '\\';
        CI_CONF.TMPDIR[len + 1] = '\0';
    }
#else
    if (CI_CONF.TMPDIR[len] != '/') {
        CI_CONF.TMPDIR[len] = '/';
        CI_CONF.TMPDIR[len + 1] = '\0';
    }
#endif
    /*Check if tmpdir exists. If no try to build it , report an error and uses the default... */
    CI_TMPDIR = CI_CONF.TMPDIR;   /*Sets the library's temporary dir to .... */
    ci_debug_printf(2, "Setting parameter: %s=%s\n", directive, argv[0]);
    return 1;
}

int cfg_set_acl_controllers(const char *directive, const char **argv, void *setdata)
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
        ci_cfg_alloc_mem((argc+1) * sizeof(access_control_module_t *));
    k = 0;
    ret = 1;
    for (i = 0; i < argc; i++) {
        if ((acl_mod = find_access_controller(argv[i])) != NULL) {
            used_access_controllers[k++] = acl_mod;
        } else {
            ci_debug_printf(1, "No access controller with name :%s\n",
                            argv[i]);
            ret = 0;
        }
    }
    used_access_controllers[k] = NULL;
    return ret;

}


int cfg_set_auth_method(const char *directive, const char **argv, void *setdata)
{
    const char *method = NULL;
    if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        return 0;
    }
    method = argv[0];

    if (strncasecmp(argv[1], "none", 4) == 0) {
        return set_method_authenticators(method, NULL);
    }
    return set_method_authenticators(method, (const char **)argv + 1);
}

int cfg_acl_add(const char *directive, const char **argv, void *setdata)
{
    const char *acl_name, *acl_type;
    int argc, ok;

    if (!argv[0] || !argv[1] || !argv[2]) /* at least an argument */
        return 0;


    acl_name = argv[0];
    acl_type = argv[1];
    for (argc = 2, ok =1; argv[argc] != NULL && ok; argc++) {
        ci_debug_printf(2, "Adding to acl %s the data %s\n", acl_name, argv[argc]);
        ok = ci_acl_add_data(acl_name, acl_type, argv[argc]);
    }
    ci_debug_printf(2, "New ACL with name:%s and  ACL Type: %s\n", argv[0], argv[1]);
    return ok;
}

int cfg_include_config_file(const char *directive, const char **argv, void *setdata)
{
    char path[CI_MAX_PATH];
    const char *cfg_file;

    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }
    cfg_file = argv[0];
    if (cfg_file[0] != '/') {  /*Win32 code? */
        snprintf(path, CI_MAX_PATH, CI_CONFDIR "/%s", cfg_file);
        path[CI_MAX_PATH - 1] = '\0';
        cfg_file = path;
    }

    ci_debug_printf(2, "\n*** Going to open config file %s ***\n", cfg_file);
    return parse_file(cfg_file);
}

int group_source_add_by_group(const char *table_name);
int group_source_add_by_user(const char *table_name);

int cfg_group_source_by_group(const char *directive, const char **argv, void *setdata)
{
    const char *group_table = NULL;
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }
    group_table = argv[0];
    return group_source_add_by_group(group_table);
}

int cfg_group_source_by_user(const char *directive, const char **argv, void *setdata)
{
    const char *group_table = NULL;
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }
    group_table = argv[0];
    return group_source_add_by_user(group_table);
}

int cfg_shared_mem_scheme(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }
    return ci_shared_mem_set_scheme(argv[0]);
}

int cfg_proc_lock_scheme(const char *directive, const char **argv, void *setdata)
{
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }
    return ci_proc_mutex_set_scheme(argv[0]);
}

int cfg_set_template_dir(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_set_str(directive, argv, &TEMPLATE_DIR);
}

int cfg_set_template_default_lang(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_set_str(directive, argv, &TEMPLATE_DEF_LANG);
}

int cfg_set_template_reload_time(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_set_int(directive, argv, &TEMPLATE_RELOAD_TIME);
}

int cfg_set_template_cache_size(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_set_str(directive, argv, &TEMPLATE_CACHE_SIZE);
}

int cfg_set_template_membuf_size(const char *directive, const char **argv, void *setdata)
{
    return intl_cfg_set_str(directive, argv, &TEMPLATE_MEMBUF_SIZE);
}

/**************************************************************************/
/* Parse file functions                                                   */

int fread_line(FILE * f_conf, char *line)
{
    if (!fgets(line, LINESIZE, f_conf)) {
        if (feof(f_conf)) {
            line[0] = '\0';
            return -1;
        } else
            return 0;
    }
    if (strlen(line) >= LINESIZE - 2 && line[LINESIZE - 2] != '\n') {  //Size of line > LINESIZE
        while (!feof(f_conf)) {
            if (fgetc(f_conf) == '\n')
                return 1;
        }
        return 0;
    }
    return 1;
}


struct ci_conf_entry *find_action(char *str, char **arg)
{
    char *end, *table, *s;
    end = str;
    while (*end != '\0' && !isspace(*end))
        end++;
    *end = '\0';               /*Mark the end of Variable...... */
    end++;                     /*... and continue.... */
    while (*end != '\0' && isspace(*end))      /*Find the start of arguments ...... */
        end++;
    *arg = end;
    if ((s = strchr(str, '.')) != NULL) {
        table = str;
        str = s + 1;
        *s = '\0';
    } else
        table = NULL;

//     return search_conf_table(conf_variables,str);
    return search_variables(table, str);
}

char **split_args(char *args)
{
    int len, i = 0, brkt;
    char **argv = NULL, *str, *end, *p;
    argv = malloc((MAX_ARGS + 1) * sizeof(char *));
    end = args;
    do {
        str = end;
        if (*end == '"') {
            end++;
            str = end;
            while (*end != '\0' && *end != '"') {
                /*support escaped \" */
                if (*end == '\\' && *(end+1) == '"') {
                    for (p = end; *p != '\0'; p++)
                        *p = *(p+1);
                }
                end++;
            }
        } else {
            /*Support arguments in the form arg{a, b...}*/
            brkt = 0;
            while (*end != '\0' && (!isspace(*end) || brkt)) {
                if (*end == '{') brkt = 1;
                else if (brkt && *end == '}') brkt = 0;
                end++;
            }
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

int process_line(char *orig_line)
{
    char *str, *args, **argv = NULL;
    int ret = 1;
    struct ci_conf_entry *entry;
    char line[LINESIZE];

    strncpy(line, orig_line, LINESIZE);
    line[LINESIZE-1] = '\0';

    str = line;
    while (*str != '\0' && isspace(*str))      /*Eat the spaces in the begging */
        str++;
    if (*str == '\0' || *str == '#')   /*Empty line or comment */
        return 1;

    entry = find_action(str, &args);
//   ci_debug_printf(10,"Line %s (Args:%s)\n",entry->name,args);

    if (entry && entry->action) {
        argv = split_args(args);
        if ((*(entry->action)) (entry->name, (const char **)argv, entry->data) == 0)
            ret = 0;
        free_args(argv);
        return ret;
    }
    /*OK*/
    /*Else parse error.......
       Log an error..... */
    return 0;
}

static int PARSE_LEVEL = 0;

int parse_file(const char *conf_file)
{
    FILE *f_conf;
    char line[LINESIZE];
    int line_count, ret_value;

    if (PARSE_LEVEL >= MAX_INCLUDE_LEVEL) {
        ci_debug_printf(1, "Include level > %d. I will not parse file:%s\n",
                        MAX_INCLUDE_LEVEL,
                        conf_file);
        return 0;
    }

    if ((f_conf = fopen(conf_file, "r")) == NULL) {
        //or log_server better........
        ci_debug_printf(1, "Can not open configuration file %s\n", conf_file);
        return 0;
    }
    line_count = 0;
    ret_value = 1;
    PARSE_LEVEL++;
    while (!feof(f_conf)) {
        line_count++;
        if (!fread_line(f_conf, line) || !process_line(line)) {
            ci_debug_printf(1, "Fatal error while parsing config file: \"%s\" line: %d\n",
                            conf_file, line_count);
            ci_debug_printf(1, "The line is: %s\n", line);
            ret_value = 0;
        }
    }

    fclose(f_conf);
    PARSE_LEVEL--;
    return ret_value;
}


/****************************************************************/
/* Command line options implementation, function and structures */


/* #ifdef _WIN32 */
/* #define opt_pre "/"  */
/* #else */
#define opt_pre "-"
/* #endif */

static struct ci_options_entry options[] = {
    {opt_pre "V", NULL, &VERSION_MODE, ci_cfg_version, "Print c-icap version and exits"},
    {opt_pre "VV", NULL, &VERSION_MODE, ci_cfg_build_info, "Print c-icap version and build informations and exits"},
    {
        opt_pre "f", "filename", &CI_CONF.cfg_file, ci_cfg_set_str,
        "Specify the configuration file"
    },
    {opt_pre "N", NULL, &DAEMON_MODE, ci_cfg_disable, "Do not run as daemon"},
    {
        opt_pre "d", "level", NULL, cfg_set_debug_level_cmd,
        "Specify the debug level"
    },
    {
        opt_pre "D", NULL, NULL, cfg_set_debug_stdout,
        "Print debug info to stdout"
    },
    {opt_pre "h", NULL, &HELP_MODE, ci_cfg_enable, "Do not run as daemon"},
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
    if (VERSION_MODE)
        exit(0);
    if (HELP_MODE) {
        ci_args_usage(argv[0], options);
        exit(0);
    }
    if (!parse_file(CI_CONF.cfg_file)) {
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
    if (!parse_file(CI_CONF.cfg_file)) {
        ci_debug_printf(1,
                        "Error opening/parsing config file, while reconfiguring!\n");
        return 0;
    }
    return 1;

}


int init_server();
void release_modules();
void ci_dlib_closeall();
int log_open();
void reset_http_auth();

void system_shutdown()
{
    /*
      - reset commands table
    */
    commands_reset();

    /*
     - clean registry
    */
    ci_registry_clean();

    /*
      - close/release services and modules
    */
    release_services();
    release_modules();
    ci_dlib_closeall();

    /*
        Release other subsystems
     */
    ci_magic_db_free();
    CI_CONF.MAGIC_DB = NULL;
    ci_txt_template_close();
#ifdef USE_OPENSSL
    ci_tls_cleanup();
#endif
}

int system_reconfigure()
{
    ci_vector_t *old_ports;
    ci_debug_printf(1, "Going to reconfigure system!\n");

    old_ports = CI_CONF.PORTS;
    CI_CONF.PORTS = NULL;

    system_shutdown();
    reset_conf_tables();
    ci_acl_reset();
    reset_http_auth();

    ci_debug_printf(1, "All resources released. Going to reload!\n");
    ci_txt_template_init();
    if (!(CI_CONF.MAGIC_DB = ci_magic_db_load(CI_CONF.magics_file))) {
        ci_debug_printf(1, "Can not load magic file %s!!!\n",
                        CI_CONF.magics_file);
    }
    init_modules();
    init_services();

    /*
       - Freeing all memory and resources used by configuration parameters (is it possible???)
       - reopen and read config file. Now the monitor process has now the new config parameters.
     */
    if (!reconfig())
        return 0;

    /*Check the ports, to not close and reopen ports unchanged ports.*/
    ci_port_handle_reconfigure(CI_CONF.PORTS, old_ports);
    ci_port_list_release(old_ports);

    /*
       - reinit listen socket if needed
     */
    if (!init_server())
        return 0;

    log_open();

    /*
       - post_init services and modules
     */
    post_init_modules();
    post_init_services();
    return 1;
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
    ci_debug_printf(8, "Searching %p for default value\n", param);
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

int intl_cfg_set_str(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(char *));
    /*or better keep all string not just the pointer to default value? */
    return ci_cfg_set_str(directive, argv, setdata);
}

int intl_cfg_set_int(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(int));
    return ci_cfg_set_int(directive, argv, setdata);
}

int intl_cfg_onoff(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(int));
    return ci_cfg_onoff(directive, argv, setdata);
}

int intl_cfg_disable(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(int));
    return ci_cfg_disable(directive, argv, setdata);
}

int intl_cfg_enable(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(int));
    return ci_cfg_enable(directive, argv, setdata);
}

int intl_cfg_size_off(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(ci_off_t));
    return ci_cfg_size_off(directive, argv, setdata);
}

int intl_cfg_size_long(const char *directive, const char **argv, void *setdata)
{
    if (!setdata)
        return 0;
    cfg_default_value_store(setdata, setdata, sizeof(long int));
    return ci_cfg_size_long(directive, argv, setdata);
}
