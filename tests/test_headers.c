#include "common.h"
#include "header.h"
#include "debug.h"

#include <stdio.h>



void checkSimpleLFHeaders();
void checkReallocations();
void checkAdds();
void checkRemoves();

const char *array_search(const char *array[], const char *prefix)
{
    int i;
    for (i = 0; array[i] != NULL; i++) {
        size_t len = strlen(prefix);
        if (strncmp(array[i], prefix, len) == 0)
            return array[i];
    }
    return ""; //Always return something
}

int main(int argc, char *argv[])
{
    int i;
    ci_headers_list_t *headers;
    const char *s, *s1;
    size_t valsize;
    char buf[4096];

#define VARY_VAL "negotiate,accept-language,accept-charset"
#define CONNECTION_VAL "Keep-Alive"

    const char *headersStr[] = {
        "HTTP/1.1 200 OK",
        "Date: Fri, 23 Jul 2004 16:31:39 GMT",
        "Server: Apache/1.3.28 (Linux/SuSE) PHP/4.3.3 mod_perl/1.28",
        "Content-Location: index.html.en",
        "Vary: " VARY_VAL,
        "Last-Modified: Wed, 16 Jul 2003 20:40:02 GMT",
        "ETag: \"760-2485-3f15b822;403fc6e0\"",
        "Accept-Ranges: bytes",
        "Content-Length: 19349",
        "Keep-Alive: timeout=15, max=96",
        "Connection: " CONNECTION_VAL,
        "Content-Type: text/html",
        "Content-Language: en",
        NULL
    };
    headers = ci_headers_create();
    for (i = 0; headersStr[i] != NULL; ++i) {
        ci_headers_add(headers,headersStr[i]);
    }

    for (i=0; i<headers->used; i++) {
        assert(strcmp(headers->headers[i], headersStr[i]) == 0);
    }


    ci_headers_remove(headers,"Content-Language");
    ci_headers_remove(headers,"Content-Type");
    ci_headers_remove(headers,"Accept-Ranges");
#define XTV "a-test-value by me"
    const char *xhead_add = "X-Test-Header: " XTV;
    ci_headers_add(headers, xhead_add);
    int j = 0;
    for (i = 0; headersStr[i] != NULL; i++) {
        if (strncmp(headersStr[i], "Content-Language", 16) == 0 ||
            strncmp(headersStr[i], "Content-Type", 12) == 0 ||
            strncmp(headersStr[i], "Accept-Ranges", 13) == 0
            )
            continue;
        assert(strcmp(headers->headers[j++], headersStr[i]) == 0);
    }
    assert(strcmp(headers->headers[j], xhead_add) == 0); // Last added header


    assert(strcmp(ci_headers_first_line(headers), headersStr[0]) == 0);
    s = ci_headers_search(headers, "Vary");
    assert(s);
    assert(strcmp(s, array_search(headersStr, "Vary")) == 0);

    s = ci_headers_value(headers, "Connection");
    assert(s);
    assert(strcmp(s, CONNECTION_VAL) == 0);

    s = ci_headers_value(headers, "X-Test-Header");
    assert(s);
    assert(strcmp(s, XTV) == 0);

    ci_headers_copy_value(headers, "Connection", buf, sizeof(buf));
    assert(strcmp(buf, CONNECTION_VAL) == 0);
    ci_headers_copy_value(headers, "X-Test-Header", buf, sizeof(buf));
    assert(strcmp(buf, XTV) == 0);

    s = ci_headers_search2(headers, "Connection", &valsize);
    s1 = array_search(headersStr, "Connection");
    assert(s);
    assert(s1);
    assert(valsize == strlen(s1));
    assert(strncmp(s, s1, valsize) == 0);

    s = ci_headers_value2(headers, "X-Test-Header", &valsize);
    assert(s);
    assert(valsize == strlen(XTV));
    assert(strncmp(s, XTV, valsize) == 0);

    ci_headers_list_t *clonedUnpacked = ci_headers_clone(headers);

    ci_headers_pack(headers);
    ci_headers_list_t *clonedPacked = ci_headers_clone(headers);

    // headers are packed now.
    s = ci_headers_first_line2(headers, &valsize);
    assert(s);
    assert(valsize == strlen(headersStr[0]));
    assert(strncmp(s, headersStr[0], valsize) == 0);

    s = ci_headers_search2(headers, "Connection", &valsize);
    s1 = array_search(headersStr, "Connection");
    assert(s);
    assert(s1);
    assert(valsize == strlen(s1));
    assert(strncmp(s, s1, valsize) == 0);

    s = ci_headers_value2(headers, "X-Test-Header", &valsize);
    assert(s);
    assert(valsize == strlen(XTV));
    assert(strncmp(s, XTV, valsize) == 0);

    for (i = 0, j = 0; headersStr[i] != NULL; i++) {
        if (strncmp(headersStr[i], "Content-Language", 16) == 0 ||
            strncmp(headersStr[i], "Content-Type", 12) == 0 ||
            strncmp(headersStr[i], "Accept-Ranges", 13) == 0
            )
            continue;
        assert(strcmp(clonedUnpacked->headers[j++], headersStr[i]) == 0);
    }
    assert(strcmp(clonedUnpacked->headers[j], xhead_add) == 0); // Last added header

    size_t bytes = ci_headers_pack_to_buffer(clonedUnpacked, buf, sizeof(buf));
    for (i = 0; i < 100000; ++i) {
        ci_headers_unpack(headers);
        ci_headers_pack(headers);
    }
    assert(headers->bufused == clonedPacked->bufused);
    assert(memcmp(clonedPacked->buf, headers->buf, headers->bufused) == 0);
    assert(clonedPacked->bufused == bytes);
    assert(memcmp(clonedPacked->buf, buf, clonedPacked->bufused) == 0);

    checkSimpleLFHeaders();
    checkReallocations();
    checkAdds();
    checkRemoves();
    return 0;
}

void checkSimpleLFHeaders()
{
    int i = 0;
    const char *heads =
        "HTTP/1.1 200 OK\n"
        "Date: Fri, 23 Jul 2004 16:31:39 GMT\n"
        "Server: Apache/1.3.28 (Linux/SuSE) PHP/4.3.3 mod_perl/1.28\n"
        "Content-Location: index.html.en\n"
        "Vary: negotiate,accept-language,accept-charset\n"
        "Last-Modified: Wed, 16 Jul 2003 20:40:02 GMT\n"
        "ETag: \"760-2485-3f15b822;403fc6e0\"\n"
        "Accept-Ranges: bytes\n"
        "Content-Length: 9349\n"
        "Keep-Alive: timeout=15, max=96\n"
        "Connection: Keep-Alive\n"
        "Content-Type: text/html\n"
        "Content-Language: en\n\n"
        ;
    size_t len = strlen(heads);
    ci_headers_list_t *headers = ci_headers_create();
    memcpy(headers->buf, heads, len);
    headers->bufused = len;
    ci_headers_unpack(headers);
    char *tmp_heads = strdup(heads);
    const char *xh = strtok(tmp_heads, "\n");
    for (i = 0; i < headers->used; i++) {
        assert(xh);
        assert(strcmp(headers->headers[i], xh) == 0);
        xh = strtok(NULL, "\n");
    }
    free(tmp_heads);

    ci_headers_remove(headers,"Content-Language");
    ci_headers_remove(headers,"Content-Type");
    ci_headers_remove(headers,"Accept-Ranges");
    const char *xtest_hdr = "X-Test-Header: a-test-value by me";
    ci_headers_add(headers, xtest_hdr);
    tmp_heads = strdup(heads);
    xh = strtok(tmp_heads, "\n");
    for (i = 0; i < headers->used - 1; i++) {
        assert(xh);
        assert(strcmp(headers->headers[i], xh) == 0);
        do {
            xh = strtok(NULL, "\n");
        } while (xh && (strncmp(xh, "Content-Language", 16) == 0 ||
                        strncmp(xh, "Content-Type", 12) == 0 ||
                        strncmp(xh, "Accept-Ranges", 13) == 0));
    }
    assert(strcmp(headers->headers[i], xtest_hdr) == 0); // last added header
    free(tmp_heads);

    ci_headers_list_t *clonedUnpacked = ci_headers_clone(headers);
    for (int i = 0; i < 10; ++i) {
        ci_headers_unpack(headers);
        ci_headers_pack(headers);
    }
    ci_headers_unpack(headers);
    assert(headers->bufused == clonedUnpacked->bufused);
    assert(memcmp(clonedUnpacked->buf, headers->buf, headers->bufused) == 0);
}

void checkReallocations()
{
    int i;
    const char *heads =
        "HTTP/1.1 200 OK\r\n"
        "Date: Fri, 23 Jul 2004 16:31:39 GMT\r\n"
        "Server: Apache/1.3.28 (Linux/SuSE) PHP/4.3.3 mod_perl/1.28\r\n"
        "Content-Location: index.html.en\r\n"
        "Vary: negotiate,accept-language,accept-charset\r\n"
        "Last-Modified: Wed, 16 Jul 2003 20:40:02 GMT\r\n"
        "ETag: \"760-2485-3f15b822;403fc6e0\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 9349\r\n"
        "Keep-Alive: timeout=15, max=96\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Type: text/html\r\n"
        "Content-Language: en\r\n"
        "\r\n"
        ;
    printf("\nStart check for headers reallocations\n");
    size_t len = strlen(heads);
    ci_headers_list_t *headers = ci_headers_create();
    memcpy(headers->buf, heads, len);
    headers->bufused = len;
    ci_headers_unpack(headers);
    int headers_before_add = headers->used;
    const char *header_pattern = "X-Text-Add-Header-%d: X test values XXXXXXXXXXXXXXXXXXXXXXXXXXX-%d";
    char buf[256];
    printf("Headers build add 124 headers\n");
    for (i = 0; i < 124; ++i) {
        snprintf(buf, sizeof(buf), header_pattern, i, i);
        ci_headers_add(headers, buf);
    }

    printf("Check added headers after huge updates:\n");
    for (i = headers->used - 1; i > headers_before_add; --i) {
        snprintf(buf, sizeof(buf), header_pattern, i - headers_before_add, i - headers_before_add);
        assert(strcmp(buf, headers->headers[i]) == 0);
    }
    printf("Check done: New headers of mem size: %d, headers number: %d\n", headers->bufused, headers->used);
}

void checkAdds()
{
    int i;
    const char *heads =
        "HTTP/1.1 200 OK\r\n"
        "Date: Fri, 23 Jul 2004 16:31:39 GMT\r\n"
        "Server: Apache/1.3.28 (Linux/SuSE) PHP/4.3.3 mod_perl/1.28\r\n"
        "Content-Location: index.html.en\r\n"
        "Vary: negotiate,accept-language,accept-charset\r\n"
        "Last-Modified: Wed, 16 Jul 2003 20:40:02 GMT\r\n"
        "ETag: \"760-2485-3f15b822;403fc6e0\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 9349\r\n"
        "Keep-Alive: timeout=15, max=96\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Type: text/html\r\n"
        "Content-Language: en\r\n"
        "\r\n"
        ;

    printf("\nStart check for multiple headers additions\n");
    size_t len = strlen(heads);
    ci_headers_list_t *hstart = ci_headers_create();
    memcpy(hstart->buf, heads, len);
    hstart->bufused = len;
    int ret = ci_headers_unpack(hstart);
    assert(ret == EC_100);

    // build xheaders
    const int xheads_num = 124;
    const char *xhead_pattern = "X-Test-header-%d: x-value-%d";
    ci_headers_list_t *add_headers = ci_headers_create();
    for(i = 0; i < xheads_num; ++i) {
        char buf[256];
        snprintf(buf, sizeof(buf), xhead_pattern, i, i);
        ci_headers_add(add_headers, buf);
    }

    // Build new headers from hstart and add_headers
    ci_headers_list_t *headers = ci_headers_create();
    ci_headers_addheaders(headers, hstart);
    int headers_number_step1 = headers->used;
    ci_headers_addheaders(headers, add_headers);
    // Check if all are ok
    assert(headers->used == hstart->used + add_headers->used);
    for (i = 0; i < headers_number_step1; ++i) {
        assert(strcmp(headers->headers[i], hstart->headers[i]) == 0);
    }
    for (i = headers_number_step1; i < headers->used; ++i) {
        assert(strcmp(headers->headers[i], add_headers->headers[i - headers_number_step1]) == 0);
    }
    printf("Check done!\n");
}

void checkRemoves()
{
    ci_headers_list_t *h = ci_headers_create();
    ci_headers_add(h, "Content-Length: 6978");
    ci_headers_remove(h, "Content-Length");
    ci_headers_add(h, "Content-Length: 6978");
    ci_headers_destroy(h);
}
