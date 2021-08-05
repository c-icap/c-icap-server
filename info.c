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

#include "common.h"
#include "c-icap.h"
#include "commands.h"
#include "service.h"
#include "header.h"
#include "body.h"
#include "request_util.h"
#include "server.h"
#include "stats.h"
#include "proc_threads_queues.h"
#include "debug.h"

int info_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf);
int info_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *);
int info_end_of_data_handler(ci_request_t * req);
void *info_init_request_data(ci_request_t * req);
void info_close_service();
void info_release_request_data(void *data);
int info_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req);

CI_DECLARE_MOD_DATA ci_service_module_t info_service = {
    "info",                         /* mod_name, The module name */
    "C-icap run-time information",            /* mod_short_descr,  Module short description */
    ICAP_REQMOD,                    /* mod_type, The service type is request modification */
    info_init_service,              /* mod_init_service. Service initialization */
    NULL,                           /* post_init_service. Service initialization after c-icap
                    configured. Not used here */
    info_close_service,           /* mod_close_service. Called when service shutdowns. */
    info_init_request_data,         /* mod_init_request_data */
    info_release_request_data,      /* mod_release_request_data */
    info_check_preview_handler,     /* mod_check_preview_handler */
    info_end_of_data_handler,       /* mod_end_of_data_handler */
    info_io,                        /* mod_service_io */
    NULL,
    NULL
};

struct time_range_stats {
    int requests_per_sec;
    int used_servers;
    int max_servers;
    int children;
};

struct info_time_stats {
    struct time_range_stats _1sec;
    struct time_range_stats _1min;
    struct time_range_stats _5min;
    struct time_range_stats _30min;
    struct time_range_stats _60min;
};

struct info_req_data {
    ci_membuf_t *body;
    int txt_mode;
    int childs;
    int *child_pids;
    int free_servers;
    int used_servers;
    unsigned int closing_childs;
    int *closing_child_pids;
    unsigned int started_childs;
    unsigned int closed_childs;
    unsigned int crashed_childs;
    ci_stat_memblock_t *collect_stats;
    struct info_time_stats *info_time_stats;
};

extern struct childs_queue *childs_queue;
extern ci_proc_mutex_t accept_mutex;

int InfoSharedMemId = -1;

static int build_statistics(struct info_req_data *info_data);
static void info_monitor_init_cmd(const char *name, int type, void *data);
static void info_monitor_periodic_cmd(const char *name, int type, void *data);

int info_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf)
{
    ci_service_set_xopts(srv_xdata,  CI_XAUTHENTICATEDUSER|CI_XAUTHENTICATEDGROUPS);
    InfoSharedMemId = ci_server_shared_memblob_register("InfoSharedData", sizeof(struct info_time_stats));
    ci_command_register_action("info::mon_start", CI_CMD_MONITOR_START, NULL,
                               info_monitor_init_cmd);
    ci_command_register_action("info::monitor_periodic", CI_CMD_MONITOR_ONDEMAND, NULL,
                               info_monitor_periodic_cmd);
    return CI_OK;
}

void info_close_service()
{
    ci_debug_printf(5,"Service %s shutdown!\n", info_service.mod_name);
}

void *info_init_request_data(ci_request_t * req)
{
    struct info_req_data *info_data;

    info_data = malloc(sizeof(struct info_req_data));

    info_data->body = ci_membuf_new();
    info_data->childs = 0;
    info_data->child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->free_servers = 0;
    info_data->used_servers = 0;
    info_data->closing_childs = 0;
    info_data->closing_child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->started_childs = 0;
    info_data->closed_childs = 0;
    info_data->crashed_childs = 0;
    info_data->txt_mode = 0;
    if (req->args[0] != '\0') {
        if (strstr(req->args, "view=text"))
            info_data->txt_mode = 1;
    }

    void *mem = malloc(ci_stat_memblock_size());
    info_data->collect_stats = mem ? ci_stat_memblock_init(mem, ci_stat_memblock_size()) : NULL;
    if (!info_data->collect_stats) {
        info_release_request_data((void *)info_data);
        info_data = NULL;
    }

    info_data->info_time_stats = ci_server_shared_memblob(InfoSharedMemId);
    return info_data;
}

void info_release_request_data(void *data)
{
    struct info_req_data *info_data = (struct info_req_data *)data;

    if (info_data->body)
        ci_membuf_free(info_data->body);

    if (info_data->child_pids)
        free(info_data->child_pids);

    if (info_data->closing_child_pids)
        free(info_data->closing_child_pids);

    if (info_data->collect_stats)
        free(info_data->collect_stats);

    free(info_data);
}


int info_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t * req)
{
    struct info_req_data *info_data = ci_service_data(req);

    if (ci_req_hasbody(req))
        return CI_MOD_ALLOW204;

    ci_req_unlock_data(req);

    ci_http_response_create(req, 1, 1); /*Build the responce headers */

    ci_http_response_add_header(req, "HTTP/1.0 200 OK");
    ci_http_response_add_header(req, "Server: C-ICAP");
    ci_http_response_add_header(req, "Content-Type: text/html");
    ci_http_response_add_header(req, "Content-Language: en");
    ci_http_response_add_header(req, "Connection: close");
    if (info_data->body) {
        build_statistics (info_data);
    }

    return CI_MOD_CONTINUE;
}

int info_end_of_data_handler(ci_request_t * req)
{
    return CI_MOD_DONE;
}

int info_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
            ci_request_t * req)
{
    int ret;
    struct info_req_data *info_data = ci_service_data(req);
    ret = CI_OK;

    if (wbuf && wlen) {
        if (info_data->body)
            *wlen = ci_membuf_read(info_data->body, wbuf, *wlen);
        else
            *wlen = CI_EOF;
    }

    return ret;
}

/*Statistisc implementation .....*/

void fill_queue_statistics(struct childs_queue *q, struct info_req_data *info_data)
{

    int i;
    int requests = 0;
    int32_t used_servers;
    ci_stat_memblock_t *stats;
    struct server_statistics *srv_stats;
    if (!q->childs)
        return;

    /*Merge childs data*/
    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid != 0 && q->childs[i].to_be_killed == 0) {
            if (info_data->child_pids)
                info_data->child_pids[info_data->childs] = q->childs[i].pid;
            info_data->childs++;
            ci_atomic_load_i32(&q->childs[i].usedservers, &used_servers);
            info_data->free_servers += (q->childs[i].servers - used_servers);
            info_data->used_servers += used_servers;
            requests += q->childs[i].requests;

            stats = q->stats_area + i * (q->stats_block_size);
            assert(ci_stat_memblock_check(stats));
            ci_stat_memblock_merge(info_data->collect_stats, stats);
        } else if (q->childs[i].pid != 0 && q->childs[i].to_be_killed) {
            if (info_data->closing_child_pids)
                info_data->closing_child_pids[info_data->closing_childs] = q->childs[i].pid;
            info_data->closing_childs++;
        }
    }
    /*Merge history data*/
    stats = q->stats_area + q->size * q->stats_block_size;
    assert(ci_stat_memblock_check(stats));
    ci_stat_memblock_merge(info_data->collect_stats, stats);

    srv_stats =
        (struct server_statistics *)(q->stats_area + q->size * q->stats_block_size + q->stats_block_size);
    /*Compute server statistics*/
    info_data->started_childs = srv_stats->started_childs;
    info_data->closed_childs = srv_stats->closed_childs;
    info_data->crashed_childs = srv_stats->crashed_childs;
}

struct stats_tmpl {
    char *simple_table_start;
    char *simple_table_end;
    char *simple_table_item_int;
    char *simple_table_item_kbs;
    char *simple_table_item_str;
    char *sep;
};

struct stats_tmpl txt_tmpl = {
    "\n%s Statistics\n==================\n",
    "",
    "%s : %llu\n",
    "%s : %llu Kbs %u bytes\n",
    "%s : %s\n",
    ", "
};

struct stats_tmpl html_tmpl = {
    "<H1>%s Statistics</H1>\n<TABLE>",
    "</TABLE>",
    "<TR><TH>%s:</TH><TD>  %llu</TD>\n",
    "<TR><TH>%s:</TH><TD>  %llu Kbs %u bytes</TD>\n",
    "<TR><TH>%s:</TH><TD>  %s</TD>\n",
    "<BR>"
};

static int print_statistics(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[1024];
    int sz;
    ci_kbs_t kbs;
    assert(stat);
    assert(label);
    assert(data);
    struct info_req_data *info_data = (struct info_req_data *)data;
    struct stats_tmpl *tmpl = info_data->txt_mode ? &txt_tmpl : &html_tmpl;
    switch (stat->type) {
    case CI_STAT_INT64_T:
        sz = snprintf(buf, sizeof(buf),
                      tmpl->simple_table_item_int,
                      label,
                      ci_stat_memblock_get_counter(info_data->collect_stats, id));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        break;
    case CI_STAT_KBS_T:
        kbs = ci_stat_memblock_get_kbs(info_data->collect_stats, id);
        sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_kbs,
                      label,
                      ci_kbs_kilobytes(&kbs),
                      ci_kbs_remainder_bytes(&kbs));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        break;
    default:
        break;
    }
    return 0;
}

static int print_group_statistics(void *data, const char *grp_name, int group_id)
{
    char buf[1024];
    int sz;
    struct info_req_data *info_data = (struct info_req_data *)data;
    struct stats_tmpl *tmpl = info_data->txt_mode ? &txt_tmpl : &html_tmpl;
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, grp_name);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    ci_stat_statistics_iterate(data, group_id, print_statistics);
    sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
    return 0;
}


static void build_running_servers_statistics(struct info_req_data *info_data)
{
    char buf[1024];
    int sz, i;
    struct stats_tmpl *tmpl;
    ci_membuf_t *tmp_membuf = NULL;

    if (info_data->txt_mode)
        tmpl = &txt_tmpl;
    else
        tmpl = &html_tmpl;

    assert(info_data->body);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, "Running Servers");
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Children number", info_data->childs);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Free Servers", info_data->free_servers);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Used Servers", info_data->used_servers);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Started Processes", info_data->started_childs);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Closed Processes", info_data->closed_childs);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Crashed Processes", info_data->crashed_childs);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Closing Processes", info_data->closing_childs);
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    /*Children pids*/
    tmp_membuf = ci_membuf_new_sized(4096);
    assert(ci_membuf_set_flag(tmp_membuf,  CI_MEMBUF_NULL_TERMINATED) != 0);
    for (i = 0; i < info_data->childs; i++) {
        sz = snprintf(buf, sizeof(buf), "%d ", info_data->child_pids[i]);
        ci_membuf_write(tmp_membuf, buf, sz, 0);
    }
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_str, "Children pids", ci_membuf_raw(tmp_membuf));
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    /*Closing children pids*/
    ci_membuf_truncate(tmp_membuf, 0);
    for (i = 0; i < info_data->closing_childs; i++) {
        sz = snprintf(buf, sizeof(buf), "%d ", info_data->closing_child_pids[i]);
        ci_membuf_write(tmp_membuf, buf, sz, 0);
    }
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_str, "Closing Children pids", ci_membuf_raw(tmp_membuf));
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    /*Print inter-process semaphores*/
    /*TODO: add mechanism to list all created interprocess semaphores not only the following two*/
    ci_membuf_truncate(tmp_membuf, 0);
    sz = accept_mutex.scheme->proc_mutex_print_info(&accept_mutex, buf, sizeof(buf));
    ci_membuf_write(tmp_membuf, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);
    ci_membuf_write(tmp_membuf, tmpl->sep, strlen(tmpl->sep), 0);

    sz = accept_mutex.scheme->proc_mutex_print_info(&childs_queue->queue_mtx, buf, sizeof(buf));
    ci_membuf_write(tmp_membuf, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_str, "Semaphores in use", ci_membuf_raw(tmp_membuf));
    ci_membuf_write(info_data->body, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);

    /*Shared mem info*/
    /*Add mechanism to list all allocated shared mem-blocks*/
    ci_membuf_truncate(tmp_membuf, 0);
    if (childs_queue->shmid.scheme) {
        sz = childs_queue->shmid.scheme->shared_mem_print_info(&childs_queue->shmid, buf, sizeof(buf));
        ci_membuf_write(tmp_membuf, buf, sz < sizeof(buf) ? sz : sizeof(buf), 0);
    }
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_str, "Shared mem-blocks in use", ci_membuf_raw(tmp_membuf));
    ci_membuf_write(info_data->body,buf, sz, 0);

    ci_membuf_write(info_data->body, tmpl->simple_table_end, strlen(tmpl->simple_table_end), 0);
    ci_membuf_free(tmp_membuf);
    tmp_membuf = NULL;
}

static int build_statistics(struct info_req_data *info_data)
{
    char buf[1024];
    int sz, k;
    struct stats_tmpl *tmpl;

    if (info_data->txt_mode)
        tmpl = &txt_tmpl;
    else
        tmpl = &html_tmpl;

    if (!info_data->body)
        return 0;

    fill_queue_statistics(childs_queue, info_data);
    build_running_servers_statistics(info_data);
    if (info_data->info_time_stats) {
        struct {
            const char *label;
            struct time_range_stats *v;
        } time_servers[5] = {{"Current", &info_data->info_time_stats->_1sec},
                             {"Last 1 minute", &info_data->info_time_stats->_1min},
                             {"Last 5 minutes", &info_data->info_time_stats->_5min},
                             {"Last 30 minutes", &info_data->info_time_stats->_30min},
                             {"Last 60 minutes", &info_data->info_time_stats->_60min}};

        for (k = 0; k < 5; ++k) {
            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, time_servers[k].label);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);

            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Requests/second", time_servers[k].v->requests_per_sec);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);

            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average used servers", time_servers[k].v->used_servers);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);

            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average running servers", time_servers[k].v->max_servers);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);

            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average running children", time_servers[k].v->children);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);

            sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);
        }

    }

    ci_stat_groups_iterate(info_data, print_group_statistics);
    ci_membuf_write(info_data->body, NULL, 0, 1);

    return 1;
}

struct info_time_stats_snapshot {
    int snapshots;
    time_t when;
    time_t when_max;
    uint64_t requests;
    uint64_t min_requests;
    int children;
    int servers;
    int used_servers;
};

static struct info_time_stats_snapshot OneMinSecs[60];
static struct info_time_stats_snapshot OneHourMins[60];

static void append_snapshots(struct info_time_stats_snapshot *dest, const struct info_time_stats_snapshot *add)
{
    dest->snapshots += add->snapshots;

    if (dest->when > add->when || dest->when == 0)
        dest->when = add->when;
    if (dest->when_max < add->when_max)
        dest->when_max = add->when_max;

    if (add->requests > dest->requests)
        dest->requests = add->requests;
    if (add->min_requests < dest->min_requests || dest->min_requests == 0)
        dest->min_requests = add->min_requests;

    dest->children += add->children;
    dest->servers += add->servers;
    dest->used_servers += add->used_servers;
}

static void build_time_range_stats(struct time_range_stats *tr, const struct info_time_stats_snapshot *sn, time_t min_range)
{
    time_t period = sn->when_max > sn->when ? sn->when_max - sn->when : 1;
    if (period < min_range)
        return;
    tr->requests_per_sec = (sn->requests - sn->min_requests) / period;
    tr->max_servers = sn->servers / sn->snapshots;
    tr->used_servers = sn->used_servers / sn->snapshots;
    tr->children = sn->children / sn->snapshots;
}


static struct info_time_stats_snapshot *take_snapshot(time_t curr_time)
{
    int i;
    struct childs_queue *q = childs_queue;
    struct info_time_stats_snapshot *snapshot = NULL;
    struct info_time_stats_snapshot *minsnapshot = NULL;

    int indx = curr_time % 60;
    time_t minute = curr_time / (time_t)60;
    int min_indx =  minute % 60;
    snapshot = &OneMinSecs[indx];
    minsnapshot = &OneHourMins[min_indx];

    if (snapshot->when != curr_time) {
        memset(snapshot, 0, sizeof(struct info_time_stats_snapshot));
        snapshot->when = curr_time;
        snapshot->when_max = curr_time;
    } else
        snapshot->requests = 0; /*Will update with newer higher value*/
    snapshot->snapshots++;

    if ((minsnapshot->when % 60) != minute) {
        memset(minsnapshot, 0, sizeof(struct info_time_stats_snapshot));
        minsnapshot->when = curr_time;
        minsnapshot->when_max = curr_time;
    } else {
        minsnapshot->requests = 0; /*Will update with newer higher value*/
        minsnapshot->when_max = curr_time;
    }
    minsnapshot->snapshots++;

    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid != 0) {
            snapshot->children++;
            snapshot->servers += q->childs[i].servers;
            snapshot->used_servers += q->childs[i].usedservers;
            snapshot->requests += q->childs[i].requests;

            minsnapshot->children++;
            minsnapshot->servers += q->childs[i].servers;
            minsnapshot->used_servers += q->childs[i].usedservers;
            minsnapshot->requests += q->childs[i].requests;
        }
    }
    snapshot->requests += q->srv_stats->history_requests;
    minsnapshot->requests += q->srv_stats->history_requests;

    if (snapshot->min_requests == 0)
        snapshot->min_requests = snapshot->requests;
    if (minsnapshot->min_requests == 0)
        minsnapshot->min_requests = minsnapshot->requests;
    return snapshot;
}

static void process_snapshot(const struct info_time_stats_snapshot *snapshot)
{
    time_t curr_time = snapshot->when;
    time_t btime = snapshot->when - 1;
    int prevIndx = btime % 60;
    struct info_time_stats_snapshot *prevsnapshot = &OneMinSecs[prevIndx];
    int BACKSECS = 5;
    while (prevsnapshot->when != btime && BACKSECS > 0) {
        BACKSECS--;
        btime--;
        prevIndx = btime % 60;
        prevsnapshot = &OneMinSecs[prevIndx];
    }

    struct info_time_stats *info_tstats = ci_server_shared_memblob(InfoSharedMemId);
    struct info_time_stats_snapshot stats_1s;
    memset(&stats_1s, 0, sizeof(struct info_time_stats_snapshot));
    if (BACKSECS > 0) {
        append_snapshots(&stats_1s, prevsnapshot);
        append_snapshots(&stats_1s, snapshot);
        build_time_range_stats(&info_tstats->_1sec, &stats_1s, 1);

        ci_debug_printf(10, "children %d, used servers: %d/%d, request rate: %d\n",
                        info_tstats->_1sec.children,
                        info_tstats->_1sec.used_servers,
                        info_tstats->_1sec.max_servers,
                        info_tstats->_1sec.requests_per_sec);
    }

    int i;
    struct info_time_stats_snapshot stats_1m;
    memset(&stats_1m, 0, sizeof(struct info_time_stats_snapshot));
    stats_1m.when = curr_time;
    for (i = 0; i < 60; i++) {
        if (OneMinSecs[i].when >= (curr_time - 60)) {
            append_snapshots(&stats_1m, &OneMinSecs[i]);
        }
    }
    build_time_range_stats(&info_tstats->_1min, &stats_1m, 60 - 2);

    struct info_time_stats_snapshot stats_5m;
    struct info_time_stats_snapshot stats_30m;
    struct info_time_stats_snapshot stats_60m;
    memset(&stats_5m, 0, sizeof(struct info_time_stats_snapshot));
    stats_5m.when = curr_time;
    memset(&stats_30m, 0, sizeof(struct info_time_stats_snapshot));
    stats_30m.when = curr_time;
    memset(&stats_60m, 0, sizeof(struct info_time_stats_snapshot));
    stats_60m.when = curr_time;
    for (i = 0; i < 60; i++) {
        if (OneHourMins[i].when >= (curr_time - 300)) {
            append_snapshots(&stats_5m, &OneHourMins[i]);
        }
        if (OneHourMins[i].when >= (curr_time - 1800)) {
            append_snapshots(&stats_30m, &OneHourMins[i]);
        }
        if (OneHourMins[i].when >= (curr_time - 3600)) {
            append_snapshots(&stats_60m, &OneHourMins[i]);
        }
    }
    build_time_range_stats(&info_tstats->_5min, &stats_5m, 300 - 10);
    build_time_range_stats(&info_tstats->_30min, &stats_30m, 1800 - 60);
    build_time_range_stats(&info_tstats->_60min, &stats_60m, 3600 - 60);
}


void info_monitor_init_cmd(const char *name, int type, void *data)
{
    memset(OneMinSecs, 0, sizeof(OneMinSecs));
    memset(OneHourMins, 0, sizeof(OneHourMins));
    ci_command_schedule("info::monitor_periodic", NULL, 1);
}

void info_monitor_periodic_cmd(const char *name, int type, void *data)
{
    struct info_time_stats_snapshot *snapshot = NULL;
    time_t now;
    time(&now);
    if ((snapshot = take_snapshot(now))) {
        process_snapshot(snapshot);
    }
    ci_command_schedule_on("info::monitor_periodic", NULL, now + 1);
}
