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
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"
#include "cfg_param.h"
#include <clamav.h>
#include "srv_clamav.h"
#include "filetype.h"
#include "ci_threads.h"
#include "include/commands.h"
#include <errno.h>

#ifdef HAVE_LIBCLAMAV_09X
#define CL_ENGINE struct cl_engine
#else
#define CL_ENGINE struct cl_node
#endif

int must_scanned(int type, av_req_data_t * data);

struct virus_db {
     CL_ENGINE *db;
     int refcount;
};

struct cl_limits limits;

struct virus_db *virusdb;
struct virus_db *old_virusdb = NULL;
ci_thread_mutex_t db_mutex;


void generate_error_page(av_req_data_t * data, request_t * req);
char *srvclamav_compute_name(request_t * req);
/***********************************************************************************/
/* Module definitions                                                              */

static int SEND_PERCENT_BYTES = 0;      /* Can send all bytes that has received without checked */
static int ALLOW204 = 0;
static ci_off_t MAX_OBJECT_SIZE = 0;
static ci_off_t START_SEND_AFTER = 0;

static struct ci_magics_db *magic_db = NULL;
static int *scantypes = NULL;
static int *scangroups = NULL;

/*char *VIR_SAVE_DIR="/srv/www/htdocs/downloads/";
  char *VIR_HTTP_SERVER="http://fortune/cgi-bin/get_file.pl?usename=%f&file="; */

char *VIR_SAVE_DIR = NULL;
char *VIR_HTTP_SERVER = NULL;
int VIR_UPDATE_TIME = 15;

/*srv_clamav service extra data ... */
service_extra_data_t *srv_clamav_xdata = NULL;


int srvclamav_init_service(service_extra_data_t * srv_xdata,
                           struct icap_server_conf *server_conf);
void srvclamav_close_service(service_module_t * this);
int srvclamav_check_preview_handler(char *preview_data, int preview_data_len,
                                    request_t *);
int srvclamav_end_of_data_handler(request_t *);
void *srvclamav_init_request_data(service_module_t * serv, request_t * req);
void srvclamav_release_request_data(void *data);
int srvclamav_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                 request_t * req);

/*Arguments parse*/
void srvclamav_parse_args(av_req_data_t * data, char *args);
/*Configuration Functions*/
int cfg_ScanFileTypes(char *directive, char **argv, void *setdata);
int cfg_SendPercentBytes(char *directive, char **argv, void *setdata);
int cfg_ClamAvTmpDir(char *directive, char **argv, void *setdata);
/*Commands functions*/
void dbreload_command(char *name, int type, char **argv);
/*General functions*/
int get_filetype(request_t * req, char *buf, int len);
int init_virusdb();
CL_ENGINE *get_virusdb();
void release_virusdb(CL_ENGINE *);
void destroy_virusdb();
void set_istag(service_extra_data_t * srv_xdata);

/*It is dangerous to pass directly fields of the limits structure in conf_variables,
  becouse in the feature some of this fields will change type (from int to unsigned int 
  or from long to long long etc)
  I must use global variables and use the post_init_service function to fill the 
  limits structure.
  But, OK let it go for the time ....
*/

/*Configuration Table .....*/
static struct conf_entry conf_variables[] = {
     {"SendPercentData", NULL, cfg_SendPercentBytes, NULL},
     {"ScanFileTypes", NULL, cfg_ScanFileTypes, NULL},
     {"MaxObjectSize", &MAX_OBJECT_SIZE, ci_cfg_size_off, NULL},
     {"StartSendPercentDataAfter", &START_SEND_AFTER, ci_cfg_size_off, NULL},
     {"Allow204Responces", &ALLOW204, ci_cfg_onoff, NULL},
     {"ClamAvMaxRecLevel", &limits.maxreclevel, ci_cfg_set_int, NULL},
     {"ClamAvMaxFilesInArchive", &limits.maxfiles, ci_cfg_set_int, NULL},
/*     {"ClamAvBzipMemLimit",NULL,setBoolean,NULL},*/
     {"ClamAvMaxFileSizeInArchive", &limits.maxfilesize, ci_cfg_size_long,
      NULL},
     {"ClamAvTmpDir", NULL, cfg_ClamAvTmpDir, NULL},
#ifdef VIRALATOR_MODE
     {"VirSaveDir", &VIR_SAVE_DIR, ci_cfg_set_str, NULL},
     {"VirHTTPServer", &VIR_HTTP_SERVER, ci_cfg_set_str, NULL},
     {"VirUpdateTime", &VIR_UPDATE_TIME, ci_cfg_set_int, NULL},
     {"VirScanFileTypes", NULL, cfg_ScanFileTypes, NULL},
#endif
     {NULL, NULL, NULL, NULL}
};


CI_DECLARE_MOD_DATA service_module_t service = {
     "srv_clamav",              /*Module name */
     "Clamav/Antivirus service",        /*Module short description */
     ICAP_RESPMOD | ICAP_REQMOD,        /*Service type responce or request modification */
     srvclamav_init_service,    /*init_service. */
     NULL,                      /*post_init_service. */
     srvclamav_close_service,   /*close_service */
     srvclamav_init_request_data,       /*init_request_data. */
     srvclamav_release_request_data,    /*release request data */
     srvclamav_check_preview_handler,
     srvclamav_end_of_data_handler,
     srvclamav_io,
     conf_variables,
     NULL
};



int srvclamav_init_service(service_extra_data_t * srv_xdata,
                           struct icap_server_conf *server_conf)
{
     int ret, i;
     magic_db = server_conf->MAGIC_DB;
     scantypes = (int *) malloc(ci_magic_types_num(magic_db) * sizeof(int));
     scangroups = (int *) malloc(ci_magic_groups_num(magic_db) * sizeof(int));

     for (i = 0; i < ci_magic_types_num(magic_db); i++)
          scantypes[i] = 0;
     for (i = 0; i < ci_magic_groups_num(magic_db); i++)
          scangroups[i] = 0;


     ci_debug_printf(10, "Going to initialize srvclamav\n");
     ret = init_virusdb();
     if (!ret)
          return 0;
     srv_clamav_xdata = srv_xdata;      /*Needed by db_reload command */
     set_istag(srv_clamav_xdata);
     ci_service_set_preview(srv_xdata, 1024);
     ci_service_enable_204(srv_xdata);
     ci_service_set_transfer_preview(srv_xdata, "*");
     memset(&limits, 0, sizeof(struct cl_limits));
     limits.maxfiles = 0 /*1000 */ ;    /* max files */
     limits.maxfilesize = 100 * 1048576;        /* maximal archived file size == 100 Mb */
     limits.maxreclevel = 5;    /* maximal recursion level */
     limits.maxratio = 200;     /* maximal compression ratio */
     limits.archivememlim = 0;  /* disable memory limit for bzip2 scanner */

     /*initialize service commands */
     register_command("srv_clamav:dbreload", MONITOR_PROC_CMD | CHILDS_PROC_CMD,
                      dbreload_command);

     return 1;
}


void srvclamav_close_service(service_module_t * this)
{
     free(scantypes);
     free(scangroups);
     destroy_virusdb();
}



void *srvclamav_init_request_data(service_module_t * serv, request_t * req)
{
     int preview_size;
     av_req_data_t *data;

     preview_size = ci_req_preview_size(req);

     if (req->args) {
          ci_debug_printf(5, "service arguments:%s\n", req->args);
     }
     if (ci_req_hasbody(req)) {
          ci_debug_printf(8, "Request type: %d. Preview size:%d\n", req->type,
                          preview_size);
          if (!(data = malloc(sizeof(av_req_data_t)))) {
               ci_debug_printf(1,
                               "Error allocation memory for service data!!!!!!!");
               return NULL;
          }
          data->body = NULL;
          data->error_page = NULL;
          data->virus_name = NULL;
          data->must_scanned = SCAN;
          data->virus_check_done = 0;
          if (ALLOW204)
               data->args.enable204 = 1;
          else
               data->args.enable204 = 0;
          data->args.forcescan = 0;
          data->args.sizelimit = 1;
          data->args.mode = 0;

          if (req->args) {
               ci_debug_printf(5, "service arguments:%s\n", req->args);
               srvclamav_parse_args(data, req->args);
          }
          if (data->args.enable204 && ci_allow204(req))
               data->allow204 = 1;
          else
               data->allow204 = 0;
          data->req = req;
          return data;
     }
     return NULL;
}


void srvclamav_release_request_data(void *data)
{
     if (data) {
          ci_debug_printf(8, "Releaseing srv_clamav data.....\n");
#ifdef VIRALATOR_MODE
          if (((av_req_data_t *) data)->must_scanned == VIR_SCAN) {
               ci_simple_file_release(((av_req_data_t *) data)->body);
               if (((av_req_data_t *) data)->requested_filename)
                    free(((av_req_data_t *) data)->requested_filename);
          }
          else
#endif
          if (((av_req_data_t *) data)->body)
               ci_simple_file_destroy(((av_req_data_t *) data)->body);

          if (((av_req_data_t *) data)->error_page)
               ci_membuf_free(((av_req_data_t *) data)->error_page);

          if (((av_req_data_t *) data)->virus_name)
               free(((av_req_data_t *) data)->virus_name);
          free(data);
     }
}


int srvclamav_check_preview_handler(char *preview_data, int preview_data_len,
                                    request_t * req)
{
     ci_off_t content_size = 0;
     int file_type;
     av_req_data_t *data = ci_service_data(req);

     ci_debug_printf(9, "OK The preview data size is %d\n", preview_data_len);

     if (!data || !ci_req_hasbody(req))
          return CI_MOD_ALLOW204;

     /*Going to determine the file type,get_filetype can take preview_data as null ....... */
     file_type = get_filetype(req, preview_data, preview_data_len);
     if ((data->must_scanned = must_scanned(file_type, data)) == 0) {
          ci_debug_printf(8, "Not in \"must scanned list\".Allow it...... \n");
          return CI_MOD_ALLOW204;
     }

     content_size = ci_content_lenght(req);
#ifdef VIRALATOR_MODE
     /*Lets see ........... */
     if (data->must_scanned == VIR_SCAN && ci_req_type(req) == ICAP_RESPMOD) {
          init_vir_mode_data(req, data);
          data->expected_size = content_size;
     }
     else {
#endif

          if (data->args.sizelimit && MAX_OBJECT_SIZE
              && content_size > MAX_OBJECT_SIZE) {
               ci_debug_printf(1,
                               "Object size is %" PRINTF_OFF_T " ."
                               " Bigger than max scannable file size (%"
                               PRINTF_OFF_T "). Allow it.... \n", content_size,
                               MAX_OBJECT_SIZE);
               return CI_MOD_ALLOW204;
          }

          data->body = ci_simple_file_new(content_size);

          if (SEND_PERCENT_BYTES >= 0 && START_SEND_AFTER == 0) {
               ci_req_unlock_data(req); /*Icap server can send data before all body has received */
               /*Let ci_simple_file api to control the percentage of data.For the beggining no data can send.. */
               ci_simple_file_lock_all(data->body);
          }
#ifdef VIRALATOR_MODE
     }
#endif
     if (!data->body)           /*Memory allocation or something else ..... */
          return CI_ERROR;

     if (preview_data_len) {
          ci_simple_file_write(data->body, preview_data, preview_data_len,
                               ci_req_hasalldata(req));
     }
     return CI_MOD_CONTINUE;
}



int srvclamav_read_from_net(char *buf, int len, int iseof, request_t * req)
{
     /*We can put here scanning hor jscripts and html and raw data ...... */
     int allow_transfer;
     av_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

     if (data->must_scanned == NO_SCAN
#ifdef VIRALATOR_MODE
         || data->must_scanned == VIR_SCAN
#endif
         ) {                    /*if must not scanned then simply write the data and exit..... */
          return ci_simple_file_write(data->body, buf, len, iseof);
     }

     if (data->args.sizelimit
         && ci_simple_file_size(data->body) >= MAX_OBJECT_SIZE) {
          data->must_scanned = 0;
          ci_req_unlock_data(req);      /*Allow ICAP to send data before receives the EOF....... */
          ci_simple_file_unlock_all(data->body);        /*Unlock all body data to continue send them..... */
     }                          /*else Allow transfer SEND_PERCENT_BYTES of the data */
     else if (data->args.mode != 1 &&   /*not in the simple mode */
              SEND_PERCENT_BYTES
              && START_SEND_AFTER < ci_simple_file_size(data->body)) {
          ci_req_unlock_data(req);
          allow_transfer =
              (SEND_PERCENT_BYTES * (data->body->endpos + len)) / 100;
          ci_simple_file_unlock(data->body, allow_transfer);
     }
     return ci_simple_file_write(data->body, buf, len, iseof);
}



int srvclamav_write_to_net(char *buf, int len, request_t * req)
{
     int bytes;
     av_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

#ifdef VIRALATOR_MODE
     if (data->must_scanned == VIR_SCAN) {
          return send_vir_mode_page(data, buf, len, req);
     }
#endif

     if (data->virus_name != NULL && data->error_page == 0) {
          /*Inform user. Q:How? Maybe with a mail...... */
          return CI_EOF;        /* Do not send more data if a virus found and data has sent (readpos!=0) */
     }
     /*if a virus found and no data sent, an inform page has already generated */

     if (data->error_page)
          return ci_membuf_read(data->error_page, buf, len);

     bytes = ci_simple_file_read(data->body, buf, len);
     return bytes;
}

int srvclamav_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                 request_t * req)
{
     int ret = CI_OK;
     if (wbuf && wlen) {
          *wlen = srvclamav_read_from_net(wbuf, *wlen, iseof, req);
          if (*wlen < 0)
               ret = CI_OK;
     }
     else if (iseof)
          srvclamav_read_from_net(NULL, 0, iseof, req);
     if (rbuf && rlen) {
          *rlen = srvclamav_write_to_net(rbuf, *rlen, req);
     }
     return CI_OK;
}

int srvclamav_end_of_data_handler(request_t * req)
{
     av_req_data_t *data = ci_service_data(req);
     CL_ENGINE *vdb;
     ci_simple_file_t *body;
     const char *virname;
     int ret = 0;
     unsigned long scanned_data = 0;

     if (!data || !data->body)
          return CI_MOD_DONE;

     body = data->body;
     data->virus_check_done = 1;
     if (data->must_scanned == NO_SCAN) {       /*If exceeds the MAX_OBJECT_SIZE for example ......  */
          ci_simple_file_unlock_all(body);      /*Unlock all data to continue send them . Not really needed here.... */
          return CI_MOD_DONE;
     }


     ci_debug_printf(8, "Scan from file\n");
     lseek(body->fd, 0, SEEK_SET);
     vdb = get_virusdb();
     ret =
         cl_scandesc(body->fd, &virname, &scanned_data, vdb, &limits,
                     CL_SCAN_STDOPT);
     if (ret == CL_VIRUS)
          data->virus_name = strdup(virname);
     release_virusdb(vdb);

     ci_debug_printf(9,
                     "Clamav engine scanned %lu blocks of  data. Data size: %"
                     PRINTF_OFF_T "...\n", scanned_data, body->endpos);

     if (ret == CL_VIRUS) {
          ci_debug_printf(1, "VIRUS DETECTED:%s.\nTake action.......\n ",
                          data->virus_name);
          if (!ci_req_sent_data(req))   /*If no data had sent we can send an error page  */
               generate_error_page(data, req);
          else if (data->must_scanned == VIR_SCAN) {
               endof_data_vir_mode(data, req);
          }
          else
               ci_debug_printf(3, "Simply not send other data\n");
          return CI_MOD_DONE;
     }
     else if (ret != CL_CLEAN) {
          ci_debug_printf(1,
                          "srvClamAv module:An error occured while scanning the data\n");
     }

     if (data->must_scanned == VIR_SCAN) {
          endof_data_vir_mode(data, req);
     }
     else if (data->allow204 && !ci_req_sent_data(req)) {
          ci_debug_printf(7, "srvClamAv module: Respond with allow 204\n");
          return CI_MOD_ALLOW204;
     }

     ci_simple_file_unlock_all(body);   /*Unlock all data to continue send them..... */
     ci_debug_printf(7,
                     "file unlocked, flags :%d (unlocked:%" PRINTF_OFF_T ")\n",
                     body->flags, body->unlocked);
     return CI_MOD_DONE;
}



/*******************************************************************************/
/* Other  functions                                                            */

int init_virusdb()
{
     int ret;
     unsigned int no = 0;
     virusdb = malloc(sizeof(struct virus_db));
     memset(virusdb, 0, sizeof(struct virus_db));
     if (!virusdb)
          return 0;
#ifdef HAVE_LIBCLAMAV_09X
     if ((ret = cl_load(cl_retdbdir(), &(virusdb->db), &no, CL_DB_STDOPT))) {
          ci_debug_printf(1, "Clamav DB reload: cl_load failed: %s\n",
                          cl_strerror(ret));
#else
     if ((ret = cl_loaddbdir(cl_retdbdir(), &(virusdb->db), &no))) {
          ci_debug_printf(1, "cl_loaddbdir: %s\n", cl_perror(ret));
#endif
          return 0;
     }
     if ((ret = cl_build(virusdb->db))) {
          ci_debug_printf(1, "Database initialization error: %s\n",
                          cl_strerror(ret));
          cl_free(virusdb->db);
          free(virusdb);
          virusdb = NULL;
          return 0;
     }
     ci_thread_mutex_init(&db_mutex);
     virusdb->refcount = 1;
     old_virusdb = NULL;
     return 1;
}

/*
  Instead of using struct virus_db and refcount's someone can use the cl_dup function
  of clamav library, but it is  undocumented so I did not use it.
  The following implementation we are starting to reload clamav db while threads are 
  scanning for virus but we are not allow any child to start a new scan until we are 
  loading DB.
*/
/*#define DB_NO_FULL_LOCK 1*/
#undef DB_NO_FULL_LOCK
int reload_virusdb()
{
     struct virus_db *vdb = NULL;
     int ret;
     unsigned int no = 0;
     ci_thread_mutex_lock(&db_mutex);
     if (old_virusdb) {
          ci_debug_printf(1, "Clamav DB reload pending, canceling.\n");
          ci_thread_mutex_unlock(&db_mutex);
          return 0;
     }
#ifdef DB_NO_FULL_LOCK
     ci_thread_mutex_unlock(&db_mutex);
#endif
     vdb = malloc(sizeof(struct virus_db));
     if (!vdb)
          return 0;
     memset(vdb, 0, sizeof(struct virus_db));
     ci_debug_printf(9, "db_reload going to load db\n");
#ifdef HAVE_LIBCLAMAV_09X
     if ((ret = cl_load(cl_retdbdir(), &(vdb->db), &no, CL_DB_STDOPT))) {
          ci_debug_printf(1, "Clamav DB reload: cl_load failed: %s\n",
                          cl_strerror(ret));
#else
     if ((ret = cl_loaddbdir(cl_retdbdir(), &(vdb->db), &no))) {
          ci_debug_printf(1, "Clamav DB reload: cl_loaddbdir failed: %s\n",
                          cl_perror(ret));
#endif
          return 0;
     }
     ci_debug_printf(9, "loaded. Going to build\n");
     if ((ret = cl_build(vdb->db))) {
          ci_debug_printf(1,
                          "Clamav DB reload: Database initialization error: %s\n",
                          cl_strerror(ret));
          cl_free(vdb->db);
          free(vdb);
          vdb = NULL;
#ifdef DB_NO_FULL_LOCK
          /*no lock needed */
#else
          ci_thread_mutex_unlock(&db_mutex);
#endif
          return 0;
     }
     ci_debug_printf(9, "Done releasing.....\n");
#ifdef DB_NO_FULL_LOCK
     ci_thread_mutex_lock(&db_mutex);
#endif
     old_virusdb = virusdb;
     old_virusdb->refcount--;
     ci_debug_printf(9, "Old VirusDB refcount:%d\n", old_virusdb->refcount);
     if (old_virusdb->refcount <= 0) {
          cl_free(old_virusdb->db);
          free(old_virusdb);
          old_virusdb = NULL;
     }
     virusdb = vdb;
     virusdb->refcount = 1;
     ci_thread_mutex_unlock(&db_mutex);
     return 1;
}

CL_ENGINE *get_virusdb()
{
     struct virus_db *vdb;
     ci_thread_mutex_lock(&db_mutex);
     vdb = virusdb;
     vdb->refcount++;
     ci_thread_mutex_unlock(&db_mutex);
     return vdb->db;
}

void release_virusdb(CL_ENGINE * db)
{
     ci_thread_mutex_lock(&db_mutex);
     if (virusdb && db == virusdb->db)
          virusdb->refcount--;
     else if (old_virusdb && (db == old_virusdb->db)) {
          old_virusdb->refcount--;
          ci_debug_printf(9, "Old VirusDB refcount:%d\n",
                          old_virusdb->refcount);
          if (old_virusdb->refcount <= 0) {
               cl_free(old_virusdb->db);
               free(old_virusdb);
               old_virusdb = NULL;
          }
     }
     else {
          ci_debug_printf(1,
                          "BUG in srv_clamav service! please contact the author\n");
     }
     ci_thread_mutex_unlock(&db_mutex);
}

void destroy_virusdb()
{
     if (virusdb) {
          cl_free(virusdb->db);
          free(virusdb);
          virusdb = NULL;
     }
     if (old_virusdb) {
          cl_free(old_virusdb->db);
          free(old_virusdb);
          old_virusdb = NULL;
     }
}

void set_istag(service_extra_data_t * srv_xdata)
{
     char istag[SERVICE_ISTAG_SIZE + 1];
     char str_version[64];
     char *daily_path;
     char *s1, *s2;
     struct cl_cvd *d1;
     int version = 0, cfg_version = 0;
     struct stat daily_stat;

     /*instead of 128 should be strlen("/daily.inc/daily.info")+1*/
     daily_path = malloc(strlen(cl_retdbdir()) + 128);
     if (!daily_path)           /*???????? */
          return;
     sprintf(daily_path, "%s/daily.cvd", cl_retdbdir());
     
     if(stat(daily_path,&daily_stat) != 0){
	 /* if the clamav_lib_path/daily.cvd does not exists
	    try to use the clamav_lib_path/daily.inc/daly.info file instead" */
	 sprintf(daily_path, "%s/daily.inc/daily.info", cl_retdbdir());
     }

     if ((d1 = cl_cvdhead(daily_path))) {
          version = d1->version;
          free(d1);
     }
     free(daily_path);

     s1 = (char *) cl_retver();
     s2 = str_version;
     while (*s1 != '\0' && s2 - str_version < 64) {
          if (*s1 != '.') {
               *s2 = *s1;
               s2++;
          }
          s1++;
     }
     *s2 = '\0';
     /*cfg_version maybe must set by user when he is changing 
        the srv_clamav configuration.... */
     snprintf(istag, SERVICE_ISTAG_SIZE, "-%.3d-%s-%d%d",
              cfg_version, str_version, cl_retflevel(), version);
     istag[SERVICE_ISTAG_SIZE] = '\0';
     ci_service_set_istag(srv_xdata, istag);
}

/* Content-Encoding: gzip*/
int ci_extend_filetype(struct ci_magics_db *db,
                       request_t * req, char *buf, int len, int *iscompressed);

int get_filetype(request_t * req, char *buf, int len)
{
     int iscompressed, filetype;
     filetype = ci_extend_filetype(magic_db, req, buf, len, &iscompressed);
/*     if iscompressed we do not care becouse clamav can understand zipped objects*/

/*     Yes but what about deflate compression as encoding ??????
       I don't know, maybe we can modify web-client requests to not send
       deflate method to Accept-Encoding header  :( .
       Or decompress internally the file and pass to the 
       clamav the decompressed data....
*/
     return filetype;
}

int must_scanned(int file_type, av_req_data_t * data)
{
     int type, i;
     int *file_groups;
     file_groups = ci_data_type_groups(magic_db, file_type);
     type = NO_SCAN;
     i = 0;
     while (file_groups[i] >= 0 && i < MAX_GROUPS) {
          if ((type = scangroups[file_groups[i]]) > 0)
               break;
          i++;
     }

     if (type == NO_SCAN)
          type = scantypes[file_type];

     if (type == 0 && data->args.forcescan)
          type = SCAN;
     else if (type == VIR_SCAN && data->args.mode == 1) /*in simple mode */
          type = SCAN;

     return type;
}

static const char *clamav_error_message = "<H1>VIRUS FOUND</H1>\n\n"
    "You try to upload/download a file that contain the virus<br>\n";
static const char *clamav_tail_message =
    "\n<p>This message generated by C-ICAP srvClamAV/antivirus module\n";

void generate_error_page(av_req_data_t * data, request_t * req)
{
     int new_size = 0;
     ci_membuf_t *error_page;
     char buf[128];

     snprintf(buf, 128, "X-Infection-Found: Type=0; Resolution=2; Threat=%s;",
              data->virus_name);
     buf[127] = '\0';
     ci_request_add_xheader(req, buf);
     new_size =
         strlen(clamav_error_message) + strlen(clamav_tail_message) +
         strlen(data->virus_name) + 10;
     if (ci_respmod_headers(req))
          ci_respmod_reset_headers(req);
     else
          ci_request_create_respmod(req, 1, 1);
     ci_respmod_add_header(req, "HTTP/1.0 200 OK");
     ci_respmod_add_header(req, "Server: C-ICAP");
     ci_respmod_add_header(req, "Connection: close");
     ci_respmod_add_header(req, "Content-Type: text/html");
     ci_respmod_add_header(req, "Content-Language: en");


     error_page = ci_membuf_new_sized(new_size);
     ((av_req_data_t *) data)->error_page = error_page;

     ci_membuf_write(error_page, (char *) clamav_error_message,
                     strlen(clamav_error_message), 0);
     ci_membuf_write(error_page, (char *) data->virus_name,
                     strlen(data->virus_name), 0);
     ci_membuf_write(error_page, (char *) clamav_tail_message, strlen(clamav_tail_message), 1); /*And here is the eof.... */
}

/***************************************************************************************/
/* Parse arguments function - 
   Current arguments: allow204=on|off, force=on, sizelimit=off, mode=simple|vir|mixed          
*/
void srvclamav_parse_args(av_req_data_t * data, char *args)
{
     char *str;
     if ((str = strstr(args, "allow204="))) {
          if (strncmp(str + 9, "on", 2) == 0)
               data->args.enable204 = 1;
          else if (strncmp(str + 9, "off", 3) == 0)
               data->args.enable204 = 0;
     }
     if ((str = strstr(args, "force="))) {
          if (strncmp(str + 6, "on", 2) == 0)
               data->args.forcescan = 1;
     }
     if ((str = strstr(args, "sizelimit="))) {
          if (strncmp(str + 10, "off", 3) == 0)
               data->args.sizelimit = 0;
     }
     if ((str = strstr(args, "mode="))) {
          if (strncmp(str + 5, "simple", 6) == 0)
               data->args.mode = 1;
          else if (strncmp(str + 5, "vir", 3) == 0)
               data->args.mode = 2;
          else if (strncmp(str + 5, "mixed", 5) == 0)
               data->args.mode = 3;
     }
}

/****************************************************************************************/
/*Commands functions                                                                    */
void dbreload_command(char *name, int type, char **argv)
{
     ci_debug_printf(1, "Clamav virus database reload command received\n");
     if (!reload_virusdb())
          ci_debug_printf(1, "Clamav virus database reload command failed!\n");
     if (srv_clamav_xdata)
          set_istag(srv_clamav_xdata);
}

/****************************************************************************************/
/*Configuration Functions                                                               */

int cfg_ScanFileTypes(char *directive, char **argv, void *setdata)
{
     int i, id;
     int type = NO_SCAN;
     if (strcmp(directive, "ScanFileTypes") == 0)
          type = SCAN;
     else if (strcmp(directive, "VirScanFileTypes") == 0)
          type = VIR_SCAN;
     else
          return 0;

     for (i = 0; argv[i] != NULL; i++) {
          if ((id = ci_get_data_type_id(magic_db, argv[i])) >= 0)
               scantypes[id] = type;
          else if ((id = ci_get_data_group_id(magic_db, argv[i])) >= 0)
               scangroups[id] = type;
          else
               ci_debug_printf(1, "Unknown data type %s \n", argv[i]);

     }

     ci_debug_printf(1, "Iam going to scan data for %s scanning of type:",
                     (type == 1 ? "simple" : "vir_mode"));
     for (i = 0; i < ci_magic_types_num(magic_db); i++) {
          if (scantypes[i] == type)
               ci_debug_printf(1, ",%s", ci_data_type_name(magic_db, i));
     }
     for (i = 0; i < ci_magic_groups_num(magic_db); i++) {
          if (scangroups[i] == type)
               ci_debug_printf(1, ",%s", ci_data_group_name(magic_db, i));
     }
     ci_debug_printf(1, "\n");
     return 1;
}


int cfg_SendPercentBytes(char *directive, char **argv, void *setdata)
{
     int val = 0;
     char *end;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive %s \n", directive);
          return 0;
     }
     errno = 0;
     val = strtoll(argv[0], &end, 10);
     if (errno != 0 || val < 0 || val > 100) {
          ci_debug_printf(1, "Invalid argument in directive %s \n", directive);
          return 0;
     }

     SEND_PERCENT_BYTES = val;
     ci_debug_printf(1, "Setting parameter :%s=%d\n", directive, val);
     return val;
}



int cfg_ClamAvTmpDir(char *directive, char **argv, void *setdata)
{
     int val = 0;
     struct stat stat_buf;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }
     if (stat(argv[0], &stat_buf) != 0 || !S_ISDIR(stat_buf.st_mode)) {
          ci_debug_printf(1,
                          "The directory %s (%s=%s) does not exist or is not a directory !!!\n",
                          argv[0], directive, argv[0]);
          return 0;
     }

     /*TODO:Try to write to the directory to see if it is writable ........

      */

     cl_settempdir(argv[0], 0);
     ci_debug_printf(1, "Setting parameter :%s=%s\n", directive, argv[0]);
     return val;
}
