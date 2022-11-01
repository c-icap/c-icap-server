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
#include "module.h"
#include "header.h"
#include "body.h"
#include "debug.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "dlib.h"
#include "cfg_param.h"

#define FLAG_MOD_STATIC 0x1
struct module_entry {
    void *module;
    uint64_t flags;
};

struct modules_list {
    struct module_entry *modules;
    int modules_num;
    int list_size;
};


/*
 ci_service_module_t **module_list=NULL;
 int module_list_size;
 int modules_num=0;
*/
#define STEP 20


static struct modules_list service_handlers;
service_handler_module_t *default_service_handler;

static struct modules_list loggers = {NULL, 0, 0};
static struct modules_list access_controllers = {NULL, 0, 0};
static struct modules_list auth_methods = {NULL, 0, 0};
static struct modules_list authenticators = {NULL, 0, 0};
static struct modules_list common_modules = {NULL, 0, 0};


static struct modules_list *modules_lists_table[] = {   /*Must follows the 'enum module_type'
                                                           enumeration */
    NULL,
    &service_handlers,
    &loggers,
    &access_controllers,
    &auth_methods,
    &authenticators,
    &common_modules
};


static void *load_module(const char *module_file, const char *argv[], int *static_declaration, int mod_type)
{
    common_module_t *(*module_builder)() = NULL;
    void *module = NULL;
    CI_DLIB_HANDLE module_handle;
    int forceUnload = 1;

    while (*argv != NULL) {
        if (strcasecmp(*argv, "forceUnload=off") == 0)
            forceUnload = 0;
        argv++;
    }

    module_handle = ci_module_load(module_file, CI_CONF.MODULES_DIR);
    if (!module_handle)
        return NULL;

    int cicap_release_ok = 1;
    void *build_for = ci_module_sym(module_handle, "__ci_build_for");
    if (build_for) {
        uint64_t module_cicap_version = *(uint64_t *) build_for;
        if (!C_ICAP_VERSION_MAJOR_CHECK(module_cicap_version)) {
            ci_debug_printf(1, "ERROR: Module is build with wrong c-icap release\n");
            cicap_release_ok = 0;
        }
    } else if (mod_type == COMMON) {
        ci_debug_printf(1, "WARNING: Can not check the used c-icap release to build service %s\n", module_file);
    }
    *static_declaration = 0;
    if (cicap_release_ok) {
        module = ci_module_sym(module_handle, "module");
        if (module) {
            *static_declaration = 1;
            ci_debug_printf(2, "Use old static module initialization\n");
        } else {
            if ((module_builder = ci_module_sym(module_handle, "__ci_module_build"))) {
                ci_debug_printf(3, "New c-icap modules initialization procedure\n");
                if ((module = (*module_builder)()) == NULL) {
                    ci_debug_printf(1, "ERROR: \"__ci_module_build\" return failed/nil\n");
                }
            }
        }
        if (!module) {
            ci_debug_printf(1, "No symbol \"module\" or \"__ci_module_build\" found in library\n");
        }
    }

    if (!module) {
        ci_debug_printf(1,
                        "Symbol \"module\" not found in library; unload it\n");
        ci_module_unload(module_handle, module_file);
        return NULL;
    }

    if (ci_module_sym(module_handle, CI_MOD_DISABLE_FORCE_UNLOAD_STR))
        forceUnload = 0;

    ci_dlib_entry("module", module_file, module_handle, forceUnload);
    return module;
}


/*
  Must called only in initialization procedure.
  It is not thread-safe!
*/
static void *add_to_modules_list(struct modules_list *mod_list, void *module, int isStatic)
{
    if (mod_list->modules == NULL) {
        mod_list->list_size = STEP;
        mod_list->modules = malloc(mod_list->list_size * sizeof(struct module_entry));
    } else if (mod_list->modules_num == mod_list->list_size) {
        mod_list->list_size += STEP;
        mod_list->modules =
            realloc(mod_list->modules, mod_list->list_size * sizeof(struct module_entry));
    }

    if (mod_list->modules == NULL) {
        //log an error......and...
        exit(-1);
    }
    mod_list->modules[mod_list->modules_num].module = module;
    mod_list->modules[mod_list->modules_num].flags = (isStatic ? 0x1 : 0);
    mod_list->modules_num++;
    return module;
}

static int module_type(const char *type)
{
    if (strcmp(type, "service_handler") == 0) {
        return SERVICE_HANDLER;
    } else if (strcmp(type, "logger") == 0) {
        return LOGGER;
    } else if (strcmp(type, "access_controller") == 0) {
        return ACCESS_CONTROLLER;
    } else if (strcmp(type, "auth_method") == 0) {
        return AUTH_METHOD;
    } else if (strcmp(type, "authenticator") == 0) {
        return AUTHENTICATOR;
    } else if (strcmp(type, "common") == 0) {
        return COMMON;
    }

    ci_debug_printf(1, "Uknown type of module:%s\n", type);
    return UNKNOWN;
}

#define INIT_MODULE(module, mod_type, INIT_METHOD)                      \
    if (((mod_type *) module)->INIT_METHOD)                             \
        ret = ((mod_type *) module)->INIT_METHOD(&CI_CONF);             \
    if (((mod_type *) module)->conf_table)                              \
        register_conf_table(((mod_type *) module)->name,                \
                            ((mod_type *) module)->conf_table, MAIN_TABLE);

static int init_module(void *module, enum module_type type)
{
    int ret = 0;
    switch (type) {
    case SERVICE_HANDLER:
        INIT_MODULE(module, service_handler_module_t, init_service_handler);
        break;
    case LOGGER:
        INIT_MODULE(module, logger_module_t, init_logger);
        break;
    case ACCESS_CONTROLLER:
        INIT_MODULE(module, access_control_module_t, init_access_controller);
        break;
    case AUTH_METHOD:
        INIT_MODULE(module, http_auth_method_t, init_auth_method);
        break;
    case AUTHENTICATOR:
        INIT_MODULE(module, authenticator_module_t, init_authenticator);
        break;
    case COMMON:
        INIT_MODULE(module, common_module_t, init_module);
        break;
    default:
        return 0;
    }
    return ret;
}


logger_module_t *find_logger(const char *name)
{
    logger_module_t *sh;
    int i;
    for (i = 0; i < loggers.modules_num; i++) {
        sh = (logger_module_t *) loggers.modules[i].module;
        if (sh->name && strcmp(sh->name, name) == 0)
            return sh;
    }
    return NULL;

}

access_control_module_t *find_access_controller(const char *name)
{
    access_control_module_t *sh;
    int i;
    for (i = 0; i < access_controllers.modules_num; i++) {
        sh = (access_control_module_t *) access_controllers.modules[i].module;
        if (sh->name && strcmp(sh->name, name) == 0)
            return sh;
    }
    return NULL;
}

common_module_t *find_common(const char *name)
{
    common_module_t *m;
    int i;
    for (i = 0; i < common_modules.modules_num; i++) {
        m = (common_module_t *) common_modules.modules[i].module;
        if (m->name && strcmp(m->name, name) == 0)
            return m;
    }
    return NULL;

}

/******************************************************************/




http_auth_method_t *find_auth_method(const char *method)
{
    int i;
    for (i = 0; i < auth_methods.modules_num; i++) {
        if (strcmp
                (method,
                 ((http_auth_method_t *) auth_methods.modules[i].module)->name) == 0)
            return (http_auth_method_t *) auth_methods.modules[i].module;
    }
    return NULL;
}


/* The following function is a hacked version of find_auth_method function.
Also return an integer (method_id) which corresponds to a hash key points
to an array with authenticators which can handle the authentication method.
 */

http_auth_method_t *find_auth_method_id(const char *method, int *method_id)
{
    int i;
    *method_id = 0;
    for (i = 0; i < auth_methods.modules_num; i++) {
        if (strcasecmp
                (method,
                 ((http_auth_method_t *) auth_methods.modules[i].module)->name) == 0) {
            *method_id = i;
            return (http_auth_method_t *) auth_methods.modules[i].module;
        }
    }
    return NULL;
}


authenticator_module_t *find_authenticator(const char *name)
{
    int i;
    for (i = 0; i < authenticators.modules_num; i++) {
        if (strcmp
                (name,
                 ((authenticator_module_t *) authenticators.modules[i].module)->name) ==
                0) {
            return (authenticator_module_t *) authenticators.modules[i].module;
        }
    }
    return NULL;
}


service_handler_module_t *find_servicehandler(const char *name)
{
    service_handler_module_t *sh;
    int i;
    for (i = 0; i < service_handlers.modules_num; i++) {
        sh = (service_handler_module_t *) service_handlers.modules[i].module;
        if (sh->name && strcmp(sh->name, name) == 0)
            return sh;
    }
    return NULL;
}

void *find_module(const char *name, int *type)
{
    void *mod;
    if ((mod = find_logger(name)) != NULL) {
        *type = LOGGER;
        return mod;
    }
    if ((mod = find_servicehandler(name)) != NULL) {
        *type = SERVICE_HANDLER;
        return mod;
    }
    if ((mod = find_access_controller(name)) != NULL) {
        *type = ACCESS_CONTROLLER;
        return mod;
    }
    if ((mod = find_auth_method(name)) != NULL) {
        *type = AUTH_METHOD;
        return mod;
    }
    if ((mod = find_authenticator(name)) != NULL) {
        *type = AUTHENTICATOR;
        return mod;
    }
    if ((mod = find_common(name)) != NULL) {
        *type = COMMON;
        return mod;
    }
    *type = UNKNOWN;
    return NULL;
}

/*All struct modules as first field have the name.*/
struct module_tmp_struct {
    char *name;
    void *other_data;
};

void *register_module(const char *module_file, const char *type, const char *argv[])
{
    void *module = NULL;
    int mod_type;
    struct modules_list *l = NULL;
    struct module_tmp_struct *check_mod;
    int check_mod_type;

    l = modules_lists_table[mod_type = module_type(type)];
    if (l == NULL)
        return NULL;

    int isStatic = 0;
    module = load_module(module_file, argv, &isStatic, mod_type);
    if (!module) {
        ci_debug_printf(3, "Error while loading  module %s\n",
                        module_file);
        return NULL;
    }

    check_mod = (struct module_tmp_struct *)module;
    if (find_module(check_mod->name, &check_mod_type) != NULL) {
        ci_debug_printf(1, "Error, the module %s is already loaded\n", check_mod->name);
        return NULL;
    }

    init_module(module, mod_type);

    add_to_modules_list(l, module, isStatic);
    return module;
}


service_handler_module_t *find_servicehandler_by_ext(const char *extension)
{
    service_handler_module_t *sh;
    const char *s;
    int i, len_extension, len_s = 0, found = 0;
    len_extension = strlen(extension);
    for (i = 0; i < service_handlers.modules_num; i++) {
        sh = (service_handler_module_t *) service_handlers.modules[i].module;
        s = sh->extensions;

        do {
            if ((s = strstr(s, extension)) != NULL) {
                len_s = strlen(s);
                if (len_s >= len_extension
                        && (strchr(",. \t", s[len_extension])
                            || s[len_extension] == '\0')) {
                    found = 1;
                }
            }
            if (!s || len_extension >= len_s)        /*There is no any more extensions....... */
                break;
            s += len_extension;
        } while (s && !found);

        if (found) {
            ci_debug_printf(3,
                            "Found handler %s for service with extension: %s\n",
                            sh->name, extension);
            return sh;
        }
    }

    ci_debug_printf(1, "No handler for extension %s. Using default ...\n",
                    extension);
    return default_service_handler;
}



/*************************************************************************************/

#define MAX_HASH_SIZE 256       /*Maybe, better a value of 10 or 20 */
struct auth_hash {
    authenticator_module_t ***hash;    /*A 2-d array which contains pointers to authenticator_module_t */
    int usedsize;
    int hash_size;
};

struct auth_hash authenticators_hash;

int init_auth_hash(struct auth_hash *hash)
{
    hash->usedsize = 0;
    if (NULL == (hash->hash = malloc(STEP * sizeof(authenticator_module_t **)))) {
        hash->hash_size = STEP;
        return 0;
    }
    hash->hash_size = STEP;
    memset(hash->hash, 0, hash->hash_size);
    return 1;
}

void release_auth_hash(struct auth_hash *hash)
{
    int i;
    for (i = 0; i < hash->hash_size; i++) {
        if (hash->hash[i] != NULL) {
            free(hash->hash[i]);
        }
    }
    free(hash->hash);
    hash->hash = NULL;
}


authenticator_module_t **get_authenticators_list(struct auth_hash *hash,
        int method_id)
{
    if (method_id > hash->hash_size)
        return NULL;
    return hash->hash[method_id];
}

int check_to_add_method_id(struct auth_hash *hash, int method_id)
{
    authenticator_module_t ***new_mem;
    if (method_id > MAX_HASH_SIZE || method_id < 0) {
        ci_debug_printf(1,
                        "Method id is %d. Possible bug, please report it to developers!!!!!!\n",
                        method_id);
        return 0;
    }

    while (hash->hash_size < method_id) {
        new_mem = realloc(hash->hash, hash->hash_size + STEP);
        if (!new_mem) {
            ci_debug_printf(1,
                            "Error allocating memory for authenticator hash!!!!!!\n");
            return 0;
        }
        memset(hash->hash + hash->hash_size, 0, STEP);       /*Reset the newly allocated memory */
        hash->hash = new_mem;
        hash->hash_size += STEP;
    }
    return 1;
}


int methods_authenticators(struct auth_hash *hash, const char *method_name,
                           int method_id, const char **argv)
{
    int i, k, auths_num;
    authenticator_module_t **new_mem, *auth_mod;

    if (!check_to_add_method_id(hash, method_id))
        return 0;

    for (auths_num = 0; argv[auths_num] != NULL; auths_num++);

    if (NULL ==
            (new_mem =
                 malloc((auths_num + 1) * sizeof(authenticator_module_t *)))) {
        ci_debug_printf(1, "Error allocating memory!!!!!!\n");
        return 0;
    }
    memset(new_mem, 0, auths_num + 1);
    if (hash->hash[method_id] != NULL)
        free(hash->hash[method_id]);
    hash->hash[method_id] = new_mem;

    k = 0;
    for (i = 0; i < auths_num; i++) {
        ci_debug_printf(3, "Authenticator %s......\n", argv[i]);
        if ((auth_mod = find_authenticator(argv[i])) == NULL) {
            ci_debug_printf(1, "Authenticator %s does not exist!!!!!\n",
                            argv[i]);
            continue;
        }
        if (strcasecmp(auth_mod->method, method_name) != 0) {
            ci_debug_printf(1,
                            "Authenticator %s does not provide authentication method %s!!!!\n",
                            auth_mod->name, method_name);
            continue;
        }
        new_mem[k++] = auth_mod;
    }
    new_mem[k] = NULL;
    return 1;
}


int set_method_authenticators(const char *method_name, const char **argv)
{
    int method_id;
    http_auth_method_t *method_mod;

    if (!(method_mod = find_auth_method_id(method_name, &method_id))) {
        ci_debug_printf(1, "Authentication method \"%s\" not supported\n",
                        method_name);
        return 0;
    }
    return methods_authenticators(&authenticators_hash, method_name, method_id,
                                  argv);
}


http_auth_method_t *get_authentication_schema(const char *method_name,
        authenticator_module_t ***
        authenticators)
{
    int method_id;
    http_auth_method_t *method_mod;
    if (!(method_mod = find_auth_method_id(method_name, &method_id))) {
        *authenticators = NULL;
        return NULL;
    }
    *authenticators = get_authenticators_list(&authenticators_hash, method_id);
    return method_mod;
}


/*************************************************************************************/


extern service_handler_module_t c_service_handler;
extern logger_module_t file_logger;
extern logger_module_t *default_logger;
extern access_control_module_t default_acl;
extern http_auth_method_t basic_auth;
extern authenticator_module_t basic_simple_db;


int init_modules()
{
    /*first initialize authenticators hash...... */
    init_auth_hash(&authenticators_hash);

    default_service_handler = &c_service_handler;
    add_to_modules_list(&service_handlers, default_service_handler, 1);

    init_module(&file_logger,LOGGER);
    add_to_modules_list(&loggers, &file_logger, 1);

    init_module(&default_acl, ACCESS_CONTROLLER);
    add_to_modules_list(&access_controllers, &default_acl, 1);

    init_module(&basic_auth, AUTH_METHOD);
    add_to_modules_list(&auth_methods, &basic_auth, 1);

    init_module(&basic_simple_db, AUTHENTICATOR);
    add_to_modules_list(&authenticators, &basic_simple_db, 1);

    return 1;
}


int post_init_modules()
{
    int i;

    /*     common modules */
    for (i = 0; i < common_modules.modules_num ; i++) {
        if (((common_module_t *) common_modules.modules[i].module)->
                post_init_module != NULL)
            ((common_module_t *) common_modules.modules[i].module)->
            post_init_module(&CI_CONF);
    }

    /*     service_handlers */
    for (i = 0; i < service_handlers.modules_num; i++) {
        if (((service_handler_module_t *) service_handlers.modules[i].module)->
                post_init_service_handler != NULL)
            ((service_handler_module_t *) service_handlers.modules[i].module)->
            post_init_service_handler(&CI_CONF);
    }

    /*     loggers? loggers do not have post init handlers .... */


    /*     access_controllers */
    for (i = 0; i < access_controllers.modules_num; i++) {
        if (((access_control_module_t *) access_controllers.modules[i].module)->
                post_init_access_controller != NULL)
            ((access_control_module_t *) access_controllers.modules[i].module)->
            post_init_access_controller(&CI_CONF);
    }



    /*     auth_methods */
    for (i = 0; i < auth_methods.modules_num; i++) {
        if (((http_auth_method_t *) auth_methods.modules[i].module)->
                post_init_auth_method != NULL)
            ((http_auth_method_t *) auth_methods.modules[i].module)->
            post_init_auth_method(&CI_CONF);
    }

    /*     authenticators */
    for (i = 0; i < authenticators.modules_num; i++) {
        if (((authenticator_module_t *) authenticators.modules[i].module)->
                post_init_authenticator != NULL)
            ((authenticator_module_t *) authenticators.modules[i].module)->
            post_init_authenticator(&CI_CONF);
    }

    return 1;
}

int access_reset();
void log_reset();

#define RELEASE_MOD_LIST(modlist, mod_type, RELEASE_HANDLER)            \
    for (i = 0; i < modlist.modules_num; i++) {                         \
    mod_type *mod = (mod_type *) modlist.modules[i].module;             \
    if (mod->RELEASE_HANDLER != NULL)                                   \
        mod->RELEASE_HANDLER();                                         \
    if (!(modlist.modules[i].flags & FLAG_MOD_STATIC)) {                \
        if (mod->conf_table)                                            \
            ci_cfg_conf_table_release(mod->conf_table);                 \
        free(mod);                                                      \
    }                                                                   \
    modlist.modules[i].module = NULL;                                   \
    }                                                                   \
    free(modlist.modules); modlist.modules = NULL; modlist.list_size = 0; modlist.modules_num=0;

int release_modules()
{
    int i;

    log_reset();               /*resetting logs- we are going to release loggers ... */
    access_reset();

    RELEASE_MOD_LIST(service_handlers, service_handler_module_t, release_service_handler);
    RELEASE_MOD_LIST(loggers, logger_module_t, log_close);
    RELEASE_MOD_LIST(access_controllers, access_control_module_t, release_access_controller);
    RELEASE_MOD_LIST(auth_methods, http_auth_method_t, close_auth_method);
    RELEASE_MOD_LIST(authenticators, authenticator_module_t, close_authenticator);
    RELEASE_MOD_LIST(common_modules, common_module_t, close_module);
    return 1;
}

common_module_t * ci_common_module_build(const char *name, int (*init_module)(struct ci_server_conf *server_conf), int (*post_init_module)(struct ci_server_conf *server_conf), void (*close_module)(), struct ci_conf_entry *conf_table)
{
    common_module_t *mod = malloc(sizeof(common_module_t));
    mod->name = name;
    mod->init_module = init_module;
    mod->post_init_module = post_init_module;
    mod->close_module = close_module;
    mod->conf_table = conf_table;
    return mod;
}
