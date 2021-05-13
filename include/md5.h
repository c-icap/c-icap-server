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

/**
   Convert the 16-bytes md5 digest to a hex/string representation.
   To fully dump md5 string needs a buffer of size at least
   32+1 bytes.
   It always produces a null terminated string.
   \ingroup UTILITY
   \return The written bytes. If the output string is truncated then the
           return value us the number of characters which would have been
           been written to the out string if enough space had been available.
*/
CI_DECLARE_FUNC(int) ci_MD5_to_str(unsigned char digest[16], char *out, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __C_ICAP_CI_MD5_H */
