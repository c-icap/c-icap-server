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


#ifndef __C_ICAP_CFG_PARAM_H
#define __C_ICAP_CFG_PARAM_H
#include "c-icap.h"
#include "array.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct ci_magics_db;
struct ci_port;

/**
 \defgroup CONFIG c-icap server configuration API
 \ingroup API
 *
 */


/**
 * This struct holds the basic configurations of c-icap server. It passed as
 * argument to services and modules inititalization functions
 \ingroup CONFIG
 *
 * Do not use directly this struct but better use the documended macros and
 * functions.
 */
struct ci_server_conf {
    ci_vector_t  *PORTS;
    char *TMPDIR;
    char *PIDFILE;
    char *COMMANDS_SOCKET;
    char *RUN_USER;
    char *RUN_GROUP;
    char *cfg_file;
    char *magics_file;
    struct ci_magics_db *MAGIC_DB;
    char *SERVICES_DIR;
    char *MODULES_DIR;
    char *SERVER_ADMIN;
    char *SERVER_NAME;
    int START_SERVERS;
    int MAX_SERVERS;
    int THREADS_PER_CHILD;
    int MIN_SPARE_THREADS;
    int MAX_SPARE_THREADS;

#ifdef USE_OPENSSL
    char *TLS_PASSPHRASE;
    int TLS_ENABLED;
#endif
};

/**
 * This struct holds a configuration parameter of c-icap server.
 \ingroup CONFIG
 * An array of ci_conf_entry structs can be used to define the configuration
 * directives of a service or module which can be set in c-icap configuration
 * file.
 \code
 int AParam;
 struct ci_conf_entry conf_table[] = {
    {"Aparameter", &AParam, ci_cfg_set_int, "This is a simple configuration parameter"},
    {NULL,NULL,NULL,NULL}
 }
 \endcode
 * In the above example the  ci_cfg_set_int function is predefined.
 * If the table "conf_table" attached to the service "AService" then the AParam
 * integer variable can be set from the c-icap configuration file using the
 * directive "AService.Aparameter"
 */
struct ci_conf_entry {
    /**
     * The configuration directive
     */
    const char *name;
    /**
     * A pointer to the configuration data
     */
    void *data;
    /**
     * Pointer to the function which will be used to set configuration data.
     \param name is the configuration directive.It passed as argument by the
     *      c-icap server
     \param argv is a NULL termined string array which holds the list of
     *      arguments of configuration parameter
     \param setdata is o pointer to set data which passed as argument by
     *      c-icap server
     \return Non zero on success, zero otherwise
     */
    int (*action)(const char *name, const char **argv,void *setdata);
    /**
     * A description message
     */
    const char *msg;
};

/* Command line options implementation structure */
struct ci_options_entry {
    const char *name;
    const char *parameter;
    void *data;
    int (*action)(const char *name, const char **argv,void *setdata);
    const char *msg;
};

/*Struct for storing default parameter values*/
struct cfg_default_value {
    void *param;
    void *value;
    int size;
    struct cfg_default_value *next;
};

#define MAIN_TABLE 1
#define ALIAS_TABLE 2

#ifndef CI_BUILD_LIB
extern struct ci_server_conf CI_CONF;

struct cfg_default_value * cfg_default_value_store(void *param, void *value,int size);
struct cfg_default_value * cfg_default_value_replace(void *param, void *value);
void *                     cfg_default_value_restore(void *value);
struct cfg_default_value * cfg_default_value_search(void *param);

int register_conf_table(const char *name,struct ci_conf_entry *table,int type);
struct ci_conf_entry * unregister_conf_table(const char *name);
int config(int argc, char **argv);

int intl_cfg_set_str(const char *directive,const char **argv,void *setdata);
int intl_cfg_set_int(const char *directive,const char **argv,void *setdata);
int intl_cfg_onoff(const char *directive,const char **argv,void *setdata);
int intl_cfg_disable(const char *directive,const char **argv,void *setdata);
int intl_cfg_enable(const char *directive,const char **argv,void *setdata);
int intl_cfg_size_off(const char *directive,const char **argv,void *setdata);
int intl_cfg_size_long(const char *directive,const char **argv,void *setdata);
int intl_cfg_set_octal(const char *directive, const char **argv, void *setdata);
int intl_cfg_set_int_range(const char *directive, const char **argv, void *setdata);
#endif


CI_DECLARE_FUNC(void)   ci_cfg_lib_init();
CI_DECLARE_FUNC(void)   ci_cfg_lib_reset();
CI_DECLARE_FUNC(void *) ci_cfg_alloc_mem(int size);

/**
 * Sets a string configuration parameter. The setdata are a pointer to a
 * string pointer
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_set_str(const char *directive,const char **argv,void *setdata);

/**
 * Sets an int configuration parameter. The setdata is a pointer to an integer
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_set_int(const char *directive,const char **argv,void *setdata);

/**
 * Sets an on/off configuration parameter. The setdata is a pointer to an
 * integer, which when the argument is "on" it is set to 1 and when the
 * argument is "off" it is set to 0.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_onoff(const char *directive,const char **argv,void *setdata);

/**
 * Can used with configuration parameters which does not takes arguments but
 * when defined just disable a feature.
 * The setdata is a pointer to an int which is set to zero.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_disable(const char *directive,const char **argv,void *setdata);

/**
 * Can used with configuration parameters which does not takes arguments but
 * when defined just enable a feature.
 * The setdata is a pointer to an int which is set to non zero.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_enable(const char *directive,const char **argv,void *setdata);

/**
 * Sets a configuration parameter of type ci_off_t (typedef of off_t type).
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_size_off(const char *directive,const char **argv,void *setdata);

/**
 * Sets a configuration parameter of type long.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_size_long(const char *directive,const char **argv,void *setdata);

/**
 * Sets a configuration parameter of type int to a value expressed in octal
 * form.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_set_octal(const char *directive, const char **argv, void *setdata);

/**
 * Sets a configuration parameter of type float.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_set_float(const char *directive,const char **argv,void *setdata);

/**
 * Sets a configuration parameter of type double.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_set_double(const char *directive,const char **argv,void *setdata);


struct ci_cfg_int_range {
    int *data;
    int start;
    int end;
};

/**
 * Builds range specification for integer variable VAR.
 * For use with the  ci_cfg_set_int_range function.
 \ingroup CONFIG
 */
#define CI_CFG_INT_RANGE(VAR, START, END) (&(struct ci_cfg_int_range){&VAR, START, END})

/**
 * Sets an int configuration parameter.
 * The setdata is passed using the CI_CFG_INT_RANGE macro to define the
 * range of accepted values.
 \ingroup CONFIG
 * example usage:
 \code
 int RangeParam = 0;
 struct ci_conf_entry my_module_conf_variables[] = {
 ...
 {"RangeParameter", CI_CFG_INT_RANGE(RangeParam, -100, 100), ci_cfg_set_int_range, NULL},
 ...
 };
 \endcode
 */
CI_DECLARE_FUNC(int) ci_cfg_set_int_range(const char *directive, const char **argv, void *setdata);

/**
 * Sets a configuration parameter of type int to 1 and prints c-icap version.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_version(const char *directive, const char **argv, void *setdata);

/**
 * Sets a configuration parameter of type int to 1 and prints c-icap build
 * information.
 \ingroup CONFIG
 */
CI_DECLARE_FUNC(int) ci_cfg_build_info(const char *directive, const char **argv, void *setdata);

CI_DECLARE_FUNC(void) ci_args_usage(const char *progname,struct ci_options_entry *options);
CI_DECLARE_FUNC(int)  ci_args_apply(int argc, char *argv[],struct ci_options_entry *options);


#ifdef __CI_COMPAT
#define  icap_server_conf   ci_server_conf
#define  conf_entry         ci_conf_entry
#define  options_entry      ci_options_entry
#endif

#ifdef __cplusplus
}
#endif

#endif
