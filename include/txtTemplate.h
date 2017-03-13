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

#ifndef __TXTTEMPLATE_H
#define __TXTTEMPLATE_H

#include <time.h>
#include "request.h"
#include "txt_format.h"
#include "body.h"

#ifdef __cplusplus
extern "C"
{
#endif

CI_DECLARE_FUNC (ci_membuf_t *)
ci_txt_template_build_content(const ci_request_t *req,
                              const char *SERVICE_NAME,
                              const char *TEMPLATE_NAME, struct ci_fmt_entry *user_table);
CI_DECLARE_FUNC (void) ci_txt_template_reset(void);
CI_DECLARE_FUNC (int)  ci_txt_template_init(void);
CI_DECLARE_FUNC (void) ci_txt_template_close(void);
CI_DECLARE_FUNC (void) ci_txt_template_set_dir(const char *dir);
CI_DECLARE_FUNC (void) ci_txt_template_set_default_lang(const char *lang);

CI_DECLARE_DATA extern const char *TEMPLATE_DIR;
CI_DECLARE_DATA extern const char *TEMPLATE_DEF_LANG;
CI_DECLARE_DATA extern int TEMPLATE_RELOAD_TIME; // Default time is one hour, this variable is in seconds
CI_DECLARE_DATA extern int TEMPLATE_CACHE_SIZE; // How many templates can be cached
CI_DECLARE_DATA extern int TEMPLATE_MEMBUF_SIZE; // Max memory for txtTemplate to expand template into txt
#ifdef __cplusplus
}
#endif

#endif /*__TXTTEMPLATE_H*/
