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

#include "common.h"
#include "c-icap.h"
#include "array.h"
#include "debug.h"
#include "registry.h"

static ci_ptr_array_t *REGISTRIES = NULL;
static int32_t REG_ITEMS_COUNT = 0;

int ci_registry_create(const char *name)
{
    /*
      Build space for 1024 different registries.
      It should be enough.
     */
    if (!REGISTRIES)
        REGISTRIES = ci_ptr_array_new2(1024);
    else if (ci_ptr_array_search(REGISTRIES, name)) {
        ci_debug_printf(1, "Registry '%s' already exist!\n", name);
        return -1;
    }

    ci_ptr_dyn_array_t *registry = ci_ptr_dyn_array_new(1024);

    ci_ptr_array_add(REGISTRIES, name, registry);
    ci_debug_printf(4, "Registry '%s' added and is ready to store new registry entries\n", name);
    return (REGISTRIES->count - 1); /*Return the pos in the REGISTRIES array*/
}

void ci_registry_clean()
{
    ci_ptr_dyn_array_t *registry = NULL;
    char buf[1024];

    if (!REGISTRIES)
        return;

    while ((registry = (ci_ptr_dyn_array_t *)ci_ptr_array_pop_value(REGISTRIES, buf, sizeof(buf))) != NULL) {
        ci_debug_printf(4, "Registry %s removed\n", buf);
        ci_ptr_dyn_array_destroy(registry);
    }
    ci_ptr_array_destroy(REGISTRIES);
    REGISTRIES = NULL;
}

int ci_registry_iterate(const char *name, void *data, int (*fn)(void *data, const char *label, const void *))
{
    const ci_ptr_dyn_array_t *registry = NULL;
    if (!REGISTRIES || (registry = ci_ptr_array_search(REGISTRIES, name)) == NULL) {
        ci_debug_printf(1, "Registry '%s' does not exist!\n", name);
        return 0;
    }
    ci_ptr_dyn_array_iterate(registry, data, fn);
    return 1;
}

int ci_registry_add_item(const char *name, const char *label, const void *obj)
{
    ci_ptr_dyn_array_t *registry = NULL;
    if (!REGISTRIES || (registry = ci_ptr_array_search(REGISTRIES, name)) == NULL) {
        ci_debug_printf(3, "Registry '%s' does not exist create it\n", name);
        if (ci_registry_create(name) < 0)
            return 0;
        registry = ci_ptr_array_search(REGISTRIES, name);
    }
    if (ci_ptr_dyn_array_add(registry, label, (void *)obj))
        return ++REG_ITEMS_COUNT;

    return 0;
}

const void * ci_registry_get_item(const char *name, const char *label)
{
    ci_ptr_dyn_array_t *registry = NULL;
    if (!REGISTRIES || (registry = ci_ptr_array_search(REGISTRIES, name)) == NULL) {
        ci_debug_printf(1, "Registry '%s' does not exist!\n", name);
        return NULL;
    }

    return ci_ptr_dyn_array_search(registry, label);
}

struct check_reg_data {
    const char *name;
    int found;
    int count;
};

static int check_reg(void *data, const char *name, const void *val)
{
    struct check_reg_data *rdata = (struct check_reg_data *) data;
    rdata->count++;
    if (strcmp(rdata->name, name) == 0) {
        rdata->found = 1;
        return 1; /*Found the registry, return !=0 to stop iteration*/
    }
    return 0;
}

int ci_registry_get_id(const char *name)
{
    struct check_reg_data rdata;
    rdata.name = name;
    rdata.found = 0;
    rdata.count = 0;

    if (REGISTRIES)
        ci_ptr_array_iterate(REGISTRIES, &rdata, check_reg);

    if (rdata.found)
        return (rdata.count - 1);
    else
        return -1;
}

int ci_registry_id_iterate(int reg_id, void *data, int (*fn)(void *data, const char *label, const void *))
{
    const ci_ptr_dyn_array_t *registry = NULL;
    const ci_array_item_t *ai;
    if (!REGISTRIES || (ai = ci_ptr_array_get_item(REGISTRIES, reg_id)) == NULL || (registry = ai->value) == NULL) {
        ci_debug_printf(1, "Registry with id='%d' does not exist!\n", reg_id);
        return 0;
    }
    ci_ptr_dyn_array_iterate(registry, data, fn);
    return 1;
}

const void * ci_registry_id_get_item(int reg_id, const char *label)
{
    const ci_ptr_dyn_array_t *registry = NULL;
    const ci_array_item_t *ai;
    if (!REGISTRIES || (ai = ci_ptr_array_get_item(REGISTRIES, reg_id)) == NULL || (registry = ai->value) == NULL) {
        ci_debug_printf(1, "Registry with id='%d' does not exist!\n", reg_id);
        return 0;
    }
    return ci_ptr_dyn_array_search(registry, label);
}
