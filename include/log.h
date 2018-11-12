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


#ifndef __C_ICAP_LOG_H
#define __C_ICAP_LOG_H

#include "request.h"

#ifdef __cplusplus
extern "C"
{
#endif

int log_open();
void log_close();
void log_reset();
void log_flush();

void log_access(ci_request_t *req,int status);
void log_server(ci_request_t *req, const char *format, ... );
void vlog_server(ci_request_t *req, const char *format, va_list ap);

/* The followings can be used by modules */
CI_DECLARE_FUNC(char *) logformat_fmt(const char *name);
#ifdef __cplusplus
}
#endif

#endif
