/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */


#include "common.h"
#include "c-icap.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
#ifdef HAVE_BZLIB
#include <bzlib.h>
#endif
#ifdef HAVE_BROTLI
#include "brotli/decode.h"
#include "brotli/encode.h"
#include "brotli/types.h"
#include "brotli/port.h"
#endif


unsigned char base64_table[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
    52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255,   0, 255, 255,
    255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
    41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};


int ci_base64_decode(const char *encoded, char *decoded, int len)
{
    int i;
    unsigned char *str,*result;

    if (!encoded || !decoded || !len)
        return 0;

    str = (unsigned char *)encoded;
    result = (unsigned char *)decoded;

    for (i = len; i > 3; i -= 3) {

        /*if one of the last 4 bytes going to be proccessed is not valid just
          stops processing. This "if" cover the '\0' string termination character
          of str (because base64_table[0] = 255)
         */
        if (base64_table[*str]>63 || base64_table[*(str+1)] > 63 ||
                base64_table[*(str+2)] > 63 ||base64_table[*(str+3)] > 63)
            break;

        /*6 bits from the first + 2 last bits from second*/
        *(result++) = (base64_table[*str] << 2) | (base64_table[*(str+1)] >>4);
        /*last 4 bits from second + first 4 bits from third*/
        *(result++) = (base64_table[*(str+1)] << 4) | (base64_table[*(str+2)] >>2);
        /*last 2 bits from third + 6 bits from forth */
        *(result++) = (base64_table[*(str+2)] << 6) | (base64_table[*(str+3)]);
        str += 4;
    }
    *result = '\0';
    return len-i;
}


char *ci_base64_decode_dup(const char *encoded)
{
    int len;
    char *result;
    len = strlen(encoded);
    len = ((len+3)/4)*3+1;
    if (!(result = malloc(len*sizeof(char))))
        return NULL;

    ci_base64_decode(encoded,result,len);
    return result;
}

/* byte1____ byte2____ byte3____ */
/* 123456 78 1234 5678 12 345678 */
/* b64_1_ b64_2__ b64_3__ b64_4_ */

#define dobase64(s, b0, b1, b2)                                 \
    s[k++] = base64_set[(b0 >> 2) & 0x3F];                      \
    s[k++] = base64_set[((b0 << 4) | (b1 >> 4)) & 0x3F];        \
    s[k++] = base64_set[((b1 << 2) | (b2 >> 6)) & 0x3F];        \
    s[k++] = base64_set[b2 & 0x3F];

int ci_base64_encode(const unsigned char *data, size_t len, char *out, size_t outlen)
{
    int i, k;
    const char *base64_set = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"    \
                             "abcdefghijklmnopqrstuvwxyz"\
                             "0123456789"\
                             "+/";
    for (i = 0, k = 0; i < (len - 3) && k < (outlen - 4); i += 3) {
        dobase64(out, data[i], data[i + 1], data[i + 2]);
    }
    /*if the outlen is enough big*/
    if (k < (outlen -4) && i < len) {
        dobase64(out, (i < len ? data[i] : 0), (i + 1 < len ? data[i + 1] : 0), (i + 2 < len ? data[i + 2] : 0));
    }

    out[k] = '\0';
    return k;
}

/*url decoders  */
CI_DECLARE_FUNC(int) url_decoder(const char *input,char *output, int output_len)
{
    int i, k;
    char str[3];

    i = 0;
    k = 0;
    while ((input[i] != '\0') && (k < output_len-1)) {
        if (input[i] == '%') {
            str[0] = input[i+1];
            str[1] = input[i+2];
            str[2] = '\0';
            output[k] = strtol(str, NULL, 16);
            i = i + 3;
        } else if (input[i] == '+') {
            output[k] = ' ';
            i++;
        } else {
            output[k] = input[i];
            i++;
        }
        k++;
    }
    output[k] = '\0';

    if (k == output_len-1)
        return -1;

    return 1;
}

CI_DECLARE_FUNC(int) url_decoder2(char *input)
{
    int i, k;
    char str[3];
    i = 0;
    k = 0;
    while (input[i] != '\0') {
        if (input[i] == '%') {
            str[0] = input[i+1];
            str[1] = input[i+2];
            str[2] = '\0';
            input[k] = strtol(str, NULL, 16);
            i = i + 3;
        } else if (input[i] == '+') {
            input[k] = ' ';
            i++;
        } else {
            input[k] = input[i];
            i++;
        }
        k++;
    }
    input[k] = '\0';
    return 1;
}


#define CHUNK 8192

static const char *uncompress_errors[] = {
    "uncompress: No Error",
    "uncompress: Uncompression Failure",
    "uncompress: Write Failed",
    "uncompress: Input Corrupted",
    "uncompress: Compression Bomb"
};

const char *ci_decompress_error(int err)
{
    ci_debug_printf (7, "Inflate error %d\n", err);
    if (err < CI_UNCOMP_ERR_NONE && err >= CI_UNCOMP_ERR_BOMB)
        return uncompress_errors[-err];
    return "No Error";
}

const char *ci_inflate_error(int err)
{
    return ci_decompress_error(err);
}

int ci_encoding_method(const char *content_encoding)
{
    if (!content_encoding)
        return CI_ENCODE_NONE;

    if (strcasestr(content_encoding, "gzip") != NULL) {
        return CI_ENCODE_GZIP;
    }

    if (strcasestr(content_encoding, "deflate") != NULL) {
        return CI_ENCODE_DEFLATE;
    }

    if (strcasestr(content_encoding, "br") != NULL) {
        return CI_ENCODE_BROTLI;
    }

    if (strcasestr(content_encoding, "bzip2") != NULL) {
        return CI_ENCODE_BZIP2;
    }

    return CI_ENCODE_UNKNOWN;
}

static int write_membuf_func(void *obj, const char *buf, size_t len)
{
    return ci_membuf_write((ci_membuf_t *)obj, buf, len, 0);
}

static int write_simple_file_func(void *obj, const char *buf, size_t len)
{
    return ci_simple_file_write((ci_simple_file_t *)obj, buf, len, 0);
}

struct unzipBuf {
    char *buf;
    size_t buf_size;
    size_t out_len;
};

static char *get_buf_outbuf(void *obj, unsigned int *len)
{
    struct unzipBuf *ab = (struct unzipBuf *)obj;
    *len = ab->buf_size;
    return ab->buf;
}

static int write_once_to_outbuf(void *obj, const char *buf, size_t len)
{
    struct unzipBuf *ab = (struct unzipBuf *)obj;
    ab->out_len = ab->buf_size < len ? ab->buf_size : len;
    memcpy(ab->buf, buf, ab->out_len);
    /*Return 0 to abort immediately uncompressing.
      We are interesting only for the first bytes*/
    return 0;
}


/*return CI_INFLATE_ERRORS
 */
int ci_decompress_to_membuf(int encoding_format, const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    switch (encoding_format) {
	case CI_ENCODE_NONE:
            return CI_UNCOMP_OK;
            break;
#ifdef HAVE_ZLIB
        case CI_ENCODE_GZIP:
        case CI_ENCODE_DEFLATE:
            return ci_inflate_to_membuf(inbuf, inlen, outbuf, max_size);
            break;
#endif
#ifdef HAVE_BZLIB
        case CI_ENCODE_BZIP2:
            return ci_bzunzip_to_membuf(inbuf, inlen, outbuf, max_size);
            break;
#endif
#ifdef HAVE_BROTLI
        case CI_ENCODE_BROTLI:
            return ci_brinflate_to_membuf(inbuf, inlen, outbuf, max_size);
            break;
#endif
        case CI_ENCODE_UNKNOWN:
        default:
            return CI_UNCOMP_ERR_ERROR;
            break;
    }
}

/*return CI_INFLATE_ERRORS
 */
int ci_decompress_to_simple_file(int encoding_format, const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    switch (encoding_format) {
	case CI_ENCODE_NONE:
            return CI_UNCOMP_OK;
            break;
#ifdef HAVE_ZLIB
        case CI_ENCODE_GZIP:
        case CI_ENCODE_DEFLATE:
            return ci_inflate_to_simple_file(inbuf, inlen, outbuf, max_size);
            break;
#endif
#ifdef HAVE_BZLIB
        case CI_ENCODE_BZIP2:
            return ci_bzunzip_to_simple_file(inbuf, inlen, outbuf, max_size);
            break;
#endif
#ifdef HAVE_BROTLI
        case CI_ENCODE_BROTLI:
            return ci_brinflate_to_simple_file(inbuf, inlen, outbuf, max_size);
            break;
#endif
        case CI_ENCODE_UNKNOWN:
        default:
            return CI_UNCOMP_ERR_ERROR;
            break;
    }
}

#ifdef HAVE_BROTLI
#define DEFAULT_LGWIN	 22
#define DEFAULT_QUALITY	 11
#define kFileBufferSize 16384

BROTLI_BOOL Br_Decompress(BrotliDecoderState* s, const char *buf, int inlen, void *outbuf, char *(*get_outbuf)(void *obj, unsigned int *len), int (*writefunc)(void *obj, const char *buf, size_t len), ci_off_t max_size)
{
    size_t available_in = 0;
    const uint8_t* next_in = NULL;
    size_t available_out = kFileBufferSize;
    uint8_t* next_out;
    unsigned have, written;
    long long outsize;
    size_t total_out = 0;
    BrotliDecoderResult result = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
    uint8_t out[kFileBufferSize];

    ci_debug_printf(4, "data-compression: brotli decompress called size: %d\n",
            inlen);

    next_in = (uint8_t *)buf;
    available_in = inlen;
    outsize = 0;
    for (;;) {
        available_out = kFileBufferSize;
        next_out = out;
        result = BrotliDecoderDecompressStream(s,
                               &available_in, &next_in,
                               &available_out,
                               &next_out, &total_out);
        have = kFileBufferSize - available_out;
        if (!have || (written =
                  writefunc(outbuf, (char *)out, have)) != have) {
            return BROTLI_FALSE;
        }
        outsize += written;
        if (max_size > 0 && outsize > max_size) {
            if ( (outsize/inlen) > 100) {
                ci_debug_printf(1, "data-compression: brotli Compression ratio UncompSize/CompSize = %"
                        PRINTF_OFF_T "/%" PRINTF_OFF_T " = %" PRINTF_OFF_T
                        "! Is it a zip bomb? aborting!\n",
                        (CAST_OFF_T)outsize,
                        (CAST_OFF_T)inlen,
                        (CAST_OFF_T)(outsize/inlen));
                return BROTLI_FALSE;
            }
            else {
                ci_debug_printf(4, "data-compression: brotli Object is bigger than max allowed file\n");
                return BROTLI_FALSE;
            }
        }
        switch (result) {
        case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
            ci_debug_printf(4, "data-compression: brotli needs more input\n");
            break;
        case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
            /* Nothing to do - output is already written. */
            break;
        case BROTLI_DECODER_RESULT_SUCCESS:
            if (available_in != 0) {
                ci_debug_printf(4, "data-compression: brotli available_in != 0\n");
                return BROTLI_FALSE;
            }
            ci_debug_printf(4, "data-compression: brotli total uncompressed size: %lld (%lld)\n",
                    outsize, (long long)total_out);
            return BROTLI_TRUE;
        default:
            ci_debug_printf(4, "data-compression: brotli corrupt input\n");
            return BROTLI_FALSE;
        }

    }
}

int ci_mem_brinflate(const char *inbuf, int inlen, void *outbuf, char *(*get_outbuf)(void *obj, unsigned int *len), int (*writefunc)(void *obj, const char *buf, size_t len), ci_off_t max_size)
{
	BROTLI_BOOL ccode = BROTLI_TRUE;
	BrotliDecoderState *s;

	s = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	if (!s) {
		ci_debug_printf(4, "data-compression:  brotli out of memory\n");
		return -1;
	}
	ccode = Br_Decompress(s, inbuf, inlen, outbuf, get_outbuf, writefunc, max_size);
	BrotliDecoderDestroyInstance(s);
	if (!ccode)
		return -1;
	return 1;
}

int ci_brinflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_brinflate(inbuf, inlen, outbuf, NULL, write_membuf_func, max_size);
    ci_membuf_write(outbuf, "", 0, 1);
    return ret;
}

int ci_brinflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_brinflate(inbuf, inlen, outbuf, NULL, write_simple_file_func, max_size);
    ci_simple_file_write(outbuf, "", 0, 1);
    return ret;
}

static int brotli_inflate_step(const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len)
{
    struct unzipBuf ub;
    ub.buf = unzipped_buf;
    ub.buf_size = *unzipped_buf_len;
    int ret = ci_mem_brinflate(buf, len,  &ub, get_buf_outbuf, write_once_to_outbuf, len);
    ci_debug_printf(5, "brotli_inflate_step: retcode %d, unzipped data: %d\n", ret, (int)ub.out_len);
    if (ub.out_len > 0) {
        *unzipped_buf_len = ub.out_len;
        return CI_OK;
    }
    return CI_ERROR;
}
#else
int ci_brinflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "brotlidecode is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}

int ci_brinflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "brotlidecode is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}
#endif

#ifdef HAVE_ZLIB
#define ZIP_HEAD_CRC     0x02   /* bit 1 set: header CRC present */
#define ZIP_EXTRA_FIELD  0x04   /* bit 2 set: extra field present */
#define ZIP_ORIG_NAME    0x08   /* bit 3 set: original file name present */
#define ZIP_COMMENT      0x10   /* bit 4 set: file comment present */

static void *alloc_a_buffer(void *op, unsigned int items, unsigned int size)
{
    return ci_buffer_alloc(items*size);
}

static void free_a_buffer(void *op, void *ptr)
{
    ci_buffer_free(ptr);
}

/*return CI_INFLATE_ERRORS
 */
int ci_mem_inflate(const char *inbuf, size_t inlen, void *out_obj, char *(*get_outbuf)(void *obj, unsigned int *len), int (*writefunc)(void *obj, const char *buf, size_t len), ci_off_t max_size)
{
    int ret, retriable;
    unsigned have, written, can_write, out_size;
    ci_off_t unzipped_size;
    z_stream strm;
    unsigned char *out, OUT[CHUNK];

    /* allocate inflate state */
    strm.zalloc = alloc_a_buffer;
    strm.zfree = free_a_buffer;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, 32 + 15);
    if (ret != Z_OK)
        return CI_UNCOMP_ERR_ERROR;

    retriable = 1;
    unzipped_size = 0;
    strm.next_in = (unsigned char*)inbuf;
    strm.avail_in = inlen;

    /* run inflate() on input until output buffer not full */
    do {
do_mem_inflate_retry:
        if (get_outbuf) {
            out = (unsigned char *)get_outbuf(out_obj, &out_size);
            strm.next_out = out;
            strm.avail_out = out_size;
            if (!out || !strm.avail_out) {
                inflateEnd(&strm);
                return CI_UNCOMP_ERR_OUTPUT;
            }
        } else {
            strm.avail_out = out_size = CHUNK;
            strm.next_out = out = OUT;
        }
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            //probably means memory overrun/overwrite etc
            ci_debug_printf(1, "Zlib/Z_STREAM_ERROR, corrupted input data to inflate?\n");
        }
        switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
            if (retriable) {
                ret = inflateInit2(&strm, -15);
                retriable = 0;
                if (ret == Z_OK) {
                    strm.avail_in = inlen;
                    strm.next_in = (unsigned char *)inbuf;
                    goto do_mem_inflate_retry;
                }
                /*else let fail ...*/
            }
        case Z_STREAM_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(&strm);
            return CI_UNCOMP_ERR_CORRUPT;
        }
        retriable = 0; // No more retries allowed
        have = out_size - strm.avail_out;
        can_write = (max_size > 0 && (max_size - unzipped_size) < have) ? (max_size - unzipped_size) : have;
        if ((written = writefunc(out_obj, (char *)out, can_write)) != can_write) {
            inflateEnd(&strm);
            return CI_UNCOMP_ERR_OUTPUT;
        }
        unzipped_size += written;
        if (written < have) {
            inflateEnd(&strm);
            if ( (unzipped_size/inlen) > 100) {
                ci_debug_printf(1, "Compression ratio UncompSize/CompSize = %" PRINTF_OFF_T "/%" PRINTF_OFF_T " = %" PRINTF_OFF_T "! Is it a zip bomb? aborting!\n", (CAST_OFF_T)unzipped_size, (CAST_OFF_T)inlen, (CAST_OFF_T)(unzipped_size/inlen));
                return CI_UNCOMP_ERR_BOMB;  /*Probably compression bomb object*/
            } else {
                ci_debug_printf(4, "Object is bigger than max allowed file\n");
                return CI_UNCOMP_ERR_NONE;
            }
        }
    } while (strm.avail_out == 0);

    /* clean up and return */
    inflateEnd(&strm);
    /*
      ret == Z_STREAM_END means that the decompression was succesfull
      else the output data are corrupted or not produced at all. 
      Example case is when the input data are not enough to produce 
      a single byte of decompressed data, so the inflate() return Z_OK
      (eg during preview request with few preview data)
     */
    return ret == Z_STREAM_END ? CI_UNCOMP_OK : CI_UNCOMP_ERR_CORRUPT;
}

int ci_inflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_inflate(inbuf, inlen, outbuf, NULL, write_membuf_func, max_size);
    ci_membuf_write(outbuf, "", 0, 1);
    return ret;
}

int ci_inflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_inflate(inbuf, inlen, outbuf, NULL, write_simple_file_func, max_size);
    ci_simple_file_write(outbuf, "", 0, 1);
    return ret;
}

static int zlib_inflate_step(const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len)
{
    struct unzipBuf ub;
    ub.buf = unzipped_buf;
    ub.buf_size = *unzipped_buf_len;
    int ret = ci_mem_inflate(buf, len,  &ub, get_buf_outbuf, write_once_to_outbuf, len);
    ci_debug_printf(5, "zlib_inflate_step: retcode %d, unzipped data: %d\n", ret, (int)ub.out_len);
    if (ub.out_len > 0) {
        *unzipped_buf_len = ub.out_len;
        return CI_OK;
    }
    return CI_ERROR;
}

#else
int ci_inflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "zlib/inflate is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}

int ci_inflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "zlib/inflate is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}
#endif

#ifdef HAVE_BZLIB
static void *bzalloc_a_buffer(void *op, int items, int size)
{
    return ci_buffer_alloc(items*size);
}

static void bzfree_a_buffer(void *op, void *ptr)
{
    ci_buffer_free(ptr);
}

/*
  TODO: fix to allow write directly to out_obj internal buffers instead of using out[CHUNK] buffer
 */
int ci_mem_bzunzip(const char *buf, int inlen,  void *out_obj, char *(*get_outbuf)(void *obj, unsigned int *len), int (*writefunc)(void *obj, const char *buf, size_t len), ci_off_t max_size)
{
    /*we can use  BZ2_bzBuffToBuffDecompress but we need to use our buffer_alloc interface...*/
    int ret;
    unsigned have, written, can_write, out_size;
    ci_off_t unzipped_size;
    bz_stream strm;
    char *out, OUT[CHUNK];

    strm.bzalloc = bzalloc_a_buffer;
    strm.bzfree = bzfree_a_buffer;
    strm.opaque = NULL;
    strm.avail_in = 0;
    strm.next_in = NULL;
    ret = BZ2_bzDecompressInit(&strm, 0, 0);
    if (ret != BZ_OK) {
        ci_debug_printf(1,
                        "Error initializing  bzlib (BZ2_bzDeompressInit return:%d)\n",
                        ret);
        return CI_UNCOMP_ERR_ERROR;
    }

    strm.next_in = (char *)buf;
    strm.avail_in = inlen;

    unzipped_size = 0;

    do {
        if (get_outbuf) {
            out = get_outbuf(out_obj, &out_size);
            strm.next_out = out;
            strm.avail_out = out_size;
            if (!out || !strm.avail_out) {
                BZ2_bzDecompressEnd(&strm);
                return CI_UNCOMP_ERR_OUTPUT;
            }
        } else {
            strm.avail_out = out_size = CHUNK;
            strm.next_out = out = OUT;
        }
        ret = BZ2_bzDecompress(&strm);
        switch (ret) {
        case BZ_PARAM_ERROR:
        case BZ_DATA_ERROR:
        case BZ_DATA_ERROR_MAGIC:
        case BZ_MEM_ERROR:
            BZ2_bzDecompressEnd(&strm);
            return CI_UNCOMP_ERR_ERROR;
        }

        have = out_size - strm.avail_out;
        can_write = (max_size > 0 && (max_size - unzipped_size) < have) ? (max_size - unzipped_size) : have;
        if (!have || (written = writefunc(out_obj, (char *)out, can_write)) != can_write) {
            BZ2_bzDecompressEnd(&strm);
            return CI_UNCOMP_ERR_OUTPUT;
        }
        unzipped_size += written;
        if (written < have) {
            BZ2_bzDecompressEnd(&strm);
            if ( (unzipped_size/inlen) > 100) {
                ci_debug_printf(1, "Compression ratio UncompSize/CompSize = %" PRINTF_OFF_T "/%" PRINTF_OFF_T " = %" PRINTF_OFF_T "! Is it a zip bomb? aborting!\n", (CAST_OFF_T)unzipped_size, (CAST_OFF_T)inlen, (CAST_OFF_T)(unzipped_size/inlen));
                return CI_UNCOMP_ERR_BOMB;  /*Probably compression bomb object*/
            } else {
                ci_debug_printf(4, "Object is bigger than max allowed file\n");
                return CI_UNCOMP_ERR_NONE;
            }
        }
    } while (strm.avail_out == 0);


    BZ2_bzDecompressEnd(&strm);
    return CI_UNCOMP_OK;
}

int ci_bzunzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_bzunzip(inbuf, inlen, outbuf, NULL, write_membuf_func, max_size);
    ci_membuf_write(outbuf, "", 0, 1);
    return ret;
}

int ci_bzunzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    int ret = ci_mem_bzunzip(inbuf, inlen, outbuf, NULL, write_simple_file_func, max_size);
    ci_simple_file_write(outbuf, "", 0, 1);
    return ret;
}

static int bzlib_uncompress_step(const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len)
{
    struct unzipBuf ub;
    ub.buf = unzipped_buf;
    ub.buf_size = *unzipped_buf_len;
    int ret = ci_mem_bzunzip(buf, len,  &ub, get_buf_outbuf, write_once_to_outbuf, len);
    ci_debug_printf(5, "bzlib_uncompress_step: retcode %d, unzipped data: %d\n", ret, (int)ub.out_len);
    if (ub.out_len > 0) {
        *unzipped_buf_len = ub.out_len;
        return CI_OK;
    }
    return CI_ERROR;
}

#else
int ci_bzunzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "bzlib/bzunzip is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}

int ci_bzunzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size)
{
    ci_debug_printf(1, "bzlib/bzunzip is not supported.\n");
    return CI_UNCOMP_ERR_NONE;
}
#endif

int ci_uncompress_preview(int compress_method, const char *buf, int len, char *unzipped_buf,
                          int *unzipped_buf_len)
{
#ifdef HAVE_BZLIB
    if (compress_method == CI_ENCODE_BZIP2)
        return  bzlib_uncompress_step(buf, len, unzipped_buf, unzipped_buf_len);
    else
#endif
#ifdef HAVE_BROTLI
    if (compress_method == CI_ENCODE_BROTLI)
        return  brotli_inflate_step(buf, len, unzipped_buf, unzipped_buf_len);
    else
#endif
#ifdef HAVE_ZLIB
        return zlib_inflate_step(buf, len, unzipped_buf, unzipped_buf_len);
#endif
    return CI_ERROR;
}
