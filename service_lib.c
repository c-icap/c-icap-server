/*
 *  Copyright (C) 2004 Christos Tsantilas
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


#include "c-icap.h"
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

void ci_service_set_istag(ci_service_xdata_t * srv_xdata, char *istag)
{
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     strncpy(srv_xdata->ISTag + SRV_ISTAG_POS, istag,
             SRV_ISTAG_SIZE - SRV_ISTAG_POS);
     srv_xdata->ISTag[SRV_ISTAG_SIZE] = '\0';
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_preview(ci_service_xdata_t * srv_xdata,
                                     char *preview)
{
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     strcpy(srv_xdata->TransferPreview, "Transfer-Preview: ");
     strncat(srv_xdata->TransferPreview, preview,
             MAX_HEADER_SIZE - sizeof("Transfer-Preview: "));
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_ignore(ci_service_xdata_t * srv_xdata,
                                    char *ignore)
{
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     strcpy(srv_xdata->TransferIgnore, "Transfer-Ignore: ");
     strncat(srv_xdata->TransferIgnore, ignore,
             MAX_HEADER_SIZE - sizeof("Transfer-Ignore: "));
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_set_transfer_complete(ci_service_xdata_t * srv_xdata,
                                      char *complete)
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


void ci_service_set_xopts(ci_service_xdata_t * srv_xdata, int xopts)
{
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     srv_xdata->xopts = xopts;
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_add_xopts(ci_service_xdata_t * srv_xdata, int xopts)
{
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     srv_xdata->xopts |= xopts;
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}

void ci_service_add_xincludes(ci_service_xdata_t * srv_xdata,
                              char **xincludes)
{
     int len, i;
     len = 0;
     i = 0;
     if (!xincludes)
          return;
     ci_thread_rwlock_wrlock(&srv_xdata->lock);
     while (XINCLUDES_SIZE - len - 2 > 0 && xincludes[i]) {
          if (len) {
               strcat(srv_xdata->xincludes, ", ");
               len += 2;
          }
          strncat(srv_xdata->xincludes, xincludes[i], XINCLUDES_SIZE - len);
          len += strlen(xincludes[i]);
          i++;
     }
     ci_thread_rwlock_unlock(&srv_xdata->lock);
}
