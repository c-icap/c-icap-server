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
#include "request.h"
#include "request_util.h"
#include "ci_threads.h"
#include "client.h"
#include "net_io.h"
#if defined(USE_OPENSSL)
#include "net_io_ssl.h"
#endif
#include "cfg_param.h"
#include "debug.h"
#include "util.h"
#include "stats.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>


/*GLOBALS ........*/
int CONN_TIMEOUT = 10;
int IO_TIMEOUT = 30;
char *servername = "localhost";
int PORT = 1344;
char *service = "echo";
int threadsnum = 10;
int MAX_REQUESTS = 100;
int VERSION_MODE = 0;
int USE_DEBUG_LEVEL = -1;

int DoReqmod = 0;
int DoTransparent = 0;
#define MAX_URLS 32768
char *URLS[MAX_URLS];
int URLS_COUNT = 0;

char *BASE_URL = NULL;
static char *OUT_DIR = NULL;
int OUT_FILES_NUM = 4096;
time_t START_TIME = 0;
int FILES_NUMBER = 0;
char **FILES = NULL;

struct thread_data {
    ci_thread_t id;
    unsigned int rand_seed;
} *threads;

int pid_to_int(ci_thread_t id)
{
#if defined(_WIN32)|| defined(__CYGWIN__)
    return (int)(uintptr_t)id;
#else
    return (int)id;
#endif
}

ci_thread_mutex_t filemtx;
uint64_t RequestsCount = 0;
int file_indx = 0;
int requests_stats = 0;
int failed_requests_stats = 0;
int soft_failed_requests_stats = 0;
int in_bytes_stats = 0;
int out_bytes_stats = 0;
int in_http_bytes_stats = 0;
int out_http_bytes_stats = 0;
int in_body_bytes_stats = 0;
int out_body_bytes_stats = 0;
int allow204_stats = 0;
int allow206_stats = 0;
int _THE_END = 0;
char **xclient_headers = NULL;
int xclient_headers_num =0;
ci_headers_list_t *xheaders = NULL;
ci_headers_list_t *http_xheaders = NULL;
ci_headers_list_t *http_resp_xheaders = NULL;
#if defined(USE_OPENSSL)
int use_tls = 0;
int tls_verify = 1;
const char *tls_method = NULL;
ci_tls_pcontext_t ctx = NULL;
#endif

ci_list_t *addresses = NULL;

int print_stat(void *data, const char *label, int id, int gid, const ci_stat_t *stat)
{
    switch (stat->type) {
    case CI_STAT_INT64_T:
        printf("\t\%s: %" PRIu64 "\n", label, stat->value.counter);
        break;
    case CI_STAT_KBS_T:
        printf("\t\%s: %" PRIu64 "Kbs and %" PRIu64 "bytes\n", label, ci_kbs_kilobytes(&stat->value.kbs), ci_kbs_remainder_bytes(&stat->value.kbs));
        break;
    case CI_STAT_TIME_US_T:
        printf("\t\%s: %" PRIu64 " usec", label, stat->value.counter);
        break;
    case CI_STAT_TIME_MS_T:
        printf("\t\%s: %" PRIu64 " msec", label, stat->value.counter);
        break;
    default:
       break;
    }
    return 0;
}

static void print_stats()
{
    time_t rtime;
    time(&rtime);
    printf("Statistics:\n\tFiles used :%d\n\t Number of threads :%d\n",
           FILES_NUMBER, threadsnum);
    rtime = rtime - START_TIME;
    printf("\tRunning for %u seconds\n", (unsigned int) rtime);
    ci_stat_statistics_iterate(NULL, -1, print_stat);
}

static void sigint_handler(int sig)
{
    int i = 0;

    /* */
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    if (sig == SIGTERM) {
        printf("SIGTERM signal received for main server.\n");
        printf("Going to term children....\n");

    } else if (sig == SIGINT) {
        printf("SIGINT signal received for icap-stretch.\n");

    } else {
        printf("Signal %d received. Exiting ....\n", sig);
    }
    _THE_END = 1;
    for (i = 0; i < threadsnum; i++) {
        if (threads[i].id)
            ci_thread_join(threads[i].id);      //What if a child is blocked??????
    }

    ci_thread_mutex_destroy(&filemtx);

    print_stats();

    exit(0);
}

static void str_trim(char *str)
{
    char *s, *e;

    if (!str)
        return;

    s = str;
    e = NULL;
    while (*s == ' ') {
        e = s;
        while (*e != '\0') {
            *e = *(e+1);
            e++;
        }
    }

    /*if (e) e--;  else */
    e = str+strlen(str);
    while (*(--e) == ' ' && e >= str) *e = '\0';
}

static int load_urls(char *filename)
{
    FILE *f;
#define URL_SIZE  1024
    char line[URL_SIZE+1];

    URLS_COUNT = 0;
    memset(URLS, 0, MAX_URLS * sizeof(char *));

    if ((f = fopen(filename, "r")) == NULL) {
        printf("Error opening magic file: %s\n", filename);
        return 0;
    }

    while (fgets(line,URL_SIZE,f) != NULL && URLS_COUNT != MAX_URLS) {
        line[strlen(line)-1] = '\0';
        str_trim(line);
        if (line[0] != '#' && line[0] != '\0') {
            URLS[URLS_COUNT] = strdup(line);
            URLS_COUNT++;
        }
    }

    fclose(f);
    return 1;
}

static ci_connection_t *connect_to_server()
{
    ci_connection_t *conn = NULL;
    ci_list_iterator_t it;
    ci_sockaddr_t *addr = NULL;

    for (addr = (ci_sockaddr_t *)ci_list_iterator_first(addresses, &it); conn == NULL && addr != NULL; addr = (ci_sockaddr_t *)ci_list_iterator_next(&it)) {
#if defined(USE_OPENSSL)
        if (use_tls) {
            if (!(conn = ci_tls_connect_to_address(addr, PORT, servername, ctx, CONN_TIMEOUT))) {
                char ipStr[256];
                ci_debug_printf(2, "Failed to establish TLS connection to the address: %s:%d\n", ci_sockaddr_t_to_ip(addr, ipStr, sizeof(ipStr)), PORT);
            }
        } else
#endif

        if (!(conn = ci_connect_to_address(addr, PORT, CONN_TIMEOUT))) {
            char ipStr[256];
            ci_debug_printf(2, "Failed to connect to address: '%s:%d'\n", ci_sockaddr_t_to_ip(addr, ipStr, sizeof(ipStr)), PORT);
        }

    }

    return conn;
}

static char *xclient_header(struct thread_data *data)
{
    if (!xclient_headers_num)
        return NULL;

    int arand = rand_r(&(data->rand_seed));
    int indx = (int) ((((double) arand) / (double) RAND_MAX) * (double)xclient_headers_num);
    return xclient_headers[indx];
}

static void build_response_headers(int fd, const char *filename, ci_headers_list_t *headers)
{
    struct stat filestat;
    int filesize;
    char lbuf[512];

    ci_headers_add(headers, "HTTP/1.1 200 OK");
    ci_headers_add(headers, "Filetype: Unknown");
    ci_headers_add(headers, "User: chtsanti");

    fstat(fd, &filestat);
    filesize = filestat.st_size;

    strncpy(lbuf, "Date: ", sizeof(lbuf));
    lbuf[sizeof(lbuf) -1] = '\0';
    const size_t DATE_PREFIX_LEN = strlen(lbuf);
    ci_strntime_rfc822(lbuf + DATE_PREFIX_LEN, sizeof(lbuf) - DATE_PREFIX_LEN);
    ci_headers_add(headers, lbuf);

    strncpy(lbuf, "Last-Modified: ", sizeof(lbuf));
    lbuf[sizeof(lbuf) -1] = '\0';
    const size_t LM_PREFIX_LEN = strlen(lbuf);
    ci_to_strntime_rfc822(lbuf + LM_PREFIX_LEN, sizeof(lbuf) - LM_PREFIX_LEN, &filestat.st_mtime);
    ci_headers_add(headers, lbuf);

    snprintf(lbuf, sizeof(lbuf), "Content-Length: %d", filesize);
    ci_headers_add(headers, lbuf);
    if (http_resp_xheaders)
        ci_headers_addheaders(headers, http_resp_xheaders);
    if (filename) {
        snprintf(lbuf, sizeof(lbuf), "X-C-ICAP-Client-Original-File: %s", filename);
        ci_headers_add(headers, lbuf);
    }
}

static void build_request_headers(const char *url, const char *method, ci_headers_list_t *headers)
{
    char lbuf[8192];

    snprintf(lbuf, sizeof(lbuf), "%s %s HTTP/1.1", method, url);
    ci_headers_add(headers, lbuf);

    strncpy(lbuf, "Date: ", sizeof(lbuf));
    lbuf[sizeof(lbuf) -1] = '\0';
    const size_t DATE_PREFIX_LEN = strlen(lbuf);
    ci_strntime_rfc822(lbuf + DATE_PREFIX_LEN, sizeof(lbuf) - DATE_PREFIX_LEN);
    ci_headers_add(headers, lbuf);
    ci_headers_add(headers, "User-Agent: C-ICAP-Client/x.xx");
    if (http_xheaders)
        ci_headers_addheaders(headers, http_xheaders);
}


static int fileread(void *fd, char *buf, int len)
{
    int ret;
    ret = read(*(int *) fd, buf, len);
    if (ret == 0)
        return CI_EOF;
    return ret;
}

static int filewrite(void *fd, char *buf, int len)
{
    if (*(int *)fd >= 0) {
        ssize_t ret = write(*(int *) fd, buf, len);
        if (ret != len) {
            ci_debug_printf(1, "WARNING: Output truncated\n");
            /* An error does not make sense to continue write*/
            close(*(int *)fd);
            *(int *)fd = -1;
            /*But return len to allow stress test to be finished*/
        }
    }
    return len;
}

static int store_meta(uint64_t reqId, ci_request_t *req, ci_headers_list_t *request_headers, ci_headers_list_t *response_headers);
static int do_req(ci_request_t *req, char *url, int *keepalive, int transparent)
{
    int ret;
    char lbuf[1024];
    char host[512];
    char path[512];
    char *s;
    ci_headers_list_t *headers;
    int fd_out = 0;
    uint64_t reqId = 0;
    ci_thread_mutex_lock(&filemtx);
    reqId = RequestsCount++;
    ci_thread_mutex_unlock(&filemtx);
    headers = ci_headers_create();
    printf("URl is: %s\n", url);
    if (transparent) {
        if ((s = strchr(url, '/')) != NULL) {
            strncpy(host, url, sizeof(host) > (s-url) ? (s-url): sizeof(path));
            host[sizeof(host) > (s-url) ? (s-url): sizeof(host) - 1] = '\0';
            strncpy(path, s, sizeof(path));
            path[sizeof(path) - 1] = '\0';
        } else {
            strncpy(host, url, sizeof(host));
            host[sizeof(host) - 1] = '\0';
            strncpy(path, "/index.html", sizeof(path));
            path[sizeof(path) - 1] = '\0';
        }
        snprintf(lbuf, sizeof(lbuf), "GET %s HTTP/1.1", path);
    } else {
        if (strstr(url, "://"))
            snprintf(lbuf, sizeof(lbuf), "GET %s HTTP/1.1", url);
        else
            snprintf(lbuf, sizeof(lbuf), "GET http://%s HTTP/1.1", url);
        host[0] = '\0';
    }

    ci_headers_add(headers, lbuf);

    if (host[0] != '\0') {
        snprintf(lbuf, sizeof(lbuf), "Host: %s", host);
        ci_headers_add(headers, lbuf);
    }

    strncpy(lbuf, "Date: ", sizeof(lbuf));
    lbuf[sizeof(lbuf) - 1] = '\0';
    const size_t DATE_PREFIX_LEN = strlen(lbuf);
    ci_strntime_rfc822(lbuf + DATE_PREFIX_LEN, sizeof(lbuf) - DATE_PREFIX_LEN);
    ci_headers_add(headers, lbuf);
    ci_headers_add(headers, "User-Agent: C-ICAP-Stretch/x.xx");
    if (http_xheaders)
        ci_headers_addheaders(headers, http_xheaders);

    req->type = ICAP_REQMOD;

    ret = ci_client_icapfilter(req,
                               IO_TIMEOUT,
                               headers,
                               NULL,
                               NULL,
                               (int (*)(void *, char *, int)) fileread,
                               &fd_out,
                               (int (*)(void *, char *, int)) filewrite);


    if (ret <=0 && req->bytes_out == 0) {
        ci_debug_printf(2, "Is the ICAP connection  closed?\n");
        *keepalive = 0;
        return 0;
    }

    if (ret <= 0) {
        ci_debug_printf(1, "Error sending requests \n");
        *keepalive = 0;
        return -1;
    }

    store_meta(reqId, req, headers, NULL);

    *keepalive = req->keepalive;

    ci_headers_destroy(headers);

    ci_stat_kbs_inc(in_bytes_stats, req->bytes_in);
    ci_stat_kbs_inc(out_bytes_stats, req->bytes_out);
    ci_stat_kbs_inc(in_http_bytes_stats, req->http_bytes_in);
    ci_stat_kbs_inc(out_http_bytes_stats, req->http_bytes_out);
    ci_stat_kbs_inc(in_body_bytes_stats, req->body_bytes_in);
    ci_stat_kbs_inc(out_body_bytes_stats, req->body_bytes_out);
    if (req->return_code == 204)
        ci_stat_uint64_inc(allow204_stats, 1);
    else if (req->return_code == 206)
        ci_stat_uint64_inc(allow206_stats, 1);

    return 1;
}

static int threadjobreqmod(void *data)
{
    ci_request_t *req;
    ci_connection_t *conn;
    char *xh;
    int indx, keepalive, ret;
    int arand = 0, p;
    struct thread_data *thread_data = (struct thread_data *)data;
    time_t start;
    time(&start);
    thread_data->rand_seed = start + pid_to_int(thread_data->id);
    while (!_THE_END) {

        if (!(conn = connect_to_server())) {
            ci_debug_printf(1, "Failed to connect to icap server.....\n");
            exit(-1);
        }
        req = ci_client_request(conn, servername, service);
        req->type = ICAP_RESPMOD;
        req->preview = 512;
        req->allow206 = 1;
        req->allow204 = 1;

        for (;;) {
            xh = xclient_header(thread_data);
            if (xh)
                ci_icap_add_xheader(req, xh);

            if (xheaders)
                ci_icap_append_xheaders(req, xheaders);

            keepalive = 0;
            arand = rand_r(&(thread_data->rand_seed));
            indx = (int) ((((double) arand) / (double) RAND_MAX) * (double)URLS_COUNT);
            if ((ret = do_req(req, URLS[indx], &keepalive, DoTransparent)) <= 0) {
                if (ret == 0)
                    ci_stat_uint64_inc(soft_failed_requests_stats, 1);
                else
                    ci_stat_uint64_inc(failed_requests_stats, 1);
                ci_stat_uint64_inc(requests_stats, 1);
                printf("Request failed...\n");
                break;
            }

            ci_stat_uint64_inc(requests_stats, 1);

            if (_THE_END) {
                printf("The end: thread dying\n");
                ci_request_destroy(req);
                return 0;
            }

            if (keepalive == 0)
                break;

            arand = rand_r(&(thread_data->rand_seed));
            p = (int) ((((double) arand) / (double) RAND_MAX) * 10.0);
            if (p == 5 || p == 7 || p == 3) {    // 30% possibility ....
                //                  printf("OK, closing the connection......\n");
                break;
            }

            usleep(500000);
            ci_client_request_reuse(req);
        }
        ci_connection_hard_close(conn);
        ci_request_destroy(req);
        if (!_THE_END)
            usleep(1000000);
    }
    return 1;
}

static int createOutFile(uint64_t reqId)
{
    char out_file[CI_MAX_PATH];
    int fd_out = -1;
    if (!OUT_DIR)
        return -1;
    if (reqId > OUT_FILES_NUM)
        return -1;
    snprintf(out_file, CI_MAX_PATH, "%s/%" PRIu64 ".data", OUT_DIR, reqId);
    if ((fd_out = open(out_file, O_CREAT| O_RDWR | O_TRUNC, 0644)) < 0) {
        ci_debug_printf(1, "Error opening file %s\n", out_file);
        return -1;
    }
    return fd_out;
}

static int store_meta(uint64_t reqId, ci_request_t *req, ci_headers_list_t *request_headers, ci_headers_list_t *response_headers)
{
    char buf[65535];
    char out_file[CI_MAX_PATH];
    int fd_out = -1;
    if (!OUT_DIR)
        return 0;
    if (reqId > OUT_FILES_NUM)
        return 0;
    snprintf(out_file, CI_MAX_PATH, "%s/%" PRIu64 ".meta", OUT_DIR, reqId);
    if ((fd_out = open(out_file, O_CREAT| O_RDWR | O_TRUNC, 0644)) < 0) {
        ci_debug_printf(1, "Error opening file %s\n", out_file);
        return 0;
    }
    size_t len;
    ci_headers_list_t *modified_request_headers = req->type == ICAP_REQMOD ? ci_http_request_headers(req) : NULL;
    ci_headers_list_t *modified_response_headers = req->type == ICAP_RESPMOD ? ci_http_response_headers(req) : NULL;
    ci_headers_list_t *out_headers[] = {request_headers, response_headers, modified_request_headers, modified_response_headers};
    int i;
    ci_headers_list_t *h;
    for (i = 0 ; i < 4; ++i) {
        h = out_headers[i];
        if (!h)
            continue;
        len = ci_headers_pack_to_buffer(h, buf, sizeof(buf));
        if (len) {
            ssize_t wt = write(fd_out, buf, len);
            if (wt != len) {
                ci_debug_printf(1, "Error writing to meta file %s\n", out_file);
                break; /*abort here*/
            }
        }
    }
    close(fd_out);
    return 1;
}

static int do_file(struct thread_data *data, ci_request_t *req, uint64_t reqId, char *input_file, int *keepalive)
{
    int fd_in,fd_out;
    int ret, arand;
    int indx;
    ci_headers_list_t *headers, *request_headers = NULL;
    const char *useUrl = NULL;
    char buf[4096];

    if (BASE_URL) {
        snprintf(buf, sizeof(buf), "%s%s%s", BASE_URL, input_file[0] != '/' ? "/" : "" ,input_file);
        useUrl = buf;
    } else if (URLS_COUNT > 0) {
        arand = rand_r(&(data->rand_seed));

        indx = (int) ((((double) arand) / (double) RAND_MAX) * (double)URLS_COUNT);
        useUrl = URLS[indx];
    } else {
        snprintf(buf, sizeof(buf), "file://%s", input_file);
        useUrl = buf;
    }

    if ((fd_in = open(input_file, O_RDONLY)) < 0) {
        ci_debug_printf(1, "Error opening file %s\n", input_file);
        return 0;
    }
    fd_out = createOutFile(reqId);

    headers = ci_headers_create();
    build_response_headers(fd_in, input_file, headers);
    if (useUrl) {
        request_headers = ci_headers_create();
        build_request_headers(useUrl, "GET", request_headers);
    }

    ret = ci_client_icapfilter(req,
                               IO_TIMEOUT,
                               request_headers,
                               headers,
                               &fd_in,
                               (int (*)(void *, char *, int)) fileread,
                               &fd_out,
                               (int (*)(void *, char *, int)) filewrite);
    close(fd_in);
    if (fd_out >= 0)
        close(fd_out);

    if (ret <=0 && req->bytes_out == 0) {
        ci_debug_printf(2, "Is the ICAP connection  closed?\n");
        *keepalive = 0;
        return 0;
    }

    if (ret<= 0) {
        ci_debug_printf(1, "Error sending requests \n");
        *keepalive = 0;
        return -1;
    }
    store_meta(reqId, req, request_headers, headers);

    *keepalive = req->keepalive;

    ci_headers_destroy(headers);
    // printf("Done(%d bytes).\n",totalbytes);
    ci_stat_kbs_inc(in_bytes_stats, req->bytes_in);
    ci_stat_kbs_inc(out_bytes_stats, req->bytes_out);
    ci_stat_kbs_inc(in_http_bytes_stats, req->http_bytes_in);
    ci_stat_kbs_inc(out_http_bytes_stats, req->http_bytes_out);
    ci_stat_kbs_inc(in_body_bytes_stats, req->body_bytes_in);
    ci_stat_kbs_inc(out_body_bytes_stats, req->body_bytes_out);
    if (req->return_code == 204)
        ci_stat_uint64_inc(allow204_stats, 1);
    else if (req->return_code == 206)
        ci_stat_uint64_inc(allow206_stats, 1);
    return 1;
}


static int threadjobsendfiles(void *data)
{
    ci_request_t *req;
    ci_connection_t *conn;
    char *xh;
    int indx, keepalive, ret;
    int arand;
    uint64_t reqId = 0;

    struct thread_data *thread_data = (struct thread_data *)data;
    time_t start;
    time(&start);
    thread_data->rand_seed = start + pid_to_int(thread_data->id);

    while (1) {

        if (!(conn = connect_to_server())) {
            ci_debug_printf(1, "Failed to connect to icap server.....\n");
            exit(-1);
        }
        req = ci_client_request(conn, servername, service);

        for (;;) {
            ci_thread_mutex_lock(&filemtx);
            reqId = RequestsCount++;
            indx = file_indx;
            if (file_indx == (FILES_NUMBER - 1))
                file_indx = 0;
            else
                file_indx++;
            ci_thread_mutex_unlock(&filemtx);

            xh = xclient_header(thread_data);
            if (xh)
                ci_icap_add_xheader(req, xh);
            if (xheaders)
                ci_icap_append_xheaders(req, xheaders);

            keepalive = 0;
            req->type = ICAP_RESPMOD;
            req->preview = 512;
            req->allow206 = 1;
            req->allow204 = 1;
            if ((ret = do_file(thread_data, req, reqId, FILES[indx], &keepalive)) <= 0) {
                if (ret == 0)
                    ci_stat_uint64_inc(soft_failed_requests_stats, 1);
                else
                    ci_stat_uint64_inc(failed_requests_stats, 1);
                printf("Request failed...\n");
                break;
            }

            ci_stat_uint64_inc(requests_stats, 1);
            if (_THE_END) {
                printf("The end: thread dying\n");
                ci_request_destroy(req);
                return 0;
            }

            if (keepalive == 0)
                break;

            arand = rand_r(&(thread_data->rand_seed));
            arand = (int) ((((double) arand) / (double) RAND_MAX) * 10.0);
            if (arand == 5 || arand == 7 || arand == 3) {    // 30% possibility ....
//                  printf("OK, closing the connection......\n");
                break;
            }
//               sleep(1);
            usleep(500000);

//             printf("Keeping alive connection\n");

            ci_client_request_reuse(req);
        }
        ci_connection_hard_close(conn);
        ci_request_destroy(req);
        if (_THE_END) {
            printf("The end: thread dying ps 2\n");
            return 0;
        }

        usleep(1000000);
    }
    return 1;
}

static int add_xheader(const char *directive, const char **argv, void *setdata)
{
    ci_headers_list_t **xh = (ci_headers_list_t **)setdata;
    const char *h;

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }
    h = argv[0];

    if (!strchr(h, ':')) {
        ci_debug_printf(1, "The header :%s should have the form \"header: value\" \n", h);
        return 0;
    }

    if (*xh == NULL)
        *xh = ci_headers_create();

    ci_headers_add(*xh, h);
    return 1;
}

static int FILES_SIZE =0;
static int cfg_files_to_use(const char *directive, const char **argv, void *setdata)
{
    assert ((void *)FILES == *(void **)setdata);

    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }

    if (FILES == NULL) {
        FILES_SIZE = 1024;
        FILES = malloc(FILES_SIZE*sizeof(char*));
    }
    if (FILES_NUMBER == FILES_SIZE) {
        FILES_SIZE += 1024;
        FILES = realloc(FILES, FILES_SIZE*sizeof(char*));
    }
    FILES[FILES_NUMBER++] = strdup(argv[0]);
    ci_debug_printf(1, "Append file %s to file list\n", argv[0]);
    return 1;
}

static int add_xclient_headers(const char *directive, const char **argv, void *setdata)
{
    int ip1, ip2, ip3, ip4_start, ip4_end, i;
    const char *ip, *s;
    char *e;
    char buf[256];
    if (argv == NULL || argv[0] == NULL) {
        ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
        return 0;
    }
    ip = argv[0];
    if (xclient_headers == NULL)
        xclient_headers = malloc(256*sizeof(char*));

    /*I am expecting something like this:
      192.168.1.1-15
     */
    s = ip;
    ip1 = strtol(s, &e, 10);
    if (*e != '.')
        return 0;

    s = e+1;
    ip2 = strtol(s, &e, 10);
    if (*e != '.')
        return 0;
    s = e+1;
    ip3 =  strtol(s, &e, 10);
    if (*e != '.')
        return 0;
    s = e+1;
    ip4_start =  strtol(s, &e, 10);
    if (*e == '-') {
        s = e+1;
        ip4_end =  strtol(s, &e, 10);
    } else
        ip4_end = ip4_start;
    for (i = ip4_start; i <= ip4_end; i++) {
        snprintf(buf, sizeof(buf), "X-Client-IP: %d.%d.%d.%d", ip1, ip2,ip3,i);
        xclient_headers[xclient_headers_num++] = strdup(buf);
    }
    return 1;
}

static char *urls_file = NULL;


static struct ci_options_entry options[] = {
    {"-V", NULL, &VERSION_MODE, ci_cfg_version, "Print version and exits"},
    {"-VV", NULL, &VERSION_MODE, ci_cfg_build_info, "Print version and build informations and exits"},
    {"-i", "icap_servername", &servername, ci_cfg_set_str, "The ICAP server to use"},
    {"-p", "port", &PORT, ci_cfg_set_int, "The ICAP server port"},
    {"-s", "service", &service, ci_cfg_set_str, "The service name"},
#if defined(USE_OPENSSL)
    {"-tls", NULL, &use_tls, ci_cfg_enable, "Use TLS"},
    {"-tls-method", "tls_method", &tls_method, ci_cfg_set_str, "Use TLS method"},
    {"-tls-no-verify", NULL, &tls_verify, ci_cfg_disable, "Disable server certificate verify"},
#endif

    {
        "-urls", "filename", &urls_file, ci_cfg_set_str,
        "File with HTTP urls to use for stress test"
    },
    {
        "-bU", "base_url", &BASE_URL, ci_cfg_set_str,
        "Base URL  to use for respmod stress test urls computation"
    },
    {
        "-req", NULL, &DoReqmod, ci_cfg_enable,
        "Send a request modification instead of response modification requests"
    },
    {
        "-m", "max-requests", &MAX_REQUESTS, ci_cfg_set_int,
        "the maximum requests to send"
    },
    {
        "-t", "threads-number", &threadsnum, ci_cfg_set_int,
        "number of threads to start"
    },
    {
        "-d", "level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "debug level info to stdout"
    },
//     {"-nopreview", NULL, &send_preview, ci_cfg_disable, "Do not send preview data"},
    {"-x", "xheader", &xheaders, add_xheader, "Include the 'xheader' in icap request headers"},
    {"-hx", "xheader", &http_xheaders, add_xheader, "Include the 'xheader' in http request headers"},
    {"-rhx", "xheader", &http_resp_xheaders, add_xheader, "Include the 'xheader' in http response headers"},
    {"-hcx", "X-Client-IP", &xclient_headers, add_xclient_headers, "Include this X-Client-IP header in request"},
//     {"-w", "preview", &preview_size, ci_cfg_set_int, "Sets the maximum preview data size"},
    {"-O", "OutputDirectory", &OUT_DIR, ci_cfg_set_str, "Output metadata and HTTP responses body data under this directory"},
    {"--max-requests-to-save", "files-number", &OUT_FILES_NUM, ci_cfg_set_int, "The maximum requests to save when the -O parameter is used (by default no more than 4096 requests are saved)"},
    {"$$", NULL, &FILES, cfg_files_to_use, "files to send"},
    {NULL, NULL, NULL, NULL}
};

#if ! defined(_WIN32)
static void log_errors(ci_request_t * req, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

#else
static void vlog_errors(ci_request_t * req, const char *format, va_list ap)
{
    vfprintf(stderr, format, ap);
}
#endif

int main(int argc, char **argv)
{
    int i;
    ci_client_library_init();
    CI_DEBUG_LEVEL = 1;        /*Default debug level is 1 */

    int ret = ci_args_apply(argc, argv, options);
    if (VERSION_MODE)
        exit(0);
    if (!ret || (DoReqmod != 0 && urls_file == NULL)
            || (DoReqmod == 0 && FILES == NULL)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

#if ! defined(_WIN32)
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */
#else
    __vlog_error = vlog_errors;        /*set c-icap library  log function for win32..... */
#endif

#if defined(USE_OPENSSL)
    if (use_tls) {
        ci_tls_client_options_t tlsOpts;
        ci_tls_init();

        memset((void *)&tlsOpts, 0, sizeof(ci_tls_client_options_t));
        tlsOpts.method = tls_method;
        tlsOpts.verify = tls_verify;
        ctx = ci_tls_create_context(&tlsOpts);
    }
#endif


    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_handler);

    time(&START_TIME);

    if (urls_file && !load_urls(urls_file)) {
        ci_debug_printf(1, "The file contains URL list %s does not exist\n", urls_file);
        exit(1);
    }

    addresses = ci_host_get_addresses(servername);
    if (!addresses) {
        ci_debug_printf(1, "Failed to retrieve ip addresses for '%s'.\n", servername);
        exit(-1);
    }

    requests_stats = ci_stat_entry_register("Requests", CI_STAT_INT64_T, "c-icap-stretch");
    failed_requests_stats = ci_stat_entry_register("Failed Requests", CI_STAT_INT64_T, "c-icap-stretch");
    soft_failed_requests_stats = ci_stat_entry_register("Soft Failed Requests", CI_STAT_INT64_T, "c-icap-stretch");
    in_bytes_stats = ci_stat_entry_register("Incoming bytes", CI_STAT_KBS_T, "c-icap-stretch");
    out_bytes_stats = ci_stat_entry_register("Outgoing bytes", CI_STAT_KBS_T, "c-icap-stretch");
    in_http_bytes_stats = ci_stat_entry_register("Incoming HTTP bytes", CI_STAT_KBS_T, "c-icap-stretch");
    out_http_bytes_stats = ci_stat_entry_register("Outgoing HTTP bytes", CI_STAT_KBS_T, "c-icap-stretch");
    in_body_bytes_stats = ci_stat_entry_register("Incoming Body bytes", CI_STAT_KBS_T, "c-icap-stretch");
    out_body_bytes_stats = ci_stat_entry_register("Outgoing Body bytes", CI_STAT_KBS_T, "c-icap-stretch");
    allow204_stats = ci_stat_entry_register("Allow 204 responses", CI_STAT_INT64_T, "c-icap-stretch");
    allow206_stats = ci_stat_entry_register("Allow 206 responses", CI_STAT_INT64_T, "c-icap-stretch");
    ci_stat_allocate_mem();

    threads = malloc(sizeof(struct thread_data) * threadsnum);
    if (!threads) {
        ci_debug_printf(1, "Error allocation memory for threads array\n");
        exit(-1);
    }
    for (i = 0; i < threadsnum; i++)
        threads[i].id = 0;

    ci_thread_mutex_init(&filemtx);
    if (DoReqmod) {
        for (i = 0; i < threadsnum; i++) {
            printf("Create thread %d\n", i);
            ci_thread_create(&(threads[i].id),
                             (void *(*)(void *)) threadjobreqmod,
                             (void *) &(threads[i].id) /*data*/);
            sleep(1);
        }
    } else {
        printf("Files to send:%d\n", FILES_NUMBER);
        for (i = 0; i < threadsnum; i++) {
            printf("Create thread %d\n", i);
            ci_thread_create(&(threads[i].id),
                             (void *(*)(void *)) threadjobsendfiles, &(threads[i]));
//             sleep(1);
        }
    }

    _CI_ATOMIC_TYPE uint64_t *requests_finished = ci_stat_uint64_ptr(requests_stats);
    assert(requests_finished);
    while (1) {
        sleep(1);
        if (MAX_REQUESTS && *requests_finished >= MAX_REQUESTS) {
            printf("Oops max requests reached. Exiting .....\n");
            _THE_END = 1;
            break;
        }
        print_stats();
    }



    for (i = 0; i < threadsnum; i++) {
        ci_thread_join(threads[i].id);
        printf("Thread %d exited\n", i);
    }

    print_stats();
    ci_thread_mutex_destroy(&filemtx);
    ci_client_library_release();
    return 0;
}
