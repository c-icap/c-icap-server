/*
 *  Copyright (C) 2007,2010 Trever L. Adams
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Additionally, you may use this file under LGPL 2 or (at your option) later

#include <time.h>
#include "txt_format.h"

typedef struct {
	char *TEMPLATE_NAME;
	char *SERVICE_NAME;
	char *LANGUAGE;
	ci_membuf_t *data;
	time_t last_used;
	time_t loaded;
	time_t modified;
	int locked;
} txtTemplate_t;

#ifndef TXTTEMPLATE
extern ci_membuf_t *
ci_txt_template_build_content(const ci_request_t *req, const char *SERVICE_NAME, const char *TEMPLATE_NAME, struct ci_fmt_entry *user_table);
extern void ci_txt_template_reload(void);
extern int ci_txt_template_init(void);
extern void ci_txt_template_close(void);
#endif
