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


#ifndef __DLIB_H
#define __DLIB_H

#include "c-icap.h"
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32
#define CI_DLIB_HANDLE void *
#else
#define CI_DLIB_HANDLE HMODULE
#endif

CI_DECLARE_FUNC(CI_DLIB_HANDLE) ci_module_load(char *module_file, char *default_path);
CI_DECLARE_FUNC(void *)         ci_module_sym(CI_DLIB_HANDLE handle,char *symbol);
CI_DECLARE_FUNC(int)            ci_module_unload(CI_DLIB_HANDLE handle,char *name);

/*Utility functions */
CI_DECLARE_FUNC(int)            ci_dlib_entry(char *name,char *file, CI_DLIB_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /*__DLIB_H*/
