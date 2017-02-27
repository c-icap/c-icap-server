#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include "simple_api.h"
#include "header.h"
#include "debug.h"





int main(int argc, char *argv[])
{
    int i;
    ci_headers_list_t *headers;
    const char *s;
    size_t valsize;
    char buf[128];

    headers = ci_headers_create();
    ci_headers_add(headers,"HTTP/1.1 200 OK");
    ci_headers_add(headers,"Date: Fri, 23 Jul 2004 16:31:39 GMT");
    ci_headers_add(headers,"Server: Apache/1.3.28 (Linux/SuSE) PHP/4.3.3 mod_perl/1.28");
    ci_headers_add(headers,"Content-Location: index.html.en");
    ci_headers_add(headers,"Vary: negotiate,accept-language,accept-charset");
    ci_headers_add(headers,"Last-Modified: Wed, 16 Jul 2003 20:40:02 GMT");
    ci_headers_add(headers,"ETag: \"760-2485-3f15b822;403fc6e0\"");
    ci_headers_add(headers,"Accept-Ranges: bytes");
    ci_headers_add(headers,"Content-Length: 9349");
    ci_headers_add(headers,"Keep-Alive: timeout=15, max=96");
    ci_headers_add(headers,"Connection: Keep-Alive");
    ci_headers_add(headers,"Content-Type: text/html");
    ci_headers_add(headers,"Content-Language: en");


    
    for (i=0; i<headers->used; i++) {
	printf(" %d. %s\n", i, headers->headers[i]);
    }

    
    ci_headers_remove(headers,"Content-Language");
    ci_headers_remove(headers,"Content-Type");
    ci_headers_remove(headers,"Accept-Ranges");
    ci_headers_add(headers,"X-Test-Header: a-test-value by me");
    
    printf("\n\nPrint headers 2\n");
    for (i=0; i<headers->used; i++) {
	printf(" %d. %s\n", i, headers->headers[i]);
    }

    printf("\nSearch functions tests:\n");
    printf(" First Line: '%s'\n", ci_headers_first_line(headers));
    printf(" Search for vary header: %s\n", ci_headers_search(headers, "Vary"));
    printf(" Search for 'Connection' header value: %s\n", ci_headers_value(headers, "Connection"));
    printf(" Search for the last 'X-Test-Header' header value: %s\n", ci_headers_value(headers, "X-Test-Header"));

    printf(" Search for 'Connection' header value and copy to buf: %s\n", ci_headers_copy_value(headers, "Connection", buf, sizeof(buf)));
    printf(" Search for the last 'X-Test-Header' header value and copy to buf: %s\n", ci_headers_copy_value(headers, "X-Test-Header", buf, sizeof(buf)));

    s = ci_headers_search2(headers, "Connection", &valsize);
    printf(" Search for 'Connection' header and get size: '%s' of size :%d\n", s, (int)valsize);
    s = ci_headers_value2(headers, "X-Test-Header", &valsize);
    printf(" Search for the last 'X-Test-Header' header value and get size: '%s' of size %d\n", s, (int)valsize);

    ci_headers_pack(headers);

    printf("\n\nThe Packed headers are:\n%.*s\n",headers->bufused, headers->headers[0]);
    s = ci_headers_first_line2(headers, &valsize);
    printf(" First Line: '%.*s'\n", (int)valsize, s);
    s = ci_headers_search2(headers, "Connection", &valsize);
    printf(" Search for 'Connection' header in packed: '%.*s'\n", (int)valsize, s);
    s = ci_headers_value2(headers, "X-Test-Header", &valsize);
    printf(" Search for the last 'X-Test-Header' header value in packed: '%.*s'\n", (int)valsize, s);

    return 0;    
}
