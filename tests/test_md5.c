#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "md5.h"

static void MDPrint(unsigned char digest[16]);
static void MDString(char *string);

static void
MDString(char *string)
{
    ci_MD5_CTX context;
    unsigned char digest[16];
    unsigned int len = strlen(string);
    ci_MD5Init(&context);
    ci_MD5Update(&context, (unsigned char *)string, len);
    ci_MD5Final(digest, &context);
    printf("MD5 (\"%s\") = ", string);
    MDPrint(digest);
    printf("\n");
}

static void
MDPrint(unsigned char digest[16])
{
    unsigned int i;
    for (i = 0; i < 16; i++)
        printf("%02x", digest[i]);
}


int main(int argc, char *argv[])
{
    printf("MD5 test suite:\n");
    MDString("");
    MDString("a");
    MDString("abc");
    MDString("message digest");
    MDString("abcdefghijklmnopqrstuvwxyz");
    MDString("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    MDString("1234567890123456789012345678901234567890"
             "1234567890123456789012345678901234567890");
    return 0;
}
