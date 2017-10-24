/*
 *  Copyright (C) 2017 Jeffrey Merkey
 *  Copyright (C) 2017 Trever L. Adams
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
#include "body.h"
#include "simple_api.h"
#include "debug.h"

#include <assert.h>
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

/*return CI_DEFLATE_ERRORS
*/
int ci_generic_compress_to_membuf(int encoding_format, const char *inbuf,
				  size_t inlen, ci_membuf_t *outbuf,
				  ci_off_t max_size)
{
	switch (encoding_format) {
	case CI_ENCODE_NONE:
		return CI_COMP_OK;
		break;
#ifdef HAVE_ZLIB
	case CI_ENCODE_GZIP:
		return ci_gzip_to_membuf(inbuf, inlen, outbuf, max_size);
		break;
	case CI_ENCODE_DEFLATE:
		return ci_deflate_to_membuf(inbuf, inlen, outbuf, max_size);
		break;
#endif
#ifdef HAVE_BZLIB
	case CI_ENCODE_BZIP2:
		return ci_bzzip_to_membuf(inbuf, inlen, outbuf, max_size);
		break;
#endif
#ifdef HAVE_BROTLI
	case CI_ENCODE_BROTLI:
		return ci_brdeflate_to_membuf(inbuf, inlen, outbuf, max_size);
		break;
#endif
	case CI_ENCODE_UNKNOWN:
	default:
		return CI_COMP_ERR_ERROR;
		break;
	}
}

/*return CI_DEFLATE_ERRORS
*/
int ci_generic_compress_to_simple_file(int encoding_format, const char *inbuf,
				       size_t inlen,
				       struct ci_simple_file *outbuf,
				       ci_off_t max_size)
{
	switch (encoding_format) {
	case CI_ENCODE_NONE:
		return CI_COMP_OK;
		break;
#ifdef HAVE_ZLIB
	case CI_ENCODE_GZIP:
		return ci_gzip_to_simple_file(inbuf, inlen, outbuf,
						 max_size);
		break;
	case CI_ENCODE_DEFLATE:
		return ci_deflate_to_simple_file(inbuf, inlen, outbuf,
						 max_size);
		break;
#endif
#ifdef HAVE_BZLIB
	case CI_ENCODE_BZIP2:
		return ci_bzzip_to_simple_file(inbuf, inlen, outbuf,
					       max_size);
		break;
#endif
#ifdef HAVE_BROTLI
	case CI_ENCODE_BROTLI:
		return ci_brdeflate_to_simple_file(inbuf, inlen, outbuf,
						   max_size);
		break;
#endif
	case CI_ENCODE_UNKNOWN:
	default:
		return CI_COMP_ERR_ERROR;
		break;
	}
}

#define CHUNK 8192

static int write_membuf_func(void *obj, const char *buf, size_t len)
{
	return ci_membuf_write((ci_membuf_t *)obj, buf, len, 0);
}

static int write_simple_file_func(void *obj, const char *buf, size_t len)
{
	return ci_simple_file_write((ci_simple_file_t *)obj, buf, len, 0);
}

#ifdef HAVE_BROTLI
#define DEFAULT_LGWIN	 22
#define DEFAULT_QUALITY	 11
#define kFileBufferSize 16384

BROTLI_BOOL brotli_compress(BrotliEncoderState* s, const char *buf, int inlen,
			    void *outbuf,
			    char *(*get_outbuf)(void *obj, unsigned int *len),
			    int (*writefunc)(void *obj,
					     const char *buf, size_t len),
			    ci_off_t max_size)
{
	size_t available_in;
	const uint8_t* next_in;
	size_t available_out;
	uint8_t* next_out;
	unsigned have, written;
	long long outsize;
	int result;
	size_t total_out = 0;
	uint8_t out[kFileBufferSize];

	ci_debug_printf(4, "data-compression:  brotli compress called size: %d\n",
			inlen);

	next_in = (uint8_t *)buf;
	available_in = inlen;
	outsize = 0;
	for (;;) {
		available_out = kFileBufferSize;
		next_out = out;
		result = BrotliEncoderCompressStream(s,
						     available_in
						     ? BROTLI_OPERATION_PROCESS
						     : BROTLI_OPERATION_FINISH,
						     &available_in, &next_in,
						     &available_out, &next_out,
						     &total_out);
		if (!result) {
			/* Should detect OOM? */
			ci_debug_printf(4, "data-compression:  brotli failed to compress data\n");
			return BROTLI_FALSE;
		}
		if (available_out != kFileBufferSize) {
			have = kFileBufferSize - available_out;
			if (!have || (written =
				      writefunc(outbuf, (char *)out, have)) !=
			    have) {
				ci_debug_printf(4, "data-compression:  brotli data corrupt\n");
				return BROTLI_FALSE;
			}
			outsize += written;
		}
		if (BrotliEncoderIsFinished(s)) {
			ci_debug_printf(4, "data-compression:  brotli total compressed size %lld (%lld) ...\n",
					outsize, (long long) total_out);
			return BROTLI_TRUE;
		}
	}
}

int ci_mem_brdeflate(const char *inbuf, int inlen, void *outbuf,
		     char *(*get_outbuf)(void *obj, unsigned int *len),
		     int (*writefunc)(void *obj, const char *buf, size_t len),
		     ci_off_t max_size)
{
	BROTLI_BOOL ccode = BROTLI_TRUE;
	BrotliEncoderState *s;

	s = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	if (!s) {
		ci_debug_printf(4, "data-compression:  brotli out of memory\n");
		return -1;
	}
	BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, BROTLI_MODE_TEXT);
	BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, DEFAULT_QUALITY);
	BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, DEFAULT_LGWIN);
	ccode = brotli_compress(s, inbuf, inlen, outbuf, get_outbuf, writefunc,
				max_size);
	BrotliEncoderDestroyInstance(s);

	if (!ccode)
		return -1;
	return 1;
}

int ci_brdeflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
			   ci_off_t max_size)
{
	int ret = ci_mem_brdeflate(inbuf, inlen, outbuf, NULL,
				   write_membuf_func, max_size);
	ci_membuf_write(outbuf, "", 0, 1);
	return ret;
}

int ci_brdeflate_to_simple_file(const char *inbuf, size_t inlen,
				struct ci_simple_file *outbuf,
				ci_off_t max_size)
{
	int ret = ci_mem_brdeflate(inbuf, inlen, outbuf, NULL,
				   write_simple_file_func, max_size);
	ci_simple_file_write(outbuf, "", 0, 1);
	return ret;
}

#else
int ci_brdeflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
			   ci_off_t max_size)
{
	ci_debug_printf(1, "brotliencode is not supported.\n");
	return CI_COMP_ERR_NONE;
}

int ci_brdeflate_to_simple_file(const char *inbuf, size_t inlen,
				struct ci_simple_file *outbuf,
				ci_off_t max_size)
{
	ci_debug_printf(1, "brotliencode is not supported.\n");
	return CI_COMP_ERR_NONE;
}
#endif

#ifdef HAVE_ZLIB
#define ZIP_HEAD_CRC     0x02   /* bit 1 set: header CRC present */
#define ZIP_EXTRA_FIELD  0x04   /* bit 2 set: extra field present */
#define ZIP_ORIG_NAME    0x08   /* bit 3 set: original file name present */
#define ZIP_COMMENT      0x10   /* bit 4 set: file comment present */

#define windowBits 15
#define GZIP_ENCODING 16

static void *alloc_a_buffer(void *op, unsigned int items, unsigned int size)
{
	return ci_buffer_alloc(items*size);
}

static void free_a_buffer(void *op, void *ptr)
{
	ci_buffer_free(ptr);
}

/*return CI_DEFLATE_ERRORS
*/
static int strm_init(z_stream * strm, int which, int inlen)
{
	int ret;

	strm->zalloc = alloc_a_buffer;
	strm->zfree  = free_a_buffer;
	strm->opaque = Z_NULL;

	switch (which) {
	case CI_ENCODE_DEFLATE:
		ci_debug_printf(4, "data-compression:  deflate called size: %d\n",
				inlen);
		ret = deflateInit(strm, Z_DEFAULT_COMPRESSION);
		break;
	case CI_ENCODE_GZIP:
	default:
		ci_debug_printf(4, "data-compression:  gzip called size: %d\n", inlen);
		ret = deflateInit2(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				   windowBits | GZIP_ENCODING, 8,
				   Z_DEFAULT_STRATEGY);
		break;
	}
	return ret;
}

int ci_mem_deflate(const char *inbuf, size_t inlen, void *out_obj,
		   char *(*get_outbuf)(void *obj, unsigned int *len),
		   int (*writefunc)(void *obj, const char *buf, size_t len),
		   ci_off_t max_size, int which) {

	int ret = Z_STREAM_END, written, outsize = 0;
	unsigned char out[CHUNK];
	z_stream strm;

	strm_init(&strm, which, inlen);
	strm.next_in = (unsigned char *) inbuf;
	strm.avail_in = inlen;
	do {
		int have;
		strm.avail_out = CHUNK;
		strm.next_out = out;
		ret = deflate(&strm, Z_FINISH);
		have = CHUNK - strm.avail_out;
		if ((written = writefunc(out_obj, (char *)out, have)) != have) {
			deflateEnd(&strm);
			return CI_COMP_ERR_CORRUPT;
		}
		outsize += written;
	}
	while (strm.avail_out == 0);

	deflateEnd (&strm);

	switch(which) {
	case CI_ENCODE_GZIP:
		ci_debug_printf(4, "data-compression:  gzip total compressed size %d ...\n", outsize);
		break;
	case CI_ENCODE_DEFLATE:
	default:
		ci_debug_printf(4, "data-compression:  deflate total compressed size %d ...\n", outsize);
		break;
	}
	return ret == Z_STREAM_END ? CI_COMP_OK : CI_COMP_ERR_CORRUPT;
}

int ci_deflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
			 ci_off_t max_size)
{
	int ret = ci_mem_deflate(inbuf, inlen, outbuf, NULL, write_membuf_func,
				 max_size, CI_ENCODE_DEFLATE);
	ci_membuf_write(outbuf, "", 0, 1);
	return ret;
}

int ci_deflate_to_simple_file(const char *inbuf, size_t inlen,
			      struct ci_simple_file *outbuf, ci_off_t max_size)
{
	int ret = ci_mem_deflate(inbuf, inlen, outbuf, NULL,
				 write_simple_file_func, max_size,
				 CI_ENCODE_DEFLATE);
	ci_simple_file_write(outbuf, "", 0, 1);
	return ret;
}

int ci_gzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
		      ci_off_t max_size)
{
	int ret = ci_mem_deflate(inbuf, inlen, outbuf, NULL, write_membuf_func,
				 max_size, CI_ENCODE_GZIP);
	ci_membuf_write(outbuf, "", 0, 1);
	return ret;
}

int ci_gzip_to_simple_file(const char *inbuf, size_t inlen,
			   struct ci_simple_file *outbuf, ci_off_t max_size)
{
	int ret = ci_mem_deflate(inbuf, inlen, outbuf, NULL,
				 write_simple_file_func,
				 max_size, CI_ENCODE_GZIP);
	ci_simple_file_write(outbuf, "", 0, 1);
	return ret;
}

#else
int ci_deflate_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
			 ci_off_t max_size)
{
	ci_debug_printf(1, "zlib/inflate is not supported.\n");
	return CI_COMP_ERR_NONE;
}

int ci_deflate_to_simple_file(const char *inbuf, size_t inlen,
			      struct ci_simple_file *outbuf, ci_off_t max_size)
{
	ci_debug_printf(1, "zlib/inflate is not supported.\n");
	return CI_COMP_ERR_NONE;
}
int ci_gzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
			 ci_off_t max_size)
{
	ci_debug_printf(1, "gzip is not supported.\n");
	return CI_COMP_ERR_NONE;
}

int ci_gzip_to_simple_file(const char *inbuf, size_t inlen,
			      struct ci_simple_file *outbuf, ci_off_t max_size)
{
	ci_debug_printf(1, "gzip is not supported.\n");
	return CI_COMP_ERR_NONE;
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

int ci_mem_bzzip(const char *buf, int inlen,  void *out_obj,
		 char *(*get_outbuf)(void *obj, unsigned int *len),
		 int (*writefunc)(void *obj, const char *buf, size_t len),
		 ci_off_t max_size)
{
	int ret;
	unsigned have, written;
	long long outsize;
	bz_stream strm;
	char out[CHUNK];

	ci_debug_printf(4, "data-compression:  bzip called size: %d\n", inlen);

	strm.bzalloc = bzalloc_a_buffer;
	strm.bzfree = bzfree_a_buffer;
	strm.opaque = NULL;
	strm.avail_in = 0;
	strm.next_in = NULL;
	ret = BZ2_bzCompressInit(&strm,
				 9, // number of 100k blocks 9 is best compression (1-9)
				 0, // verbosity (0-4) 0-none 4 max
				 30); // work factor - 30 is default (0-250)
	if (ret != BZ_OK) {
		ci_debug_printf(1,
				"data-compression:  error initializing bzlib (BZ2_bzCompressInit return:%d)\n",
				ret);
		return CI_ERROR;
	}
	strm.next_in = (char *)buf;
	strm.avail_in = inlen;
	outsize = 0;

	do {
		strm.avail_out = CHUNK;
		strm.next_out = out;

		ret = BZ2_bzCompress(&strm, BZ_FINISH);
		switch (ret) {
		case BZ_PARAM_ERROR:
		case BZ_DATA_ERROR:
		case BZ_DATA_ERROR_MAGIC:
		case BZ_MEM_ERROR:
			BZ2_bzCompressEnd(&strm);
			return CI_ERROR;
		}

		have = CHUNK - strm.avail_out;
		if (!have ||
		    (written = writefunc(out_obj, (char *)out, have)) != have) {
			BZ2_bzCompressEnd(&strm);
			return CI_COMP_ERR_OUTPUT;
		}
		outsize += written;
	} while (strm.avail_out == 0);

	BZ2_bzCompressEnd(&strm);
	ci_debug_printf(4, "data-compression:  bzip total compressed size %lld ...\n",
			outsize);
	return CI_COMP_OK;
}

int ci_bzzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
		       ci_off_t max_size)
{
	int ret = ci_mem_bzzip(inbuf, inlen, outbuf, NULL, write_membuf_func,
			       max_size);
	ci_membuf_write(outbuf, "", 0, 1);
	return ret;
}

int ci_bzzip_to_simple_file(const char *inbuf, size_t inlen,
			    struct ci_simple_file *outbuf, ci_off_t max_size)
{
	int ret = ci_mem_bzzip(inbuf, inlen, outbuf, NULL,
			       write_simple_file_func, max_size);
	ci_simple_file_write(outbuf, "", 0, 1);
	return ret;
}

#else
int ci_bzzip_to_membuf(const char *inbuf, size_t inlen, ci_membuf_t *outbuf,
		       ci_off_t max_size)
{
	ci_debug_printf(1, "bzlib/bzzip is not supported.\n");
	return CI_COMP_ERR_NONE;
}

int ci_bzzip_to_simple_file(const char *inbuf, size_t inlen,
			    struct ci_simple_file *outbuf, ci_off_t max_size)
{
	ci_debug_printf(1, "bzlib/bzzip is not supported.\n");
	return CI_COMP_ERR_NONE;
}
#endif
