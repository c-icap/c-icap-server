#include "common.h"
#include <stdio.h>
#include "c-icap.h"
#include "body.h"
#include "cfg_param.h"
#include "debug.h"
#include "md5.h"
#include "mem.h"

static void
MDPrint(const char *label, unsigned char digest[16])
{
    unsigned int i;
    printf("%s:", label);
    for (i = 0; i < 16; i++)
        printf("%02x", digest[i]);
    printf("\n");
}

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

char *FILENAME = NULL;
int USE_DEBUG_LEVEL = -1;
static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &USE_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {
        "-f", "file", &FILENAME, ci_cfg_set_str,
        "The path of the file to load"
    },
    {NULL,NULL,NULL,NULL,NULL}
};

int mem_init();
int init_body_system();
int main(int argc,char *argv[])
{
    ci_membuf_t *mb = NULL;
    ci_simple_file_t *sf = NULL;
    ci_cfg_lib_init();
    mem_init();
    init_body_system();

    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */

    if (!ci_args_apply(argc, argv, options) || !FILENAME) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    if (USE_DEBUG_LEVEL >= 0)
        CI_DEBUG_LEVEL = USE_DEBUG_LEVEL;

    FILE *f;
    char buf[4096];

    if ((f = fopen(FILENAME, "r")) == NULL) {
        ci_debug_printf(1, "Can not open file '%s'!\n", FILENAME);
        exit(-1);
    }

    if (!(sf = ci_simple_file_new(0))) {
        ci_debug_printf(1, "Error allocating simple body struct!\n");
        exit(-1);
    }

    ci_MD5_CTX md;
    unsigned char digest[16];
    ci_MD5Init(&md);
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), f))) {
        ci_MD5Update(&md, (unsigned char *)buf, bytes);
        ci_simple_file_write(sf, buf, bytes, 0);
    }
    ci_simple_file_write(sf, buf, 0, 1);

    ci_MD5Final(digest, &md);
    MDPrint("File md5", digest);


    mb = ci_simple_file_to_membuf(sf, CI_MEMBUF_CONST);
    ci_MD5Init(&md);
    ci_MD5Update(&md, (unsigned char *)mb->buf, mb->endpos);
    ci_MD5Final(digest, &md);
    MDPrint("From membuf_t, whole string md5", digest);

    ci_MD5Init(&md);
    int len;
    while ((len = ci_membuf_read(mb, buf, sizeof(buf))) > 0) {
        ci_MD5Update(&md, (unsigned char *)buf, len);
    }
    ci_MD5Final(digest, &md);
    MDPrint("From membuf_t read blocks md5", digest);

    if (mb)
        ci_membuf_free(mb);

    mb = ci_membuf_new_sized(65*1024);
    ci_membuf_flag(mb, CI_MEMBUF_NULL_TERMINATED);
    while ((len = ci_simple_file_read(sf, buf, sizeof(buf))) > 0)
        ci_membuf_write(mb, buf, len, 0);
    ci_membuf_write(mb, buf, 0, 1);
    ci_MD5Init(&md);
    ci_MD5Update(&md, (unsigned char *)mb->buf, mb->endpos);
    ci_MD5Final(digest, &md);
    MDPrint("From membuf_t after write, whole string md5", digest);

    ci_simple_file_destroy(sf);
    return 0;
}
