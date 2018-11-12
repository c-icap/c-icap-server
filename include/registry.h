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

#ifndef __C_ICAP_REGISTRY_H
#define __C_ICAP_REGISTRY_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

CI_DECLARE_FUNC(int) ci_registry_create(const char *name);
CI_DECLARE_FUNC(void) ci_registry_clean();

CI_DECLARE_FUNC(int) ci_registry_iterate(const char *name, void *data, int (*fn)(void *data, const char *label, const void *));
CI_DECLARE_FUNC(int) ci_registry_add_item(const char *name, const char *label, const void *obj);
CI_DECLARE_FUNC(const void *) ci_registry_get_item(const char *name, const char *label);

CI_DECLARE_FUNC(int) ci_registry_get_id(const char *name);
CI_DECLARE_FUNC(int) ci_registry_id_iterate(int reg_id, void *data, int (*fn)(void *data, const char *label, const void *));
CI_DECLARE_FUNC(const void *) ci_registry_id_get_item(int reg_id, const char *label);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*__REGISTRY_H*/
