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
#include "array.h"
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
int info_post_init_service(ci_service_xdata_t * srv_xdata, struct ci_server_conf *server_conf);
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
    info_post_init_service,         /* post_init_service. Service initialization after c-icap configured.*/
    info_close_service,           /* mod_close_service. Called when service shutdowns. */
    info_init_request_data,         /* mod_init_request_data */
    info_release_request_data,      /* mod_release_request_data */
    info_check_preview_handler,     /* mod_check_preview_handler */
    info_end_of_data_handler,       /* mod_end_of_data_handler */
    info_io,                        /* mod_service_io */
    NULL,
    NULL
};

struct time_counters_ids{
    const char *name;
    ci_stat_type_t type;
    const char *group;
    int id;
    int per_request:1;
    int per_time:1;
} InfoTimeCountersId[] = {
    /*TODO: Make the InfoTimeCountersId array configurable*/
    {"BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1},
    {"BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1},
    {"HTTP BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1},
    {"HTTP BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1},
    {"BODY BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1},
    {"BODY BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1},
    {NULL, 0, NULL, -1, 0, 0}
};
#define InfoTimeCountersIdLength (sizeof(InfoTimeCountersId) / sizeof(InfoTimeCountersId[0]))

struct time_range_stats {
    uint64_t requests_per_sec;
    int used_servers;
    int max_servers;
    int children;
    ci_kbs_t kbs_per_sec[InfoTimeCountersIdLength];
    ci_kbs_t kbs_per_request[InfoTimeCountersIdLength];
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
    int memory_pools_master_group_id;
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

int info_post_init_service(ci_service_xdata_t * srv_xdata, struct ci_server_conf *server_conf)
{
    int i;
    for (i = 0; InfoTimeCountersId[i].name != NULL; i++)
        InfoTimeCountersId[i].id = ci_stat_entry_find(InfoTimeCountersId[i].name, InfoTimeCountersId[i].group, InfoTimeCountersId[i].type);
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

    info_data->body = ci_membuf_new_sized(32*1024);
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
    info_data->memory_pools_master_group_id = ci_stat_group_find("Memory Pools");

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
            ci_stat_memblock_merge(info_data->collect_stats, stats, 0);
        } else if (q->childs[i].pid != 0 && q->childs[i].to_be_killed) {
            if (info_data->closing_child_pids)
                info_data->closing_child_pids[info_data->closing_childs] = q->childs[i].pid;
            info_data->closing_childs++;
        }
    }
    /*Merge history data*/
    stats = q->stats_area + q->size * q->stats_block_size;
    assert(ci_stat_memblock_check(stats));
    ci_stat_memblock_merge(info_data->collect_stats, stats, 1);

    srv_stats =
        (struct server_statistics *)(q->stats_area + q->size * q->stats_block_size + q->stats_block_size);
    /*Compute server statistics*/
    info_data->started_childs = srv_stats->started_childs;
    info_data->closed_childs = srv_stats->closed_childs;
    info_data->crashed_childs = srv_stats->crashed_childs;
}

struct stats_tmpl {
    const char *simple_table_start;
    const char *simple_table_end;
    const char *simple_table_item_int;
    const char *simple_table_item_kbs;
    const char *simple_table_item_str;
    const char *simple_table_item_usec;
    const char *simple_table_item_msec;

    const char *table_header;
    const char *table_item;
    const char *table_row_start;
    const char *table_row_end;
    const char *table_col_sep;
    const char *sep;
};

struct stats_tmpl txt_tmpl = {
    "\n%s Statistics\n==================\n",
    "",
    "%s : %llu\n",
    "%s : %llu Kbs %u bytes\n",
    "%s : %s\n",
    "%s : %llu usec\n",
    "%s : %llu msec\n",

    "%s",
    "%s",
    "\n",
    "",
    "\t",
    ", "
};

struct stats_tmpl html_tmpl = {
    "<H1>%s Statistics</H1>\n<TABLE>",
    "</TABLE>\n",
    "<TR><TH>%s:</TH><TD>  %llu</TD>\n",
    "<TR><TH>%s:</TH><TD>  %llu Kbs %u bytes</TD>\n",
    "<TR><TH>%s:</TH><TD>  %s</TD>\n",
    "<TR><TH>%s:</TH><TD>  %llu usec</TD>\n",
    "<TR><TH>%s:</TH><TD>  %llu msec</TD>\n",
    "<TH> %s </TH>",
    "<TD> %s </TD>",
    "\n<TR>",
    "</TR>",
    "",
    "<BR>"
};

struct subgroups_data {
    const char *name;
    ci_str_vector_t *labels;
    ci_str_array_t *current_row;
    int memory_pools_master_group_id;
    ci_stat_memblock_t *collect_stats;
    int txt_mode;
    ci_membuf_t *body;
};

static int build_subgroup_statistics_row(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[256];
    ci_kbs_t kbs;
    assert(label);
    assert(data);
    struct subgroups_data *subgroups_data = (struct subgroups_data *)data;
    switch (stat->type) {
    case CI_STAT_INT64_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64,
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id));
        break;
    case CI_STAT_KBS_T:
        kbs = ci_stat_memblock_get_kbs(subgroups_data->collect_stats, id);
        snprintf(buf, sizeof(buf),
                 "%" PRIu64 " Kbs %" PRIu64 " bytes",
                 ci_kbs_kilobytes(&kbs),
                 ci_kbs_remainder_bytes(&kbs));
        break;
    case CI_STAT_TIME_US_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64 " usec",
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id));
        break;
    case CI_STAT_TIME_MS_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64 " msec",
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id));
        break;
    default:
        buf[0] = '-';
        buf[1] = '\0';
        break;
    }
    ci_debug_printf(8, "SubGroup item %s:%s\n", label, buf);
    ci_str_array_add(subgroups_data->current_row, label, buf);
    return 0;
}

static int build_subgroups_statistics_table(void *data, const char *grp_name, int group_id, int master_group_id)
{
    char buf[1024];
    size_t sz;
    struct subgroups_data *subgroups_data = (struct subgroups_data *)data;
    if (master_group_id != subgroups_data->memory_pools_master_group_id)
        return 0;

    assert(subgroups_data);
    ci_debug_printf(8, "Subgroup row %s\n", grp_name);
    if (ci_array_size(subgroups_data->current_row) > 0)
        ci_str_array_rebuild(subgroups_data->current_row);

    ci_stat_statistics_iterate(data, group_id, build_subgroup_statistics_row);

    struct stats_tmpl *tmpl = subgroups_data->txt_mode ? &txt_tmpl : &html_tmpl;
    int i;
    if (ci_vector_size(subgroups_data->labels) == 0) {
        const char *label;
        sz = snprintf(buf, sizeof(buf), tmpl->table_header, subgroups_data->name);
        assert(sz < sizeof(buf));
        ci_membuf_write(subgroups_data->body, tmpl->table_row_start, strlen(tmpl->table_row_start), 0);
        ci_membuf_write(subgroups_data->body, buf, sz, 0);
        for (i = 0; (label = ci_array_name(subgroups_data->current_row, i)) != NULL; i++) {
            ci_str_vector_add(subgroups_data->labels, label);
            sz = snprintf(buf, sizeof(buf), tmpl->table_header, label);
            if (sz > sizeof(buf))
                sz = sizeof(buf);
            ci_membuf_write(subgroups_data->body, tmpl->table_col_sep, strlen(tmpl->table_col_sep), 0);
            ci_membuf_write(subgroups_data->body, buf, sz, 0);
        }
    }
    sz = snprintf(buf, sizeof(buf), tmpl->table_header, grp_name);
    assert(sz < sizeof(buf));
    ci_membuf_write(subgroups_data->body, tmpl->table_row_start, strlen(tmpl->table_row_start), 0);
    ci_membuf_write(subgroups_data->body, buf, sz, 0);
    for (i = 0; i < ci_vector_size(subgroups_data->labels); i++) {
        const char *val = "-";
        const char *item = ci_str_vector_get(subgroups_data->labels, i);
        if (item)
            val = ci_str_array_search(subgroups_data->current_row, item);
        sz = snprintf(buf, sizeof(buf), tmpl->table_item, val);
        if (sz > sizeof(buf))
            sz = sizeof(buf);
        ci_membuf_write(subgroups_data->body, tmpl->table_col_sep, strlen(tmpl->table_col_sep), 0);
        ci_membuf_write(subgroups_data->body, buf, sz, 0);
    }
    ci_membuf_write(subgroups_data->body, tmpl->table_row_end, strlen(tmpl->table_row_end), 0);
    return 0;
}

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
    case CI_STAT_TIME_US_T:
        sz = snprintf(buf, sizeof(buf),
                      tmpl->simple_table_item_usec,
                      label,
                      ci_stat_memblock_get_counter(info_data->collect_stats, id));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        break;
    case CI_STAT_TIME_MS_T:
        sz = snprintf(buf, sizeof(buf),
                      tmpl->simple_table_item_msec,
                      label,
                      ci_stat_memblock_get_counter(info_data->collect_stats, id));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        break;
    default:
        break;
    }
    return 0;
}

static int print_group_statistics(void *data, const char *grp_name, int group_id, int master_group_id)
{
    char buf[1024];
    int sz;
    if (master_group_id == CI_STAT_GROUP_MASTER)
        return 0;

    struct info_req_data *info_data = (struct info_req_data *)data;
    if (master_group_id == info_data->memory_pools_master_group_id)
        return 0; /*They are handled in their own table*/
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
    int sz, k, j;
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

            for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
                if (InfoTimeCountersId[j].per_time) {
                    char label[256];
                    snprintf(label, sizeof(label), "%s per second", InfoTimeCountersId[j].name);
                    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_kbs, label, ci_kbs_kilobytes(&time_servers[k].v->kbs_per_sec[j]), ci_kbs_remainder_bytes(&time_servers[k].v->kbs_per_sec[j]));
                    if (sz >= sizeof(buf))
                        sz = sizeof(buf) - 1;
                    ci_membuf_write(info_data->body, buf, sz, 0);
                }
            }

            for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
                if (InfoTimeCountersId[j].per_request) {
                    char label[256];
                    snprintf(label, sizeof(label), "%s per request", InfoTimeCountersId[j].name);
                    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_kbs, label, ci_kbs_kilobytes(&time_servers[k].v->kbs_per_request[j]), ci_kbs_remainder_bytes(&time_servers[k].v->kbs_per_request[j]));
                    if (sz >= sizeof(buf))
                        sz = sizeof(buf) - 1;
                    ci_membuf_write(info_data->body, buf, sz, 0);
                }
            }

            sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);
        }

    }

    struct subgroups_data mempools_data = {
    name: "pool",
    labels : ci_str_vector_create(1024),
    current_row : ci_str_array_new(2048),
    memory_pools_master_group_id: info_data->memory_pools_master_group_id,
    collect_stats: info_data->collect_stats,
    txt_mode: info_data->txt_mode,
    body: info_data->body
    };
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, "Memory pools");
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
    ci_stat_groups_iterate(&mempools_data, build_subgroups_statistics_table);
    sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
    ci_str_vector_destroy(mempools_data.labels);
    ci_str_array_destroy(mempools_data.current_row);

    ci_stat_groups_iterate(info_data, print_group_statistics);
    ci_membuf_write(info_data->body, NULL, 0, 1);

    return 1;
}

struct info_time_stats_snapshot {
    int snapshots;
    time_t when;
    time_t when_max;
    uint64_t requests;
    int children;
    int servers;
    int used_servers;
    ci_kbs_t kbs_counters_instance[InfoTimeCountersIdLength];
};

static struct info_time_stats_snapshot OneMinSecs[60];
static struct info_time_stats_snapshot OneHourMins[60];

struct info_time_stats_snapshot_results {
    int snapshots;
    time_t when_start;
    time_t when_end;
    uint64_t requests_start;
    uint64_t requests_end;
    int children;
    int servers;
    int used_servers;
    const ci_kbs_t *kbs_counters_instance_start;
    const ci_kbs_t *kbs_counters_instance_end;
};

static void append_snapshots(struct info_time_stats_snapshot_results *result, const struct info_time_stats_snapshot *add)
{
    result->snapshots += add->snapshots;

    if (result->when_start > add->when || result->when_start == 0) {
        result->when_start = add->when;
        result->requests_start = add->requests;
        result->kbs_counters_instance_start = add->kbs_counters_instance;
    }

    if (result->when_end < add->when_max) {
        result->when_end = add->when_max;
        result->requests_end = add->requests;
        result->kbs_counters_instance_end = add->kbs_counters_instance;
    }
    assert(result->kbs_counters_instance_start);
    assert(result->kbs_counters_instance_end);
    result->children += add->children;
    result->servers += add->servers;
    result->used_servers += add->used_servers;
}

static void build_time_range_stats(struct time_range_stats *tr, const struct info_time_stats_snapshot_results *accumulated, time_t time_range)
{
    int i;
    uint64_t requests = accumulated->requests_end - accumulated->requests_start;
    time_t period = accumulated->when_end > accumulated->when_start ? accumulated->when_end - accumulated->when_start : 1;
    if (period < time_range)
        return;
    tr->requests_per_sec = requests / period;
    tr->max_servers = accumulated->servers / accumulated->snapshots;
    tr->used_servers = accumulated->used_servers / accumulated->snapshots;
    tr->children = accumulated->children / accumulated->snapshots;

    for(i = 0; i < InfoTimeCountersIdLength && InfoTimeCountersId[i].name; i++) {
        uint64_t bytes = accumulated->kbs_counters_instance_end[i].bytes - accumulated->kbs_counters_instance_start[i].bytes;
        if (InfoTimeCountersId[i].per_request)
            tr->kbs_per_request[i].bytes = requests ? (bytes / requests) : 0;
        if (InfoTimeCountersId[i].per_time)
            tr->kbs_per_sec[i].bytes = bytes / period;
    }
}


static struct info_time_stats_snapshot *take_snapshot(time_t curr_time)
{
    int i, k;
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
    }
    snapshot->snapshots++;

    if ((minsnapshot->when % 60) != minute) {
        memset(minsnapshot, 0, sizeof(struct info_time_stats_snapshot));
        minsnapshot->when = curr_time;
        minsnapshot->when_max = curr_time;
    } else {
        minsnapshot->when_max = curr_time;
    }
    minsnapshot->snapshots++;

    /*Will update with newer higher value*/
    snapshot->requests = q->srv_stats->history_requests;
    minsnapshot->requests = q->srv_stats->history_requests;
    for (k = 0; InfoTimeCountersId[k].name != NULL; k++) {
        if (InfoTimeCountersId[k].id >= 0) {
            ci_kbs_t kbs = ci_stat_memblock_get_kbs(q->stats_history, InfoTimeCountersId[k].id);
            snapshot->kbs_counters_instance[k] = kbs;
            minsnapshot->kbs_counters_instance[k] = kbs;
        }
    }

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

            ci_stat_memblock_t *stats;
            stats = (ci_stat_memblock_t *)(q->stats_area + i * (q->stats_block_size));
            for (k = 0; InfoTimeCountersId[k].name != NULL; k++) {
                if (InfoTimeCountersId[k].id >= 0) {
                    ci_kbs_t kbs = ci_stat_memblock_get_kbs(stats, InfoTimeCountersId[k].id);
                    ci_kbs_add_to(&snapshot->kbs_counters_instance[k], &kbs);
                    ci_kbs_add_to(&minsnapshot->kbs_counters_instance[k], &kbs);
                }
            }
        }
    }
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
    struct info_time_stats_snapshot_results stats_1s;
    memset(&stats_1s, 0, sizeof(struct info_time_stats_snapshot_results));
    if (BACKSECS > 0) {
        append_snapshots(&stats_1s, prevsnapshot);
        append_snapshots(&stats_1s, snapshot);
        build_time_range_stats(&info_tstats->_1sec, &stats_1s, 1);

        ci_debug_printf(10, "children %d, used servers: %d/%d, request rate: %"PRIu64"\n",
                        info_tstats->_1sec.children,
                        info_tstats->_1sec.used_servers,
                        info_tstats->_1sec.max_servers,
                        info_tstats->_1sec.requests_per_sec);
    }

    int i;
    struct info_time_stats_snapshot_results stats_1m;
    memset(&stats_1m, 0, sizeof(struct info_time_stats_snapshot_results));
    for (i = 0; i < 60; i++) {
        if (OneMinSecs[i].when >= (curr_time - 60)) {
            append_snapshots(&stats_1m, &OneMinSecs[i]);
        }
    }
    build_time_range_stats(&info_tstats->_1min, &stats_1m, 60 - 2);

    struct info_time_stats_snapshot_results stats_5m;
    struct info_time_stats_snapshot_results stats_30m;
    struct info_time_stats_snapshot_results stats_60m;
    memset(&stats_5m, 0, sizeof(struct info_time_stats_snapshot_results));
    memset(&stats_30m, 0, sizeof(struct info_time_stats_snapshot_results));
    memset(&stats_60m, 0, sizeof(struct info_time_stats_snapshot_results));
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
