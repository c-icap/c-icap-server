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
#include "array.h"
#include "service.h"
#include "debug.h"

void ci_service_data_read_lock(ci_service_xdata_t * srv_xdata)
{
    ci_thread_rwlock_rdlock(&srv_xdata->lock);
}

void ci_service_data_read_unlock(ci_service_xdata_t * srv_xdata)
{
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_istag(ci_service_xdata_t * srv_xdata, const char *istag)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    snprintf(srv_xdata->ISTag, sizeof(srv_xdata->ISTag), "ISTag: \"%s%.*s\"", ISTAG, CI_SERVICE_ISTAG_SIZE, istag);
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_preview(ci_service_xdata_t * srv_xdata,
                                     const char *preview)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    strcpy(srv_xdata->TransferPreview, "Transfer-Preview: ");
    strncat(srv_xdata->TransferPreview, preview,
            MAX_HEADER_SIZE - sizeof("Transfer-Preview: "));
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_ignore(ci_service_xdata_t * srv_xdata,
                                    const char *ignore)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    strcpy(srv_xdata->TransferIgnore, "Transfer-Ignore: ");
    strncat(srv_xdata->TransferIgnore, ignore,
            MAX_HEADER_SIZE - sizeof("Transfer-Ignore: "));
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_complete(ci_service_xdata_t * srv_xdata,
                                      const char *complete)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    strcpy(srv_xdata->TransferComplete, "Transfer-Complete: ");
    strncat(srv_xdata->TransferComplete, complete,
            MAX_HEADER_SIZE - sizeof("Transfer-Complete: "));
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}


void ci_service_set_preview(ci_service_xdata_t * srv_xdata, int preview)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->preview_size = preview;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_enable_204(ci_service_xdata_t * srv_xdata)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->allow_204 = 1;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_enable_206(ci_service_xdata_t * srv_xdata)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    if (!srv_xdata->disable_206)
        srv_xdata->allow_206 = 1;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_max_connections(ci_service_xdata_t *srv_xdata, int max_connections)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->max_connections = max_connections;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_options_ttl(ci_service_xdata_t *srv_xdata, int ttl)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->options_ttl = ttl;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_xopts(ci_service_xdata_t * srv_xdata, uint64_t xopts)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->xopts = xopts;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_add_xopts(ci_service_xdata_t * srv_xdata, uint64_t xopts)
{
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    srv_xdata->xopts |= xopts;
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_add_xincludes(ci_service_xdata_t * srv_xdata,
                              char **xincludes)
{
    int len, i;
    if (!xincludes)
        return;
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    for (i = 0, len = 0; xincludes[i] != NULL && XINCLUDES_SIZE - len > 0; i++) {
         len += snprintf(srv_xdata->xincludes + len, XINCLUDES_SIZE - len, "%s%s", (len > 0 ? ", ": ""), xincludes[i]);
    }
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_add_option_handler(ci_service_xdata_t *srv_xdata, const char *name, int (*handler)(struct ci_request *))
{
    if (!handler)
        return;
    ci_thread_rwlock_wrlock(&srv_xdata->lock);
    if (!srv_xdata->option_handlers)
        srv_xdata->option_handlers = ci_list_create(1024, sizeof(struct ci_option_handler));
    struct ci_option_handler oh;
    strncpy(oh.name, name, sizeof(oh.name) - 1);
    oh.name[sizeof(oh.name) - 1] = '\0';
    oh.handler = handler;
    ci_list_push_back(srv_xdata->option_handlers, &oh);
    ci_thread_rwlock_unlock(&srv_xdata->lock);
}
