#ifndef AV_MD5_H
#define AV_MD5_H

#include "c-icap.h"

struct ci_MD5Context {
        uint32_t buf[4];
        uint32_t bits[2];
        unsigned char in[64];
};
typedef struct ci_MD5Context ci_MD5_CTX;
void ci_MD5Init(struct ci_MD5Context *ctx);
void ci_MD5Update(struct ci_MD5Context *ctx, const unsigned char *buf, size_t len);
void ci_MD5Final(unsigned char digest[16], struct ci_MD5Context *ctx);

#endif /* !AV_MD5_H */
