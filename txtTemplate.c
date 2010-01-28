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

#define TXTTEMPLATE

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <assert.h>
#include <stdlib.h>

#include "body.h"
#include "c-icap.h"
#include "service.h"
#include "header.h"
#include "request.h"
#include "debug.h"
#include "txtTemplate.h"
#include "simple_api.h"

char *TEMPLATE_DIR = NULL;
char *TEMPLATE_DEF_LANG = NULL;
int TEMPLATE_RELOAD_TIME = 360; // Default time is one hour, this variable is in seconds

txtTemplate_t *templates = NULL;
int txtTemplateInited = 0;

int TEMPLATE_CACHE_SIZE = 20; // How many templates can be cached
int TEMPLATE_MEMBUF_SIZE = 8192; // Max memory for txtTemplate to expand template into txt

static ci_thread_mutex_t templates_mutex;


// Caller should free returned pointer
static char *makeTemplatePathFileName(const char *service_name, const char *page_name, const char *lang)
{
     char *path;
     if((path = malloc(MAXPATHLEN)) == NULL)
     {
          ci_debug_printf(1, "Unable to allocate memory in txtTemplate.c for template path/file name\n");
          return NULL;
     }

     snprintf(path, MAXPATHLEN, "%s/%s/%s/%s", TEMPLATE_DIR, service_name, lang,
              page_name);
     return path;
}

int ci_txt_template_init(void)
{
     int i;
     templates = malloc(TEMPLATE_CACHE_SIZE * sizeof(txtTemplate_t));
     if(templates == NULL)
     {
          ci_debug_printf(1, "Unable to allocate memory in in inittxtTemplate for template storage!\n");
          return -1;
     }
     for (i = 0; i < TEMPLATE_CACHE_SIZE; i++) {
          // The following three elements are critical to be cleared,
          // the rest can be left unintialized
          templates[i].data = NULL;
          templates[i].loaded = 0;
          templates[i].locked = 0;
     }
     txtTemplateInited = 1;
     ci_thread_mutex_init(&templates_mutex);
     return 1;
}

//templates_mutex should be locked by caller!
static void templateFree(txtTemplate_t *template)
{
     assert(template != NULL);
     if (template->data == NULL)
          return;
     if (template->TEMPLATE_NAME)
          free(template->TEMPLATE_NAME);
     if (template->SERVICE_NAME)
          free(template->SERVICE_NAME);
     if (template->LANGUAGE)
          free(template->LANGUAGE);
     template->TEMPLATE_NAME = template->SERVICE_NAME = template->LANGUAGE = NULL;
     ci_membuf_free(template->data);
     template->data = NULL;
}

static void template_release(txtTemplate_t *template)
{
     ci_thread_mutex_lock(&templates_mutex);
     template->locked--;
     if(template->locked < 0) template->locked = 0;
     ci_thread_mutex_unlock(&templates_mutex);
}

//templates_mutex should not be locked by caller!
void ci_txt_template_reload(void)
{
     int i = 0;
     ci_thread_mutex_lock(&templates_mutex);
     for (i = 0; i < TEMPLATE_CACHE_SIZE; i++) {
          templateFree(&templates[i]);
     }
     ci_thread_mutex_unlock(&templates_mutex);
}

void ci_txt_template_close(void)
{
     ci_thread_mutex_lock(&templates_mutex);
     if(templates) free(templates);
     templates = NULL;
     ci_thread_mutex_unlock(&templates_mutex);
     ci_thread_mutex_destroy(&templates_mutex);
}

//templates_mutex should be locked by caller!
static txtTemplate_t *templateFind(const char *SERVICE_NAME, const char *TEMPLATE_NAME,
                      const char *LANGUAGE)
{
     int i = 0;
     struct stat file;
     char *path = NULL;
     time_t current_time;
     // We don't lock here as it should be locked elsewhere
     for (i = 0; i < TEMPLATE_CACHE_SIZE; i++)
     {
          if (templates[i].data != NULL)
          {
               if (strcmp(templates[i].SERVICE_NAME, SERVICE_NAME) == 0
                   && strcmp(templates[i].TEMPLATE_NAME, TEMPLATE_NAME) == 0
                   && strcmp(templates[i].LANGUAGE, LANGUAGE) == 0)
               {
                    time(&current_time);
                    if(current_time - templates[i].loaded >= TEMPLATE_RELOAD_TIME)
                    {
                         path = makeTemplatePathFileName(SERVICE_NAME, TEMPLATE_NAME, LANGUAGE);
                         if(path)
                         {
                              stat(path, &file);
                              if(file.st_mtime > templates[i].modified)
                              {
                                    ci_debug_printf(4,
                                          "templateFind: found: %s, %s, %s updated on disk, reloading.\n",
                                          SERVICE_NAME, LANGUAGE, TEMPLATE_NAME);
                                    templateFree(&templates[i]);
                                    free(path);
                                    return NULL;
                              }
                              free(path);
                         }
                         goto RETURN_CACHED;
                    }
                    else {
                         RETURN_CACHED:
                         ci_debug_printf(4,
                                         "templateFind: found: %s, %s, %s in cache at index %d\n",
                                         SERVICE_NAME, LANGUAGE, TEMPLATE_NAME, i);
                         templates[i].last_used = current_time;
                         return &templates[i];
                    }
               }
          }
     }
     return NULL;
}

//templates_mutex should be locked by caller!
static txtTemplate_t *templateFindFree(void)
{
     time_t oldest = 0;
     txtTemplate_t *useme = NULL;
     int i = 0;
     // We don't lock here as it should be locked elsewhere
     // First we try to find an unused template slot
     for (i = 0; i < TEMPLATE_CACHE_SIZE; i++)
          if (templates[i].data == NULL)
               return &templates[i];
     // We didn't find one, so look for most unused
     for (i = 0; i < TEMPLATE_CACHE_SIZE; i++) {
          if (templates[i].last_used < oldest && templates[i].locked <= 0) {
               oldest = templates[i].last_used;
               useme = &templates[i];
          }
     }
     if (useme != NULL)
          if (useme->data != NULL)
               templateFree(useme);
     return useme;
}

static txtTemplate_t *templateTryLoadText(const ci_request_t * req, const char *service_name,
                              const char *page_name, const char *lang)
{
     int fd;
     char *path;
     char buf[4096];
     struct stat file;
     ssize_t len;
     ci_membuf_t *textbuff = NULL;
     txtTemplate_t *tempTemplate = NULL;

     // Protect the template cache structure
     ci_thread_mutex_lock(&templates_mutex);

     tempTemplate = templateFind(service_name, page_name, lang);
     if (tempTemplate != NULL)
     {
          tempTemplate->locked++;
          ci_thread_mutex_unlock(&templates_mutex); // unlock the templates structure
          return tempTemplate;
     }
     ci_thread_mutex_unlock(&templates_mutex); // We didn't go into the if, release the lock

     path = makeTemplatePathFileName(service_name, page_name, lang);

     ci_debug_printf(9, "templateTryLoadText: %s\n", path);

     fd = open(path, O_RDONLY);

     if (fd < 0) {
          ci_debug_printf(4, "templateTryLoadText: '%s': %s\n", path,
                          strerror(errno));
          if(path) free(path);
          return NULL;
     }

     fstat(fd, &file);

     // Protect the template cache structure
     ci_thread_mutex_lock(&templates_mutex);
     // Find free template
     tempTemplate = templateFindFree();
     if (tempTemplate != NULL) {
          textbuff = ci_membuf_new_sized(file.st_size + 1);

          assert(textbuff != NULL);

          while ((len = read(fd, buf, sizeof(buf))) > 0) {
               ci_membuf_write(textbuff, buf, len, 0);
          }
          ci_membuf_write(textbuff, "\0", 1, 1);     // terminate the string for safety

          if (len < 0) {
               ci_debug_printf(4, "templateTryLoadText: failed to fully read: '%s': %s\n",
                               path, strerror(errno));
          }

          tempTemplate->SERVICE_NAME = strdup(service_name);
          tempTemplate->TEMPLATE_NAME = strdup(page_name);
          tempTemplate->LANGUAGE = strdup(lang);
          tempTemplate->data = textbuff;
          tempTemplate->loaded = time(NULL);
          tempTemplate->modified = file.st_mtime;
          tempTemplate->locked++;
          time(&tempTemplate->last_used);
     }
     else
          ci_debug_printf(4, "templateTryLoadText: Unable to find free template slot.\n");
     // Unlock the template cache structure
     ci_thread_mutex_unlock(&templates_mutex);

     close(fd);

     if(path) free(path);

     return tempTemplate;
}

static txtTemplate_t *templateLoadText(const ci_request_t * req, const char *service_name,
                           const char *page_name)
{
     char *languages = NULL;
     char *str = NULL, *preferred = NULL;
     txtTemplate_t *template = NULL;
     if ((languages = ci_http_request_get_header(req, "Accept-Language")) != NULL) {
          ci_debug_printf(4, "templateLoadText: Languages are: '%s'\n", languages);
          str = strchr(languages, ';');
          if (str != NULL)
               str[0] = '\0';
          preferred = languages;
          while ((str = strchr(preferred, ',')) != NULL) {
               str[0] = '\0';
               if (preferred != '\0') {
                    ci_debug_printf(4,
                                    "templateLoadText: trying preferred language: '%s'\n",
                                    preferred);
                    template =
                        templateTryLoadText(req, service_name, page_name, preferred);
                    if (template != NULL) {
                         return template;
                    }
// This is a bad idea, as currently implemented it allows frequent disk accesses.
// Symlinks en_GB -> en, en_US->en, etc. are probably the right answer. On
// thinking about it, these shouldn't trash the cash to badly.
/*                    else {
                         str2 = strchr(preferred, '-');
                         if(str2) str2[0] = '\0';
                         ci_debug_printf(4,
                                         "templateLoadText: trying base of preferred language: '%s'\n",
                                         preferred);
                         template =
                             templateTryLoadText(req, service_name, page_name, preferred);
                         if (template != NULL) {
                              return template;
                         }
                    } */
               }
               preferred = str + 1;
          }
     }
     ci_debug_printf(4, "templateLoadText: Accept-Language header found or was empty!\n");

     return templateTryLoadText(req, service_name, page_name, TEMPLATE_DEF_LANG);
}

// Caller should release the returned buffer when they have finished with it.
ci_membuf_t *ci_txt_template_build_content(const ci_request_t *req, const char *SERVICE_NAME,
                               const char *TEMPLATE_NAME, const struct ci_fmt_entry *user_table)
{
     ci_membuf_t *content = ci_membuf_new(TEMPLATE_MEMBUF_SIZE);
     char *tempbuf = NULL;
     txtTemplate_t *template = NULL;

     /*templateLoadText also locks the template*/
     template = templateLoadText(req, SERVICE_NAME, TEMPLATE_NAME);
     if (template)
     {
          content->endpos = ci_format_text(req, template->data->buf, content->buf, content->bufsize, user_table);
          template_release(template);

          ci_membuf_write(content, "\0", 1, 1);      // terminate the string for safety
     }
     else {
          tempbuf = makeTemplatePathFileName(SERVICE_NAME, TEMPLATE_NAME, TEMPLATE_DEF_LANG);
          content->endpos = snprintf(content->buf, content->bufsize, "ERROR: Unable to find specified template: %s\n", tempbuf);
          ci_debug_printf(1, "ERROR: Unable to find specified template: %s\n", tempbuf);
          free(tempbuf);
     }

     return content;
}

