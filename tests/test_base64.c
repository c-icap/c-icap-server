#include "common.h"
#include "simple_api.h"


int main(int argc, char *argv[])
{
    char encoded[1024];
    char decoded[1024];
    int l;
    const char *str;
    if (argc > 1) {
        str = argv[1];
    } else
        str = "Good morning";
    ci_base64_encode((unsigned char *)str, (size_t)strlen(str), encoded, 1024);
    l = ci_base64_decode(encoded, decoded, 1024);
    decoded[l] = '\0';
    printf("Input string: \'%s\'\n", str);
    printf("Base64 encoded string: \'%s\'\n", encoded);
    printf("Decoded string: \'%s\'\n", decoded);
    return 0;
}


