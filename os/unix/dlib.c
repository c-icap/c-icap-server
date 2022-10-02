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
#include "debug.h"
#include "dlib.h"

#ifdef HAVE_DLFCN_H

void *ci_module_load(const char *module_file, const char *default_path)
{
    char path[CI_MAX_PATH];
    void *handle;
    int requiredLen;
#if defined __CYGWIN__
     if (default_path && module_file[0] != '/' && module_file[0] != '\\' && module_file[1] != ':')
#else
    if (default_path && module_file[0] != '/')
#endif
        requiredLen = snprintf(path, CI_MAX_PATH, "%s/%s", default_path, module_file);
    else
        requiredLen = snprintf(path, sizeof(path), "%s", module_file);

    if (requiredLen >= CI_MAX_PATH) {
        ci_debug_printf(1, "Error: too long filename, truncated to '%s'\n", path);
        return NULL;
    }

    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);

    if (!handle) {
        char *error_str = dlerror ();
        ci_debug_printf(1, "Error loading module %s:%s\n", module_file, error_str);
        return NULL;
    }
    return handle;
}

void *ci_module_sym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}


int ci_module_unload(void *handle, const char *name)
{
    int ret;
    ret = dlclose(handle);
    if (ret == 1) {
        ci_debug_printf(1, "Error unloading module:%s\n", name);
        return 0;
    }
    return 1;
}

#endif
