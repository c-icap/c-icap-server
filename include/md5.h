#ifndef __C_ICAP_CI_MD5_H
#define __C_ICAP_CI_MD5_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct ci_MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    unsigned char in[64];
};
typedef struct ci_MD5Context ci_MD5_CTX;
CI_DECLARE_FUNC(void) ci_MD5Init(struct ci_MD5Context *ctx);
CI_DECLARE_FUNC(void) ci_MD5Update(struct ci_MD5Context *ctx, const unsigned char *buf, size_t len);
CI_DECLARE_FUNC(void) ci_MD5Final(unsigned char digest[16], struct ci_MD5Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __C_ICAP_CI_MD5_H */
