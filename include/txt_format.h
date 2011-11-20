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

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \defgroup FORMATING Text formating api
 * \ingroup API
 * Functions and structures used for text formating
 */

/**
 * \brief This structure used to implement text formating directives
 * \ingroup FORMATING
 */
struct ci_fmt_entry {
    /**
     * \brief The formating directive (eg "%a")
     */
    const char *directive;

    /**
     * \brief A short description
     */
    const char *description;

    /**
     * \brief Pointer to the function which implements the text formating for this directive
     * \param req_data Pointer to the current request structure
     * \param buf The output buffer
     * \param len The length of the buffer
     * \param param Parameter of the directive
     * \return Non zero on success, zero on error
     */
    int (*format)(ci_request_t *req_data, char *buf, int len, const char *param);
};

/**
 * \brief Produces formated text based on template text.
 * \ingroup FORMATING
 * \param req_data The current request
 * \param fmt The format string
 * \param buffer The output buffer
 * \param len The length of the output buffer
 * \param user_table An array of user defined directives
 * \return Non zero on success, zero on error
 *
 * This function uses the internal formating directives table. Also the user can define his own table.
 */
int ci_format_text(ci_request_t *req_data, const char *fmt, char *buffer, int len,
		   struct ci_fmt_entry *user_table);

#ifdef __cplusplus
}
#endif

#endif
