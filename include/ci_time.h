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

#ifndef __C_ICAP_CI_TIME_H
#define __C_ICAP_CI_TIME_H

#include "c-icap.h"
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32

typedef struct timespec ci_clock_time_t;

static inline void ci_clock_time_reset(ci_clock_time_t *t)
{
    static ci_clock_time_t zero = {0,0};
    *t = zero;
}


static inline int64_t ci_clock_time_diff_milli(ci_clock_time_t *tsstop, ci_clock_time_t *tsstart) {
    return
        ((int64_t)tsstop->tv_sec - (int64_t)tsstart->tv_sec) * 1000LL +
        ((int64_t)tsstop->tv_nsec - (int64_t)tsstart->tv_nsec) / 1000000LL;
}

static inline int64_t ci_clock_time_diff_micro(ci_clock_time_t *tsstop, ci_clock_time_t *tsstart) {
    return
        ((int64_t)tsstop->tv_sec - (int64_t)tsstart->tv_sec) * 1000000LL +
        ((int64_t)tsstop->tv_nsec - (int64_t)tsstart->tv_nsec) / 1000LL;
}

static inline int64_t ci_clock_time_diff_nano(ci_clock_time_t *tsstop, ci_clock_time_t *tsstart) {
    return
        ((int64_t)tsstop->tv_sec - (int64_t)tsstart->tv_sec) * 1000000000LL +
        ((int64_t)tsstop->tv_nsec - (int64_t)tsstart->tv_nsec);
}

static inline void ci_clock_time_get(ci_clock_time_t *t)
{
    clock_gettime(CLOCK_REALTIME, t);
}

#else /*_WIN32*/

typedef LARGE_INTEGER ci_clock_time_t;

#endif

#ifdef __cplusplus
}
#endif

#endif
