#ifndef __SRV_CLAMAV_H
#define __SRV_CLAMAV_H

#define VIRALATOR_MODE


typedef struct av_req_data{
     ci_simple_file_t *body;
     request_t *req;
     int must_scanned ;
     int allow204;
     const char *virus_name;
     ci_membuf_t *error_page;
#ifdef VIRALATOR_MODE
     time_t last_update;
     char *requested_filename;
     int page_sent;
     ci_off_t expected_size;
#endif
     struct{
	  int enable204;
	  int forcescan;
	  int sizelimit;
	  int mode;
     } args;
}av_req_data_t;

enum {NO_SCAN=0,SCAN,VIR_SCAN};

#ifdef VIRALATOR_MODE
void init_vir_mode_data(request_t *req,av_req_data_t *data);
int send_vir_mode_page(av_req_data_t *data,char *buf,int len,request_t *req);
void endof_data_vir_mode(av_req_data_t *data,request_t *req);
#endif

#endif
