/*
 *  Copyright (C) 2004-2009 Christos Tsantilas
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


#ifndef __TXT_FORMAT_H
#define   __TXT_FORMAT_H

#include "request.h"

struct ci_fmt_entry {
    const char *directive;
    const char *description;
    int (*format)(ci_request_t *req_data, char *buf, int len, char *param);
};

int ci_format_text(ci_request_t *req_data, const char *fmt, char *buffer, int len,
		   struct ci_fmt_entry *user_table);

#endif
