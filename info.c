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
#include "util.h"
#include "http_server.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <assert.h>
#include <ctype.h>
#include <regex.h>

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
    int average:1;
} InfoTimeCountersId[] = {
    /*TODO: Make the InfoTimeCountersId array configurable*/
    {"TIME PER REQUEST", CI_STAT_TIME_US_T, "General", -1, 0, 0, 1},
    {"PROCESSING TIME PER REQUEST", CI_STAT_TIME_US_T, "General", -1, 0, 0, 1},
    {"BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {"BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {"HTTP BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {"HTTP BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {"BODY BYTES IN", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {"BODY BYTES OUT", CI_STAT_KBS_T, "General", -1, 1, 1, 0},
    {NULL, 0, NULL, -1, 0, 0}
};
#define InfoTimeCountersIdLength (sizeof(InfoTimeCountersId) / sizeof(InfoTimeCountersId[0]))

struct per_time_stats {
    uint64_t requests;
    uint64_t requests_per_sec;
    int used_servers;
    int max_servers;
    int children;
    ci_kbs_t kbs_per_sec[InfoTimeCountersIdLength];
    ci_kbs_t kbs_per_request[InfoTimeCountersIdLength];
    uint64_t uint64_average[InfoTimeCountersIdLength];
};

struct info_time_stats {
    struct per_time_stats _1sec;
    struct per_time_stats _1min;
    struct per_time_stats _5min;
    struct per_time_stats _30min;
    struct per_time_stats _60min;
};

enum {OUT_FMT_TEXT, OUT_FMT_HTML, OUT_FMT_CSV};
enum { PRINT_INFO_MENU, PRINT_ALL_TABLES, PRINT_SOME_TABLES, PRINT_HISTOGRAMS_LIST };
struct info_req_data {
    char *url;
    ci_membuf_t *body;
    int must_free_body;
    int format;
    int print_page;
    int view_child;
    time_t time;
    char time_str[128];
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
    int supports_svg;
    ci_stat_memblock_t *collect_stats;
    struct info_time_stats *info_time_stats;
    ci_str_vector_t *tables;
    ci_str_vector_t *histos;
};

extern struct childs_queue *childs_queue;
extern ci_proc_mutex_t accept_mutex;

int InfoSharedMemId = -1;

static void parse_info_arguments(struct info_req_data *info_data, char *args);
static int print_statistics(struct info_req_data *info_data);
static void info_monitor_init_cmd(const char *name, int type, void *data);
static void info_monitor_periodic_cmd(const char *name, int type, void *data);
static int stats_web_service(ci_request_t *req);
static int build_info_web_service(ci_request_t *req);

#define ICAP_GLOBAL_STATS
#ifdef ICAP_GLOBAL_STATS

#define ICAP_STATS_MEM_NAME "/icapstats"
#define ICAP_SEM_MUTEX_NAME "/icapstatsmutex"
#define ICAP_STATS_MEM_SIZE 0x100000

void *icap_stats_ptr;
int *stats_fd;
int stats_shm;
sem_t *icap_mutex_sem;

int icap_lock(void)
{
	if (!icap_mutex_sem)
		return -1;
	if (sem_wait(icap_mutex_sem) == -1)
		return -2;
	return 0;
}

int icap_unlock(void)
{
	if (!icap_mutex_sem)
		return -1;
	if (sem_post(icap_mutex_sem) == -1)
		return -2;
	return 0;
}

void init_maps(void) 
{
	// set umask to 000
  	umask(0);

	stats_fd = mmap(NULL, sizeof *stats_fd, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (stats_fd == (void *)-1) {
		stats_fd = NULL;
		ci_debug_printf(4, "info: mmap failed pid %ld\n", (unsigned long) getpid());
	}
	else { 
		ci_debug_printf(4, "info: mmap success %p pid %ld\n", stats_fd, (unsigned long) getpid());
	}

	if ((stats_shm = shm_open(ICAP_STATS_MEM_NAME, O_RDWR | O_CREAT, 0644)) == -1) {
		stats_shm = 0;
		ci_debug_printf(4, "info: shm_open %s failed pid %ld\n", ICAP_STATS_MEM_NAME, (unsigned long) getpid());
	}
	else {
		ci_debug_printf(4, "info: shm_open %s success %i pid %ld\n", ICAP_STATS_MEM_NAME, stats_shm, (unsigned long) getpid());
	}
	if (stats_fd) 
		*stats_fd = stats_shm;

	if (ftruncate(stats_shm, ICAP_STATS_MEM_SIZE) == -1) {
		ci_debug_printf(4, "info: ftruncate failed pid %ld\n", (unsigned long) getpid());
	}
	else { 
		ci_debug_printf(4, "info: ftruncate size set to %ld pid %ld\n", (unsigned long)ICAP_STATS_MEM_SIZE, (unsigned long) getpid());	
	}

	if (stats_shm > 0) {
		if ((icap_stats_ptr = mmap(NULL, ICAP_STATS_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, stats_shm, 0)) == MAP_FAILED) {
			ci_debug_printf(4, "info: mmap failed %p pid %ld\n", icap_stats_ptr, (unsigned long) getpid());
			icap_stats_ptr = NULL;
		}
		else {
			ci_debug_printf(4, "info: mmap success %p pid %ld\n", icap_stats_ptr, (unsigned long) getpid());
		}
	}	

	if ((icap_mutex_sem = sem_open(ICAP_SEM_MUTEX_NAME, O_CREAT, 0666, 1)) == SEM_FAILED) {
		ci_debug_printf(4, "info: sem_open %s failed %p pid %ld\n", ICAP_SEM_MUTEX_NAME, icap_mutex_sem,
                                (unsigned long) getpid());
		icap_mutex_sem = NULL;
	}
	else {
		ci_debug_printf(4, "info: sem_open %s %p pid %ld\n", ICAP_SEM_MUTEX_NAME, icap_mutex_sem, (unsigned long) getpid());
	}
}

void close_maps(void) 
{
	if (icap_mutex_sem) {
		if (!sem_close(icap_mutex_sem)) {
			ci_debug_printf(4, "info: sem_close %p pid %ld\n", icap_mutex_sem, (unsigned long) getpid());
		}
		else {
			ci_debug_printf(4, "info: sem_close failed %p pid %ld\n", icap_mutex_sem, (unsigned long) getpid());
		}

		if (!sem_unlink(ICAP_SEM_MUTEX_NAME)) {
			ci_debug_printf(4, "info: sem_unlink %s pid %ld\n", ICAP_SEM_MUTEX_NAME, (unsigned long) getpid());
		}
		else {
			ci_debug_printf(4, "info: sem_unlink failed %s pid %ld\n", ICAP_SEM_MUTEX_NAME, (unsigned long) getpid());
		}
		icap_mutex_sem = NULL;
	}

	if (icap_stats_ptr) {
		if (munmap(icap_stats_ptr, ICAP_STATS_MEM_SIZE)) {
			ci_debug_printf(4, "info: munmap failed %p pid %ld\n", icap_stats_ptr, (unsigned long) getpid());
		}
		else {
			ci_debug_printf(4, "info: munmap success %p pid %ld\n", icap_stats_ptr, (unsigned long) getpid());
		}
		icap_stats_ptr = NULL;
	}

	if (stats_fd && *stats_fd) {
		if ((shm_unlink(ICAP_STATS_MEM_NAME)) == -1) {
			ci_debug_printf(4, "info: shm_unlink failed %s pid %ld\n", ICAP_STATS_MEM_NAME, (unsigned long) getpid());
		}
		else {
			ci_debug_printf(4, "info: shm_unlink success %s pid %ld\n", ICAP_STATS_MEM_NAME, (unsigned long) getpid());
			stats_shm = 0;
			*stats_fd = 0;
		}
	}

	if (stats_fd) {
		if (munmap(stats_fd, sizeof *stats_fd)) {
			ci_debug_printf(4, "info: munmap failed %p pid %ld\n", stats_fd, (unsigned long) getpid());
		}
		else {
			ci_debug_printf(4, "info: munmap success %p pid %ld\n", stats_fd, (unsigned long) getpid());
		}
		stats_fd = NULL;
	}
}

#define MAX_PROCESSES 64

unsigned long pidtable[MAX_PROCESSES];
unsigned long pidindex;
unsigned long stats_active;

static void icapstats_signal_handler(int signo)
{
	switch (signo) {
	case SIGINT:
		ci_debug_printf(4, "info: got SIGINT pid %ld\n", (unsigned long) getpid());
                stats_active = 0;
		break;
	case SIGTERM:
		ci_debug_printf(4, "info: got SIGTERM pid %ld\n", (unsigned long) getpid());
                stats_active = 0;
		break;
	case SIGHUP:
		ci_debug_printf(4, "info: got SIGHUP pid %ld\n", (unsigned long) getpid());
                stats_active = 0;
		break;
        default:
		break;
	}
}

void process_stats(void)
{
    struct info_req_data *info_data;

    info_data = malloc(sizeof(struct info_req_data));
    info_data->url = NULL;
    // set to NON-HTML   
    info_data->body = ci_membuf_new_sized(32*1024);
    info_data->must_free_body = 1;

    info_data->print_page = PRINT_INFO_MENU;
    info_data->view_child = -1;
    info_data->time = 0;
    info_data->time_str[0] = '\0';
    info_data->childs = 0;
    info_data->child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->free_servers = 0;
    info_data->used_servers = 0;
    info_data->closing_childs = 0;
    info_data->closing_child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->started_childs = 0;
    info_data->closed_childs = 0;
    info_data->crashed_childs = 0;
    info_data->format = OUT_FMT_HTML;
    info_data->supports_svg = 0;
    info_data->tables = NULL;
    info_data->histos = NULL;
    info_data->memory_pools_master_group_id = ci_stat_group_find("Memory Pools");
    // set to view=text
    info_data->format = OUT_FMT_TEXT;
    // set to table=*
    info_data->print_page = PRINT_ALL_TABLES;

    void *mem = malloc(ci_stat_memblock_size());
    info_data->collect_stats = mem ? ci_stat_memblock_init(mem, ci_stat_memblock_size()) : NULL;
    if (!info_data->collect_stats) {
       goto release_allocs;
    }
    info_data->info_time_stats = ci_server_shared_memblob(InfoSharedMemId);

    if (info_data->body) {
        info_data->body->flags |= CI_MEMBUF_NULL_TERMINATED;
        print_statistics(info_data);
    	if (info_data->body) {
		if (icap_stats_ptr) {
			if (!icap_lock()) {
				snprintf(icap_stats_ptr,
					info_data->body->endpos < (ICAP_STATS_MEM_SIZE-1)
					? info_data->body->endpos : (ICAP_STATS_MEM_SIZE-1),
					"%s", info_data->body->buf); 
				icap_unlock();
			}
		}
	}
    }

release_allocs:;
    if (info_data->url)
        ci_buffer_free(info_data->url);

    if (info_data->must_free_body && info_data->body)
        ci_membuf_free(info_data->body);

    if (info_data->child_pids)
        free(info_data->child_pids);

    if (info_data->closing_child_pids)
        free(info_data->closing_child_pids);

    if (info_data->collect_stats)
        free(info_data->collect_stats);

    if (info_data->tables)
        ci_str_vector_destroy(info_data->tables);

    if (info_data->histos)
        ci_str_vector_destroy(info_data->histos);

    free(info_data);
}

void icap_stats_process(void)
{
        register int cpid, j;

        init_maps();
	for (pidindex=j=0; j < 1; j++) {
           cpid = fork();
           if (!cpid) {
              if (prctl(PR_SET_PDEATHSIG, SIGHUP) < 0)
  	         ci_debug_printf(4, "info: could not register parent SIGHUP for pid %ld\n", (unsigned long) getpid());

              signal(SIGINT, icapstats_signal_handler);
              signal(SIGTERM, icapstats_signal_handler);
              signal(SIGHUP, icapstats_signal_handler);

              if (prctl(PR_SET_NAME, (unsigned long)"icapstats", 0, 0, 0) < 0) {
  	         ci_debug_printf(4, "info: could not set process name for pid %ld\n", (unsigned long) getpid());
              }
  	      ci_debug_printf(4, "info: icapstats process %i started pid %ld\n", j, (unsigned long) getpid());
              stats_active = 1;
              while (stats_active) {
		process_stats();
                if (sleep(1)) 
		   break;
              } 
              close_maps();
              exit(0);
           }
           else {
              pidtable[pidindex++] = cpid;                 
           }
        }
}
#else
void icap_stats_process(void) {};
#endif

int info_init_service(ci_service_xdata_t * srv_xdata,
                      struct ci_server_conf *server_conf)
{
    ci_service_set_xopts(srv_xdata,  CI_XAUTHENTICATEDUSER|CI_XAUTHENTICATEDGROUPS);
    InfoSharedMemId = ci_server_shared_memblob_register("InfoSharedData", sizeof(struct info_time_stats));
    ci_command_register_action("info::mon_start", CI_CMD_MONITOR_START, NULL,
                               info_monitor_init_cmd);
    ci_command_register_action("info::monitor_periodic", CI_CMD_MONITOR_ONDEMAND, NULL,
                               info_monitor_periodic_cmd);

    ci_http_server_register_service("/statistics", "The c-icap statistics web service", stats_web_service, 0);
    ci_http_server_register_service("/build_info", "The c-icap build configuration web service", build_info_web_service, 0);
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

int url_decoder(const char *input,char *output, int output_len);
void *info_init_request_data(ci_request_t * req)
{
    struct info_req_data *info_data;

    info_data = malloc(sizeof(struct info_req_data));
    info_data->url = NULL;
    if (req->protocol == CI_PROTO_HTTP) {
        info_data->body = ci_http_server_response_body(req);
        info_data->must_free_body = 0;
    } else {
        info_data->body = ci_membuf_new_sized(32*1024);
        info_data->must_free_body = 1;
    }
    info_data->print_page = PRINT_INFO_MENU;
    info_data->view_child = -1;
    info_data->time = 0;
    info_data->time_str[0] = '\0';
    info_data->childs = 0;
    info_data->child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->free_servers = 0;
    info_data->used_servers = 0;
    info_data->closing_childs = 0;
    info_data->closing_child_pids = malloc(childs_queue->size * sizeof(int));
    info_data->started_childs = 0;
    info_data->closed_childs = 0;
    info_data->crashed_childs = 0;
    info_data->format = OUT_FMT_HTML;
    info_data->supports_svg = 0;
    info_data->tables = NULL;
    info_data->histos = NULL;
    info_data->memory_pools_master_group_id = ci_stat_group_find("Memory Pools");
    if (req->args[0] != '\0') {
        char decoded_args[256];
        if (url_decoder(req->args, decoded_args, sizeof(decoded_args)) > 0)
            parse_info_arguments(info_data, decoded_args);
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

    if (info_data->url)
        ci_buffer_free(info_data->url);

    if (info_data->must_free_body && info_data->body)
        ci_membuf_free(info_data->body);

    if (info_data->child_pids)
        free(info_data->child_pids);

    if (info_data->closing_child_pids)
        free(info_data->closing_child_pids);

    if (info_data->collect_stats)
        free(info_data->collect_stats);

    if (info_data->tables)
        ci_str_vector_destroy(info_data->tables);

    if (info_data->histos)
        ci_str_vector_destroy(info_data->histos);

    free(info_data);
}

int url_decoder2(char *input);
int info_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t * req)
{
    struct info_req_data *info_data = ci_service_data(req);
    char url[1024];
    char *args;
    if (ci_req_hasbody(req))
        return CI_MOD_ALLOW204;

    info_data->supports_svg = 1;

    if (ci_http_request_url2(req, url, sizeof(url), CI_HTTP_REQUEST_URL_ARGS)) {
        int url_size = url_decoder2(url);
        info_data->url = ci_buffer_alloc(url_size + 1);
        if (!info_data->url)
            return CI_ERROR;
        memcpy(info_data->url, url, url_size);
        info_data->url[url_size] = '\0';

        if ((args = strchr(url, '?'))) {
            parse_info_arguments(info_data, args);
        }
    }

    ci_req_unlock_data(req);

    ci_http_response_create(req, 1, 1); /*Build the responce headers */

    ci_http_response_add_header(req, "HTTP/1.0 200 OK");
    ci_http_response_add_header(req, "Server: C-ICAP");
    if (info_data->format == OUT_FMT_TEXT)
        ci_http_response_add_header(req, "Content-Type: text/plain");
    else if (info_data->format == OUT_FMT_CSV)
        ci_http_response_add_header(req, "Content-Type: text/csv");
    else
        ci_http_response_add_header(req, "Content-Type: text/html");
    ci_http_response_add_header(req, "Content-Language: en");
    ci_http_response_add_header(req, "Connection: close");
    if (!print_statistics(info_data))
        return CI_ERROR;

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
    const ci_stat_memblock_t *stats;
    const struct server_statistics *srv_stats;
    if (!q->childs)
        return;

    /*Merge childs data*/
    for (i = 0; i < q->size; i++) {
        if (q->childs[i].pid == 0)
            continue;
        ci_debug_printf(1, "Check pids %d<>%d\n", q->childs[i].pid, info_data->view_child);
        if (info_data->view_child > 0 && q->childs[i].pid != info_data->view_child)
            continue;
        if (q->childs[i].to_be_killed == 0) {
            if (info_data->child_pids)
                info_data->child_pids[info_data->childs] = q->childs[i].pid;
            info_data->childs++;
            ci_atomic_load_i32(&q->childs[i].usedservers, &used_servers);
            info_data->free_servers += (q->childs[i].servers - used_servers);
            info_data->used_servers += used_servers;
            requests += q->childs[i].requests;

            stats = q->stats_area + i * (q->stats_block_size);
            assert(ci_stat_memblock_check(stats));
            ci_stat_memblock_merge(info_data->collect_stats, stats, 0, (info_data->childs - 1));
        } else if (q->childs[i].to_be_killed) {
            if (info_data->closing_child_pids)
                info_data->closing_child_pids[info_data->closing_childs] = q->childs[i].pid;
            info_data->closing_childs++;
        }
    }
    /*Merge history data*/
    if (info_data->view_child < 0) {
        stats = q->stats_history;
        assert(ci_stat_memblock_check(stats));
        ci_stat_memblock_merge(info_data->collect_stats, stats, 1, info_data->childs);
    }

    srv_stats = q->srv_stats;
    /*Compute server statistics*/
    info_data->started_childs = srv_stats->started_childs;
    info_data->closed_childs = srv_stats->closed_childs;
    info_data->crashed_childs = srv_stats->crashed_childs;
    time(&info_data->time);
    ci_to_strntime(info_data->time_str, sizeof(info_data->time_str), &info_data->time);
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
    "\n%s %s, %s\n==================\n",
    "",
    "%s : %llu\n",
    "%s : %llu Kbs %u bytes\n",
    "%s : %s\n",
    "%s : %llu usec\n",
    "%s : %llu msec\n",

    "%s",
    "%s",
    "",
    "\n",
    "\t",
    ", "
};

struct stats_tmpl html_tmpl = {
    "<H1>%s %s</H1>\n<TABLE>\n<CAPTION>%s</CAPTION>",
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
    int format;
    ci_membuf_t *body;
};

static int print_subgroup_stat_item(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[256];
    ci_kbs_t kbs;
    assert(label);
    assert(data);
    struct subgroups_data *subgroups_data = (struct subgroups_data *)data;
    switch (stat->type) {
    case CI_STAT_INT64_T:
    case CI_STAT_INT64_MEAN_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64,
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id));
        break;
    case CI_STAT_KBS_T:
        kbs = ci_stat_memblock_get_kbs(subgroups_data->collect_stats, id);
        if (subgroups_data->format == OUT_FMT_CSV)
            snprintf(buf, sizeof(buf), "%" PRIu64, kbs.bytes);
        else
            snprintf(buf, sizeof(buf),
                     "%" PRIu64 " Kbs %" PRIu64 " bytes",
                     ci_kbs_kilobytes(&kbs),
                     ci_kbs_remainder_bytes(&kbs));
        break;
    case CI_STAT_TIME_US_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64 "%s",
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id),
                 (subgroups_data->format == OUT_FMT_CSV ? "" : " usec")
            );
        break;
    case CI_STAT_TIME_MS_T:
        snprintf(buf, sizeof(buf),
                 "%" PRIu64 "%s",
                 ci_stat_memblock_get_counter(subgroups_data->collect_stats, id),
                 (subgroups_data->format == OUT_FMT_CSV ? "" : " msec")
            );
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

static int print_subgroup_stat_row_csv(struct subgroups_data *subgroups_data, const char *row_name)
{
    char buf[1024];
    int sz, i;
    if (ci_vector_size(subgroups_data->labels) == 0) {
        sz = snprintf(buf, sizeof(buf), "\"%s\"", subgroups_data->name);
        _CI_ASSERT(sz < sizeof(buf));
        ci_membuf_write(subgroups_data->body, buf, sz, 0);
        const char *label;
        for (i = 0; (label = ci_array_name(subgroups_data->current_row, i)) != NULL; i++) {
            ci_str_vector_add(subgroups_data->labels, label);
            sz = snprintf(buf, sizeof(buf), "\"%s\"", label);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(subgroups_data->body, buf, sz, 0);
        }
        ci_membuf_write(subgroups_data->body, "\n", 1, 0);
    }

    sz = snprintf(buf, sizeof(buf), "\"%s\"", row_name);
    _CI_ASSERT(sz < sizeof(buf));
    ci_membuf_write(subgroups_data->body, buf, sz, 0);
    for (i = 0; i < ci_vector_size(subgroups_data->labels); i++) {
        const char *val = "";
        const char *item = ci_str_vector_get(subgroups_data->labels, i);
        if (item)
            val = ci_str_array_search(subgroups_data->current_row, item);
        sz = snprintf(buf, sizeof(buf), ",%s", val);
        _CI_ASSERT(sz < sizeof(buf));
        ci_membuf_write(subgroups_data->body, buf, sz, 0);
    }
    ci_membuf_write(subgroups_data->body, "\n", 1, 0);
    return 0;
}

static int print_subgroup_stat_row(void *data, const char *grp_name, int group_id, int master_group_id)
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

    ci_stat_statistics_iterate(data, group_id, print_subgroup_stat_item);

    if (subgroups_data->format == OUT_FMT_CSV)
        return print_subgroup_stat_row_csv(subgroups_data, grp_name);

    struct stats_tmpl *tmpl = subgroups_data->format == OUT_FMT_TEXT ? &txt_tmpl : &html_tmpl;
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
        ci_membuf_write(subgroups_data->body, tmpl->table_row_end, strlen(tmpl->table_row_end), 0);
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

static int print_stat_label_csv(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[256];
    int sz;
    _CI_ASSERT(label);
    _CI_ASSERT(data);
    sz = snprintf(buf, sizeof(buf), ",\"%s\"", label);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    struct info_req_data *info_data = (struct info_req_data *)data;
    ci_membuf_write(info_data->body, buf, sz, 0);
    return 0;
}

static int print_stat_item_csv(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[128];
    int sz;
    ci_kbs_t kbs;
    uint64_t value = 0;
    _CI_ASSERT(stat);
    _CI_ASSERT(label);
    _CI_ASSERT(data);
    struct info_req_data *info_data = (struct info_req_data *)data;
    switch (stat->type) {
    case CI_STAT_INT64_T:
    case CI_STAT_TIME_US_T:
    case CI_STAT_TIME_MS_T:
    case CI_STAT_INT64_MEAN_T:
        value = ci_stat_memblock_get_counter(info_data->collect_stats, id);
        break;
    case CI_STAT_KBS_T:
        kbs = ci_stat_memblock_get_kbs(info_data->collect_stats, id);
        value = kbs.bytes;
        break;
    default:
        break;
    }
    sz = snprintf(buf, sizeof(buf), ",%" PRIu64, value);
    _CI_ASSERT(sz < sizeof(buf));
    ci_membuf_write(info_data->body, buf, sz, 0);
    return 0;
}

static int print_group_statistics_csv(struct info_req_data *info_data, const char *grp_name, int group_id, int master_group_id)
{
    char buf[128];
    int sz;
    sz = snprintf(buf, sizeof(buf), "\"Time\"");
    ci_membuf_write(info_data->body, buf, sz, 0);
    ci_stat_statistics_iterate(info_data, group_id, print_stat_label_csv);
    sz = snprintf(buf, sizeof(buf), "\n%lld", (long long)info_data->time);
    ci_membuf_write(info_data->body, buf, sz, 0);
    ci_stat_statistics_iterate(info_data, group_id, print_stat_item_csv);
    ci_membuf_write(info_data->body, "\n", 1, 0);
    return 0;
}

static int print_stat_item(void *data, const char *label, int id, int gId, const ci_stat_t *stat)
{
    char buf[1024];
    int sz;
    ci_kbs_t kbs;
    assert(stat);
    assert(label);
    assert(data);
    struct info_req_data *info_data = (struct info_req_data *)data;
    struct stats_tmpl *tmpl = info_data->format == OUT_FMT_TEXT ? &txt_tmpl : &html_tmpl;
    switch (stat->type) {
    case CI_STAT_INT64_T:
    case CI_STAT_INT64_MEAN_T:
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

int check_group(int print_type, ci_str_vector_t *tables, const char *grp_name)
{
    if (print_type == PRINT_SOME_TABLES) {
        if (!tables)
            return 0;
        if (!ci_str_vector_search(tables, grp_name))
            return 0;
    } else if (print_type != PRINT_ALL_TABLES)
        return 0;
    return 1;
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

    if (!check_group(info_data->print_page, info_data->tables, grp_name))
        return 0;

    if (info_data->format == OUT_FMT_CSV)
        return print_group_statistics_csv(info_data, grp_name, group_id, master_group_id);

    struct stats_tmpl *tmpl = info_data->format == OUT_FMT_TEXT ? &txt_tmpl : &html_tmpl;
    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, grp_name, "statistics", info_data->time_str);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    ci_stat_statistics_iterate(data, group_id, print_stat_item);
    sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
    return 0;
}

static void print_running_servers_statistics_csv(struct info_req_data *info_data)
{
    char buf[1024];
    int sz;
    sz = snprintf(buf, sizeof(buf), "\"Unix Time\",\"Children number\",\"Free Servers\",\"Used Servers\",\"Started Processes\",\"Closed Processes\",\"Crashed Processes\",\"Closing Processes\"\n");
    ci_membuf_write(info_data->body, buf, sz, 0);
    sz = snprintf(buf, sizeof(buf), "%lld,%d,%d,%d,%d,%d,%d,%u\n",
                  (long long)info_data->time, info_data->childs, info_data->free_servers, info_data->used_servers, info_data->started_childs, info_data->closed_childs, info_data->crashed_childs, info_data->closing_childs);
    _CI_ASSERT(sz < sizeof(buf));
    ci_membuf_write(info_data->body, buf, sz, 0);
}

static void print_running_servers_statistics(struct info_req_data *info_data)
{
    char buf[1024];
    int sz, i;
    struct stats_tmpl *tmpl;
    ci_membuf_t *tmp_membuf = NULL;
    const char *TableName = "Running servers";

    if (!check_group(info_data->print_page, info_data->tables, TableName))
        return;

    if (info_data->format == OUT_FMT_CSV)
        return print_running_servers_statistics_csv(info_data);
    if (info_data->format == OUT_FMT_TEXT)
        tmpl = &txt_tmpl;
    else
        tmpl = &html_tmpl;

    assert(info_data->body);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, TableName, "statistics", info_data->time_str);
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

static void print_per_time_table_csv(struct info_req_data *info_data, const char *label, struct per_time_stats *time_stats)
{
    char buf[1024];
    int sz, j;
    sz = snprintf(buf, sizeof(buf), "\"Time\",\"Requests\",\"Requests/second\",\"Average used servers\",\"Average running servers\",\"Average running children\"");
    ci_membuf_write(info_data->body, buf, sz, 0);
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].average) {
            sz = snprintf(buf, sizeof(buf), ",\"%s\"", InfoTimeCountersId[j].name);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_time) {
            sz = snprintf(buf, sizeof(buf), ",\"%s\"", InfoTimeCountersId[j].name);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_request) {
            sz = snprintf(buf, sizeof(buf), ",\"%s\"", InfoTimeCountersId[j].name);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    ci_membuf_write(info_data->body, "\n", 1, 0);
    sz = snprintf(buf, sizeof(buf), "%lld,%" PRIu64 ",%" PRIu64 ",%d,%d,%d", (long long)info_data->time, time_stats->requests, time_stats->requests_per_sec, time_stats->used_servers, time_stats->max_servers, time_stats->children);
    _CI_ASSERT(sz < sizeof(buf));
    ci_membuf_write(info_data->body, buf, sz, 0);

    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].average) {
            sz = snprintf(buf, sizeof(buf), ",%" PRIu64 "", time_stats->uint64_average[j]);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_time) {
            sz = snprintf(buf, sizeof(buf), ",%" PRIu64, time_stats->kbs_per_sec[j].bytes);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_request) {
            sz = snprintf(buf, sizeof(buf), ",%" PRIu64, time_stats->kbs_per_request[j].bytes);
            _CI_ASSERT(sz < sizeof(buf));
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    ci_membuf_write(info_data->body, "\n", 1, 0);
}

static void print_per_time_table(struct info_req_data *info_data, const char *label, struct per_time_stats *time_stats)
{
    int sz, j;
    struct stats_tmpl *tmpl;
    char buf[1024];

    if (info_data->format == OUT_FMT_CSV)
        return print_per_time_table_csv(info_data, label, time_stats);
    else if (info_data->format == OUT_FMT_TEXT)
        tmpl = &txt_tmpl;
    else
        tmpl = &html_tmpl;

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, label, "statistics", info_data->time_str);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Requests", time_stats->requests);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Requests/second", time_stats->requests_per_sec);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average used servers", time_stats->used_servers);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average running servers", time_stats->max_servers);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_int, "Average running children", time_stats->children);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].average) {
            sz = 0;
            if (InfoTimeCountersId[j].type == CI_STAT_TIME_US_T || InfoTimeCountersId[j].type == CI_STAT_TIME_MS_T || InfoTimeCountersId[j].type == CI_STAT_INT64_MEAN_T)
                sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_usec, InfoTimeCountersId[j].name, time_stats->uint64_average[j]);
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }
    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_time) {
            char label[256];
            snprintf(label, sizeof(label), "%s per second", InfoTimeCountersId[j].name);
            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_kbs, label, ci_kbs_kilobytes(&time_stats->kbs_per_sec[j]), ci_kbs_remainder_bytes(&time_stats->kbs_per_sec[j]));
            if (sz >= sizeof(buf))
                sz = sizeof(buf) - 1;
            ci_membuf_write(info_data->body, buf, sz, 0);
        }
    }

    for (j = 0; InfoTimeCountersId[j].name != NULL; j++) {
        if (InfoTimeCountersId[j].per_request) {
            char label[256];
            snprintf(label, sizeof(label), "%s per request", InfoTimeCountersId[j].name);
            sz = snprintf(buf, sizeof(buf), tmpl->simple_table_item_kbs, label, ci_kbs_kilobytes(&time_stats->kbs_per_request[j]), ci_kbs_remainder_bytes(&time_stats->kbs_per_request[j]));
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

static void print_per_time_stats(struct info_req_data *info_data)
{
    if (info_data->info_time_stats) {
        int k;
        struct {
            const char *label;
            struct per_time_stats *v;
        } time_servers[5] = {{"Current", &info_data->info_time_stats->_1sec},
                             {"Last 1 minute", &info_data->info_time_stats->_1min},
                             {"Last 5 minutes", &info_data->info_time_stats->_5min},
                             {"Last 30 minutes", &info_data->info_time_stats->_30min},
                             {"Last 60 minutes", &info_data->info_time_stats->_60min}};

        for (k = 0; k < 5; ++k) {
            if (!check_group(info_data->print_page, info_data->tables, time_servers[k].label))
                continue;
            print_per_time_table(info_data, time_servers[k].label, time_servers[k].v);
        }

    }
}

static void print_mempools(struct info_req_data *info_data)
{
    int sz;
    char buf[1024];
    struct stats_tmpl *tmpl;
    const char *TableName = "Memory pools";

    if (!check_group(info_data->print_page, info_data->tables, TableName))
        return;

    if (info_data->format == OUT_FMT_TEXT)
        tmpl = &txt_tmpl;
    else
        tmpl = &html_tmpl;

    struct subgroups_data mempools_data = {
        .name = "pool",
        .labels = ci_str_vector_create(1024),
        .current_row  = ci_str_array_new(2048),
        .memory_pools_master_group_id = info_data->memory_pools_master_group_id,
        .collect_stats = info_data->collect_stats,
        .format = info_data->format,
        .body = info_data->body
    };

    if (info_data->format != OUT_FMT_CSV) {
        sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, TableName, "statistics", info_data->time_str);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
    }

    ci_stat_groups_iterate(&mempools_data, print_subgroup_stat_row);

    if (info_data->format != OUT_FMT_CSV) {
        sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
    }
    ci_str_vector_destroy(mempools_data.labels);
    ci_str_array_destroy(mempools_data.current_row);
}

static void print_group_tables(struct info_req_data *info_data)
{
    ci_stat_groups_iterate(info_data, print_group_statistics);
}

struct histo_svg_user_data {
    ci_dyn_array_t *histogram;
    uint64_t max;
    uint64_t accounted;
};

static void retrieve_histo(void *data, const char *bin_label, uint64_t count)
{
    struct histo_svg_user_data *histo_data = (struct histo_svg_user_data *)data;
    _CI_ASSERT(histo_data->histogram);
    char name[32];
    snprintf(name, sizeof(name), "%s", bin_label);
    ci_dyn_array_add(histo_data->histogram, name, &count, sizeof(uint64_t));
    if (histo_data->max < count)
        histo_data->max = count;
    histo_data->accounted += count;
}

static void print_a_histo_svg(struct info_req_data *info_data, const char *histo_name, int histo_id)
{
    int sz;
    char buf[1024];
    struct histo_svg_user_data histo_data = {NULL, 0, 0};
    int bins_number = ci_stat_histo_bins_number(histo_id);
    // 64 bytes per bin, name plus 64bit counter(8 bytes), it should be enough
    histo_data.histogram = ci_dyn_array_new2(bins_number + 1, 64);
    ci_stat_histo_bins_iterate(histo_id, &histo_data, retrieve_histo);
    sz = snprintf(buf, sizeof(buf),
                  "<H1>%s histogram</H1>\n"
                  "Date: %s<BR>\n"
                  "Accounted objects: %" PRIu64 "<BR>\n",
                  histo_name, info_data->time_str, histo_data.accounted);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    sz = snprintf(buf, sizeof(buf), "<svg width=\"100%%\" height=\"%d\">\n", (bins_number + 2) * 20);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    const ci_array_item_t *bin;
    int i=0;
    for (i = 0; (bin = ci_dyn_array_get_item(histo_data.histogram, i)) != NULL; ++i) {
        static const char *colors[2] = {"lightgrey", "darkgrey"};
        const char *color = colors[i % 2];
        uint64_t count = *(uint64_t *)bin->value;
        /*USe 20% for histo bar, 10% for bin label and 10% for count*/
        count =  histo_data.max == 0 ? 0 : (count * 80) / histo_data.max;
        if (count > 0)
            sz = snprintf(buf, sizeof(buf), "<rect y=\"%u\" x=\"10%%\" width=\"%d%%\" height=\"20\" stroke=\"%s\" fill=\"%s\" />\n", i*20, (unsigned)count, color, color);
        else
            sz = snprintf(buf, sizeof(buf), "<rect y=\"%u\" x=\"10%%\" width=\"1\" height=\"20\" stroke=\"%s\" fill=\"%s\" />\n", i*20, color, color);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);

        sz = snprintf(buf, sizeof(buf),
                      "<text y=\"%u\" x=\"9%%\" text-anchor=\"end\"> %s</text>\n"
                      "<text y=\"%u\" x=\"%d%%\" > %" PRIu64 "</text>\n"
                      , (i + 1)*20, bin->name, (i + 1)*20, (int)count + 10 + 1, *(uint64_t *)bin->value);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
    }
    char svg_end[] = "</svg>\n";
    ci_membuf_write(info_data->body, svg_end, sizeof(svg_end) - 1, 0);
    ci_dyn_array_destroy(histo_data.histogram);
}

static void print_histo_bin(void *data, const char *bin_label, uint64_t count)
{
    struct info_req_data *info_data = (struct info_req_data *)data;
    int sz;
    char buf[1024];
    const char *tmpl_line = NULL;
    if (info_data->format == OUT_FMT_CSV) {
        tmpl_line = "%s, %" PRIu64 "\n";
    } else {
        struct stats_tmpl *tmpl = info_data->format == OUT_FMT_TEXT ? &txt_tmpl : &html_tmpl;
        tmpl_line = tmpl->simple_table_item_int;
    }
    _CI_ASSERT(tmpl_line);
    sz = snprintf(buf, sizeof(buf), tmpl_line, bin_label, count);
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
}

static int print_a_histo(void *data, const char *histo_name, int histo_id)
{
    struct info_req_data *info_data = (struct info_req_data *)data;
    if (!check_group(info_data->print_page, info_data->histos, histo_name))
        return 0;

    int sz;
    char buf[1024];
    struct stats_tmpl *tmpl = NULL;
    switch(info_data->format) {
    case OUT_FMT_TEXT:
        tmpl = &txt_tmpl;
        break;
    case OUT_FMT_HTML:
        if (info_data->supports_svg) {
            print_a_histo_svg(info_data, histo_name, histo_id);
            return 0;
        }
        tmpl = &html_tmpl;
        break;
    case OUT_FMT_CSV:
    default:
        tmpl = NULL;
    }

    if (tmpl) {
        sz = snprintf(buf, sizeof(buf), tmpl->simple_table_start, histo_name, "histogram", info_data->time_str);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        ci_membuf_write(info_data->body, tmpl->table_row_start, strlen(tmpl->table_row_start), 0);
        sz = snprintf(buf, sizeof(buf), tmpl->table_header, ci_stat_histo_data_descr(histo_id));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        ci_membuf_write(info_data->body, tmpl->table_col_sep, strlen(tmpl->table_col_sep), 0);
        sz = snprintf(buf, sizeof(buf), tmpl->table_header, "Count");
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
        ci_membuf_write(info_data->body, tmpl->table_row_end, strlen(tmpl->table_row_end), 0);
    } else {
        sz = snprintf(buf, sizeof(buf), "\"%s\", \"count\"\n", ci_stat_histo_data_descr(histo_id));
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
    }

    ci_stat_histo_bins_iterate(histo_id, data, print_histo_bin);

    if (tmpl) {
        sz = snprintf(buf, sizeof(buf), "%s", tmpl->simple_table_end);
        if (sz >= sizeof(buf))
            sz = sizeof(buf) - 1;
        ci_membuf_write(info_data->body, buf, sz, 0);
    }
    return 0;
}

static void print_histos(struct info_req_data *info_data)
{
    ci_stat_histo_iterate(info_data, print_a_histo);
}

static void print_link_(struct info_req_data *info_data, const char *label, const char *argument, const char *table)
{
    char buf[512];
    size_t sz;
    int hasArgs = (info_data->url && strchr(info_data->url, '?') != NULL);
    if (info_data->format == OUT_FMT_CSV)
        sz = snprintf(buf, sizeof(buf), "\"%s\", \"%s %s\"\n", label, argument, table);
    else if (info_data->format == OUT_FMT_TEXT)
        sz = snprintf(buf, sizeof(buf), "\t%s=%s': for '%s' statistics\n", argument, table, label);
    else
        sz = snprintf(buf, sizeof(buf), "<li><A href=\"%s%c%s=%s\"> %s </A></li>\n", info_data->url ? info_data->url : "", (hasArgs ? '&' : '?'), argument, table, label);

    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
}

static void print_link(struct info_req_data *info_data, const char *label, const char *table)
{
    print_link_(info_data, label, "table", table);
}

static int print_group(void *data, const char *grp_name, int group_id, int master_group_id)
{
    char url_encoded_name[256];
    char *s;
    struct info_req_data *info_data = (struct info_req_data *)data;
    if (master_group_id == CI_STAT_GROUP_MASTER)
        return 0;
    if (master_group_id == info_data->memory_pools_master_group_id)
        return 0; /*They are handled in their own table*/
    /*TODO: use a ci_url_encode function when this is will be implemented*/
    snprintf(url_encoded_name, sizeof(url_encoded_name), "%s", grp_name);
    s = url_encoded_name;
    while(*s) { if (*s == ' ') *s = '+'; s++;}
    print_link(info_data, grp_name, url_encoded_name);
    return 0;
}


static void rm_arguments(struct info_req_data *info_data, const char *argName)
{
    if (info_data->url) {
        char *histoArg = NULL;
        while((histoArg = strstr(info_data->url , argName))) {
            const char *tail = strchr(histoArg, '&');
            if (!tail)
                tail = histoArg + strlen(histoArg);
            if (*tail == '&')
                ++tail;
            size_t tailLen = strlen(tail) + 1; /*Include '\0' termination char*/
            memmove(histoArg, tail, tailLen);
            if (*histoArg == '\0' && info_data->url < histoArg) {
                --histoArg;
                if (*histoArg == '?')
                    *histoArg = '\0';
            }
        }
    }
}

static int print_histo_link(void *data, const char *name, int id)
{
    struct info_req_data *info_data = (struct info_req_data *)data;
    char buf[512];
    size_t sz;
    int hasArgs = (info_data->url && strchr(info_data->url, '?') != NULL);
    switch(info_data->format) {
    case OUT_FMT_CSV:
        sz = snprintf(buf, sizeof(buf), "\"%s\", \"histo %s\"\n", name, name);
        break;
    case OUT_FMT_TEXT:
        sz = snprintf(buf, sizeof(buf), "\thisto=%s': for '%s' histogram\n", name, name);
        break;
    case OUT_FMT_HTML:
        sz = snprintf(buf, sizeof(buf),
                      "<li>%s <A href=\"%s%chisto=%s&svg=off\"> text/html </A> or "
                      "<A href=\"%s%chisto=%s\"> graphics mode </A>"
                      "</li>\n",
                      name,
                      info_data->url ? info_data->url : "", (hasArgs ? '&' : '?'), name,
                      info_data->url ? info_data->url : "", (hasArgs ? '&' : '?'), name);
        break;
    default:
        sz = 0;
        break;
    }

    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);
    return 0;
}

static void print_histo_list(struct info_req_data *info_data)
{
    char buf[512];
    size_t sz;
    rm_arguments(info_data, "table=");
    rm_arguments(info_data, "histo=");
    if (info_data->format == OUT_FMT_CSV)
        sz = snprintf(buf, sizeof(buf), "\"Histograms\"\n");
    else if (info_data->format == OUT_FMT_TEXT)
        sz = snprintf(buf, sizeof(buf), "Histograms:\n");
    else
        sz = snprintf(buf, sizeof(buf), "<H1>Histograms</H1>\n<ul>\n");
    ci_membuf_write(info_data->body, buf, sz, 0);
    ci_stat_histo_iterate(info_data, print_histo_link);
}

static void print_menu(struct info_req_data *info_data)
{
    char buf[512];
    size_t sz;
    rm_arguments(info_data, "table=");
    rm_arguments(info_data, "histo=");
    if (info_data->format == OUT_FMT_CSV)
        sz = snprintf(buf, sizeof(buf), "\"Statistic Topic\",\"Option\"\n");
    else if (info_data->format == OUT_FMT_TEXT)
        sz = snprintf(buf, sizeof(buf), "Statistic topics:\n");
    else
        sz = snprintf(buf, sizeof(buf), "<H1>Statistic topics</H1>\n<ul>\n");
    if (sz >= sizeof(buf))
        sz = sizeof(buf) - 1;
    ci_membuf_write(info_data->body, buf, sz, 0);

    print_link(info_data, "Running servers", "Running+servers");
    print_link(info_data, "Current", "Current");
    print_link(info_data, "Last 1 minute", "Last+1+minute");
    print_link(info_data, "Last 5 minutes", "Last+5+minutes");
    print_link(info_data, "Last 30 minutes", "Last+30+minutes");
    print_link(info_data, "Last 60 minutes", "Last+60+minutes");
    print_link(info_data, "Memory pools", "Memory+pools");
    ci_stat_groups_iterate(info_data, print_group);
    print_link(info_data, "Histograms", "Histograms");
    print_link(info_data, "All", "*");

    if (info_data->format == OUT_FMT_HTML)
        ci_membuf_write(info_data->body, "</ul>\n", 6, 0);
}

/*TODO: make HTML styles configurable, probably using templates interface*/
const char HTML_HEAD[] =
    "<!DOCTYPE HTML>\n<HTML lang=\"en\">\n"
    "<HEAD>\n"
    "<TITLE>C-icap server statistics page</TITLE>\n"
    "<STYLE>\n"
    "  table {border-collapse: collapse;}\n"
    "  th,td,table {border-style:solid; border-width:1px; }\n"
    "  caption {text-align:right;}\n"
    "</STYLE>\n"
    "</HEAD>\n"
    "<BODY>\n"
    ;
const char HTML_END[] = "</BODY>\n</HTML>";

static int print_statistics(struct info_req_data *info_data)
{
    if (!info_data->body)
        return 0;
    if (info_data->format == OUT_FMT_HTML)
        ci_membuf_write(info_data->body, HTML_HEAD, sizeof(HTML_HEAD) - 1, 0);
    if (info_data->print_page == PRINT_INFO_MENU) {
        print_menu(info_data);
    } else if (info_data->print_page == PRINT_HISTOGRAMS_LIST) {
        print_histo_list(info_data);
    } else {
        fill_queue_statistics(childs_queue, info_data);
        print_running_servers_statistics(info_data);
        print_per_time_stats(info_data);
        print_mempools(info_data);
        print_group_tables(info_data);
        print_histos(info_data);
    }
    if (info_data->format == OUT_FMT_HTML)
        ci_membuf_write(info_data->body, HTML_END, sizeof(HTML_END) - 1, 1);
    else
        ci_membuf_write(info_data->body, NULL, 0, 1);
    return 1;
}

int stats_web_service(ci_request_t *req)
{
    struct info_req_data *info_data = (struct info_req_data *)info_init_request_data(req);
    info_data->supports_svg = 1;
    int url_size = sizeof(req->service) + sizeof(req->args) + 1;
    info_data->url = ci_buffer_alloc(sizeof(req->service) + sizeof(req->args));
    snprintf(info_data->url, url_size, "%s%s%s", req->service, (req->args[0] != '\0' ? "?" : ""), req->args);
    if (req->args[0] != '\0') {
        parse_info_arguments(info_data, req->args);
    }
    if (info_data->format == OUT_FMT_TEXT)
        ci_http_server_response_add_header(req, "Content-Type: text/plain");
    else if (info_data->format == OUT_FMT_CSV)
        ci_http_server_response_add_header(req, "Content-Type: text/csv");
    else
        ci_http_server_response_add_header(req, "Content-Type: text/html");
    ci_http_server_response_add_header(req, "Content-Language: en");
    print_statistics(info_data);
    info_release_request_data((void *)info_data);
    return 1;
}

struct keyval {const char *n; const char *v;};
extern struct keyval _CI_CONF_AUTOCONF[];
extern struct keyval _CI_CONF_C_ICAP_CONF[];
static int build_info_web_service(ci_request_t *req)
{
    char buf[4096];
    int i, bytes;
    ci_http_server_response_add_header(req, "Content-Type: text/html");
    ci_http_server_response_add_header(req, "Content-Language: en");
    ci_membuf_t *body = ci_http_server_response_body(req);
    ci_membuf_write(body, HTML_HEAD, sizeof(HTML_HEAD) - 1, 0);
    bytes = snprintf(buf, sizeof(buf),
                     "<H2>Build information</H2>\n"
                     "c-icap version: %s<BR>\n", VERSION);
    ci_membuf_write(body, buf, bytes, 0);
    bytes = snprintf(buf, sizeof(buf),
                     "Configure script options: %s<BR>\n",
                     C_ICAP_CONFIGURE_OPTIONS);
    ci_membuf_write(body, buf, bytes, 0);
    bytes = snprintf(buf, sizeof(buf),
                     "Configured for host: %s<BR>\n",
                     C_ICAP_CONFIG_HOST_TYPE);
    ci_membuf_write(body, buf, bytes, 0);
    bytes = snprintf(buf, sizeof(buf), "<H2> autoconf.h options </H2>\n<PRE>\n");
    ci_membuf_write(body, buf, bytes, 0);
    for (i = 0; _CI_CONF_AUTOCONF[i].n != NULL; i++) {
        bytes = snprintf(buf, sizeof(buf), "#define %s %s\n", _CI_CONF_AUTOCONF[i].n, _CI_CONF_AUTOCONF[i].v);
        ci_membuf_write(body, buf, bytes, 0);
    }
    ci_membuf_write(body, "</PRE>\n", 7, 0);
    bytes = snprintf(buf, sizeof(buf), "<H2>c-icap-conf.h </H2>\n<PRE>\n");
    ci_membuf_write(body, buf, bytes, 0);
    for (i = 0; _CI_CONF_C_ICAP_CONF[i].n != NULL; i++) {
        bytes = snprintf(buf, sizeof(buf), "#define %s %s\n", _CI_CONF_C_ICAP_CONF[i].n, _CI_CONF_C_ICAP_CONF[i].v);
        ci_membuf_write(body, buf, bytes, 0);
    }
    ci_membuf_write(body, "</PRE>\n", 7, 0);
    ci_membuf_write(body, HTML_END, sizeof(HTML_END) - 1, 1);
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
    uint64_t uint64_average[InfoTimeCountersIdLength];
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
    uint64_t uint64_average[InfoTimeCountersIdLength];
};

static void append_snapshots(struct info_time_stats_snapshot_results *result, const struct info_time_stats_snapshot *add)
{
    int i;
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
    for (i = 0; i < InfoTimeCountersIdLength; i++)
        result->uint64_average[i] += add->uint64_average[i];
}

static void build_per_time_stats(struct per_time_stats *tr, const struct info_time_stats_snapshot_results *accumulated, time_t time_range)
{
    int i;
    uint64_t requests = accumulated->requests_end - accumulated->requests_start;
    time_t period = accumulated->when_end > accumulated->when_start ? accumulated->when_end - accumulated->when_start : 1;
    if (period < time_range)
        return;
    tr->requests = requests;
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
        if (InfoTimeCountersId[i].average)
            tr->uint64_average[i] = accumulated->uint64_average[i] / accumulated->children; /*Includes accumulated->snapshot*/
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
            if (InfoTimeCountersId[k].type == CI_STAT_KBS_T) {
                ci_kbs_t kbs = ci_stat_memblock_get_kbs(q->stats_history, InfoTimeCountersId[k].id);
                snapshot->kbs_counters_instance[k] = kbs;
                minsnapshot->kbs_counters_instance[k] = kbs;
            } else {
                snapshot->uint64_average[k] = 0;
                minsnapshot->uint64_average[k] = 0;
            }
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
                    if (InfoTimeCountersId[k].type == CI_STAT_KBS_T) {
                        ci_kbs_t kbs = ci_stat_memblock_get_kbs(stats, InfoTimeCountersId[k].id);
                        ci_kbs_add_to(&snapshot->kbs_counters_instance[k], &kbs);
                        ci_kbs_add_to(&minsnapshot->kbs_counters_instance[k], &kbs);
                    } else if (InfoTimeCountersId[k].average) {
                        uint64_t val = ci_stat_memblock_get_counter(stats, InfoTimeCountersId[k].id);
                        snapshot->uint64_average[k] += val;
                        minsnapshot->uint64_average[k] += val;
                    }
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
        build_per_time_stats(&info_tstats->_1sec, &stats_1s, 1);

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
    build_per_time_stats(&info_tstats->_1min, &stats_1m, 60 - 2);

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
    build_per_time_stats(&info_tstats->_5min, &stats_5m, 300 - 10);
    build_per_time_stats(&info_tstats->_30min, &stats_30m, 1800 - 60);
    build_per_time_stats(&info_tstats->_60min, &stats_60m, 3600 - 60);
}

static void parse_info_arguments(struct info_req_data *info_data, char *args)
{
    char *s, *e;
    if (strstr(args, "view=text"))
        info_data->format = OUT_FMT_TEXT;
    else if (strstr(args, "view=csv"))
        info_data->format = OUT_FMT_CSV;
    else if (strstr(args, "view=html"))
        info_data->format = OUT_FMT_HTML;
    if (strstr(args, "table=*"))
        info_data->print_page = PRINT_ALL_TABLES;
    else if (strstr(args, "table=Histograms"))
        info_data->print_page = PRINT_HISTOGRAMS_LIST;
    if ((s = strstr(args, "child="))) {
        s += 6;
        info_data->view_child = strtol(s, NULL, 10);
    }
    if ((s = strstr(args, "svg=off")))
        info_data->supports_svg = 0;

    if (info_data->print_page != PRINT_INFO_MENU)
        return;

    s = args;
    while (s && *s) {
        if (strncmp(s, "table=", 6) == 0) {
            if (!info_data->tables)
                info_data->tables = ci_str_vector_create(1024);
            _CI_ASSERT(info_data->tables);
            if ((e = strchr(s, '&')))
                *e = '\0';
            else
                e = s + strlen(s) - 1;
            const char *table = s + 6;
            ci_str_vector_add(info_data->tables, table);
            info_data->print_page = PRINT_SOME_TABLES;
            s = e + 1;
        } else if (strncmp(s, "histo=", 6) == 0) {
            if (!info_data->histos)
                info_data->histos = ci_str_vector_create(1024);
            _CI_ASSERT(info_data->histos);
            if ((e = strchr(s, '&')))
                *e = '\0';
            else
                e = s + strlen(s) - 1;
            const char *histo = s + 6;
            ci_str_vector_add(info_data->histos, histo);
            info_data->print_page = PRINT_SOME_TABLES;
            s = e + 1;
        } else
            ++s;
    }
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
