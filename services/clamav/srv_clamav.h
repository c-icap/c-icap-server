#ifndef __SRV_CLAMAV_H
#define __SRV_CLAMAV_H

#define VIRALATOR_MODE


typedef struct av_req_data{
     ci_simple_file_t *body;
     request_t *req;
     int must_scanned ;
     const char *virus_name;
     ci_membuf_t *error_page;
#ifdef VIRALATOR_MODE
     time_t last_update;
     char *requested_filename;
     int page_sent;
     int expected_size;
#endif
}av_req_data_t;

enum {NO_SCAN=0,SCAN,VIR_SCAN};


#endif
