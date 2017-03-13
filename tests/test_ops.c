#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-icap.h"
#include "cfg_param.h"
#include "debug.h"
#include "types_ops.h"
#include "mem.h"
#include "net_io.h"

char *str_ip(ci_ip_t *ip)
{
    char ip_buf[512];
    char mask_buf[512];
    static char buf[1024];
    sprintf(buf, "%s/%s",
            ci_inet_ntoa(ip->family, &ip->address, ip_buf, sizeof(ip_buf)),
            ci_inet_ntoa(ip->family, &ip->netmask, mask_buf, sizeof(mask_buf)));
    return buf;
}

int check_ip_ops()
{
    int i , ret = 1;
    char ip_buf[128];
    ci_ip_t *ip1 = ci_ip_ops.dup("192.168.1.1/255.255.255.248", default_allocator);
    for (i = 1; i < 8 && ret; ++i) {
        snprintf(ip_buf, sizeof(ip_buf), "192.168.1.%d", i);
        ci_ip_t *ip2 = ci_ip_ops.dup(ip_buf, default_allocator);
        printf("IP network address: %s\n", str_ip(ip1));
        printf("IP check address: %s\n", str_ip(ip2));
        ret = ci_ip_ops.equal(ip1, ip2);
        printf("Check result: %d\n\n", ret);
        ci_ip_ops.free(ip2, default_allocator);
    }

    for (i = 8; i < 16 && ret; ++i) {
        snprintf(ip_buf, sizeof(ip_buf), "192.168.1.%d", i);
        ci_ip_t *ip2 = ci_ip_ops.dup(ip_buf, default_allocator);
        printf("IP network address: %s\n", str_ip(ip1));
        printf("IP check address: %s\n", str_ip(ip2));
        ret = ci_ip_ops.equal(ip1, ip2);
        printf("Check result: %d\n\n", ret);
        ret = !ret;
        ci_ip_ops.free(ip2, default_allocator);
    }

    ci_ip_ops.free(ip1, default_allocator);
    return ret;
}

void log_errors(void *unused, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}


static struct ci_options_entry options[] = {
    {
        "-d", "debug_level", &CI_DEBUG_LEVEL, ci_cfg_set_int,
        "The debug level"
    },
    {NULL,NULL,NULL,NULL,NULL}
};

int mem_init();
int main(int argc,char *argv[])
{
    int ret = 0;
    ci_cfg_lib_init();
    __log_error = (void (*)(void *, const char *,...)) log_errors;     /*set c-icap library log  function */
    if (!ci_args_apply(argc, argv, options)) {
        ci_args_usage(argv[0], options);
        exit(-1);
    }

    ci_cfg_lib_init();
    mem_init();

    if (!check_ip_ops()) {
        ci_debug_printf(1, "ip_ops check failed!\n");
        ret = -1;
    }

    return ret;
}
