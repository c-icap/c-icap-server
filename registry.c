/*
 *  Copyright (C) 2013 Christos Tsantilas
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

#include "array.h"
#include "debug.h"
#include "registry.h"

ci_ptr_array_t *registries = NULL;

int ci_registry_create(const char *name)
{
    /*
      Build space for 1024 different registries.
      It should be enough.
     */
    if (!registries)
        registries = ci_ptr_array_new2(1024);
    else if (ci_ptr_array_search(registries, name)) {
        ci_debug_printf(1, "Registry '%s' already exist!\n", name);
        return -1;
    }

    ci_ptr_dyn_array_t *registry = ci_ptr_dyn_array_new(1024);

    ci_ptr_dyn_array_add(registry, name, registry);
    ci_debug_printf(4, "Registry '%s' added and is ready to store new registry entries\n", name);
    return (registries->count - 1); /*Return the pos in the registries array*/
}

void ci_registry_clean()
{
    ci_ptr_dyn_array_t *registry = NULL;
    char buf[1024];
    while((registry = (ci_ptr_dyn_array_t *)ci_ptr_array_pop_value(registries, buf, sizeof(buf))) != NULL) {
        ci_debug_printf(4, "Registry %s removed\n", buf);
        ci_ptr_dyn_array_destroy(registry);
    }
    ci_ptr_array_destroy(registries);
    registries = NULL;
}

int ci_registry_iterate(const char *name, void *data, int (*fn)(void *data, const char *label, const void *))
{
    const ci_ptr_dyn_array_t *registry = NULL;
    if ((registry = ci_ptr_array_search(registries, name)) == NULL) {
        ci_debug_printf(1, "Registry '%s' does not exist!\n", name);
        return 0;
    }
    ci_ptr_dyn_array_iterate(registry, data, fn);
    return 1;
}

int ci_registry_add_item(const char *name, const char *label, const void *obj)
{
    ci_ptr_dyn_array_t *registry = NULL;
    if ((registry = ci_ptr_array_search(registries, name)) == NULL) {
        ci_debug_printf(1, "Registry '%s' does not exist!\n", name);
        return 0;
    }
    if (ci_ptr_dyn_array_add(registry, label, (void *)obj))
        return 1;

    return 0;
}

const void * ci_registry_get_item(const char *name, const char *label)
{
    ci_ptr_dyn_array_t *registry = NULL;
    if ((registry = ci_ptr_array_search(registries, name)) == NULL) {
        ci_debug_printf(1, "Registry '%s' does not exist!\n", name);
        return NULL;
    }

    return ci_ptr_dyn_array_search(registry, label);
}


