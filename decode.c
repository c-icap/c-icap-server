/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
 *
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
#include "simple_api.h"
#include "debug.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
#ifdef HAVE_BZLIB
#include <bzlib.h>
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
    
    for (i=len; i>3; i-=3) {
	
	/*if one of the last 4 bytes going to be proccessed is not valid just 
	  stops processing. This "if" cover the '\0' string termination character
	  of str (because base64_table[0]=255)
	 */
	if(base64_table[*str]>63 || base64_table[*(str+1)] > 63 || 
	   base64_table[*(str+2)] > 63 ||base64_table[*(str+3)] > 63)
	    break;
	
	/*6 bits from the first + 2 last bits from second*/
	*(result++)=(base64_table[*str] << 2) | (base64_table[*(str+1)] >>4);
	/*last 4 bits from second + first 4 bits from third*/
	*(result++)=(base64_table[*(str+1)] << 4) | (base64_table[*(str+2)] >>2);
	/*last 2 bits from third + 6 bits from forth */
	*(result++)=(base64_table[*(str+2)] << 6) | (base64_table[*(str+3)]);
	str += 4;
    }
    *result='\0';
    return len-i;
}


char *ci_base64_decode_dup(const char *encoded) 
{
    int len;
    char *result;
    len=strlen(encoded);
    len=((len+3)/4)*3+1;
    if(!(result=malloc(len*sizeof(char))))
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
    for (i = 0, k=0; i < (len - 3) && k < (outlen - 4); i+=3) {
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
int url_decoder(const char *input,char *output, int output_len)
{
    int i, k;
    char str[3];
    
    i = 0;
    k = 0;
    while ((input[i] != '\0') && (k < output_len-1)) {
	if (input[i] == '%'){ 
	    str[0] = input[i+1];
	    str[1] = input[i+2];
	    str[2] = '\0';
	    output[k] = strtol(str, NULL, 16);
	    i = i + 3;
	}
	else if (input[i] == '+') {
	    output[k] = ' ';
	    i++;
	}
	else {
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

int url_decoder2(char *input)
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
	}
	else if (input[i] == '+') {
	    input[k] = ' ';
	    i++;
	}
	else {
	    input[k] = input[i];
	    i++;
	}
	k++;
    }
    input[k] = '\0';
    return 1;
}

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

static int zlib_inflate(const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len)
{
     int ret, retriable;
     z_stream strm;
     strm.zalloc = alloc_a_buffer;
     strm.zfree = free_a_buffer;
     strm.opaque = Z_NULL;
     strm.avail_in = 0;
     strm.next_in = Z_NULL;

     ret = inflateInit2(&strm, 32 + 15);        /*MAX_WBITS + 32 for both deflate and gzip decompress */
 
     retriable = 1;
zlib_inflate_retry:
     if (ret != Z_OK) {
          ci_debug_printf(1,
                          "Error initializing  zlib (inflateInit2 return:%d)\n",
                          ret);
          return CI_ERROR;
     }

     strm.next_in = (Bytef *) buf;
     strm.avail_in = len;

     strm.avail_out = *unzipped_buf_len;
     strm.next_out = (Bytef *) unzipped_buf;
     ret = inflate(&strm, Z_NO_FLUSH);
     inflateEnd(&strm);

     switch (ret) {
     case Z_NEED_DICT:
     case Z_DATA_ERROR:
          if (retriable) {
              ret = inflateInit2(&strm, -15);
              retriable = 0;
              goto zlib_inflate_retry;
          }
     case Z_MEM_ERROR:
          return CI_ERROR;
     }

     /*If the data was not enough to get decompressed data
       return error
     */
     if (*unzipped_buf_len == strm.avail_out && ret != Z_STREAM_END)
         return CI_ERROR;
     
     *unzipped_buf_len = *unzipped_buf_len - strm.avail_out;
     return CI_OK;
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

static int bzlib_uncompress(const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len)
{
    /*we can use  BZ2_bzBuffToBuffDecompress but we need to use our buffer_alloc interface...*/
     int ret;
     bz_stream strm;
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
          return CI_ERROR;
     }

     strm.next_in = (char *)buf;
     strm.avail_in = len;
     strm.avail_out = *unzipped_buf_len;
     strm.next_out = unzipped_buf;
     ret = BZ2_bzDecompress(&strm);
     BZ2_bzDecompressEnd(&strm);
     switch (ret) {
     case BZ_PARAM_ERROR:
     case BZ_DATA_ERROR:
     case BZ_DATA_ERROR_MAGIC:
     case BZ_MEM_ERROR:
         return CI_ERROR;
     }

     /*If the data was not enough to get decompressed data
       return error
      */
     if (*unzipped_buf_len == strm.avail_out && ret != BZ_STREAM_END)
         return CI_ERROR;

     *unzipped_buf_len = *unzipped_buf_len - strm.avail_out;
     return CI_OK;
}
#endif

int ci_uncompress_preview(int compress_method, const char *buf, int len, char *unzipped_buf,
                  int *unzipped_buf_len)
{
#ifdef HAVE_BZLIB
    if (compress_method == CI_ENCODE_BZIP2)
        return  bzlib_uncompress(buf, len, unzipped_buf, unzipped_buf_len);
    else
#endif
#ifdef HAVE_ZLIB
        return zlib_inflate(buf, len, unzipped_buf, unzipped_buf_len);
#endif
    return CI_ERROR;
}
