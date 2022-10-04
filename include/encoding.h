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

#ifndef __C_ICAP_ENCODING_H
#define __C_ICAP_ENCODING_H

#include "c-icap.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Decodes a base64 encoded string.
 \ingroup UTILITY
 *
 \param str   is a buffer which holds the base64 encoded data
 \param result    is a buffer where the decoded data will be stored
 \param len    is the length of the result buffer
 \return the number of decoded bytes
 */
CI_DECLARE_FUNC(int) ci_base64_decode(const char *str,char *result,int len);

/**
 * Produces a base64 encoded string.
 \ingroup UTILITY
 *
 \param data   is a buffer which holds the data to be encoded
 \param datalen    is the length of the data buffer
 \param out    is a buffer where the encoded data will be stored
 \param outlen    is the length of the out buffer
 \return the number of decoded bytes
 */
CI_DECLARE_FUNC(int) ci_base64_encode(const unsigned char *data, size_t datalen, char *out, size_t outlen);

enum {
    CI_ENCODE_UNKNOWN = -1,
    CI_ENCODE_NONE = 0,
    CI_ENCODE_GZIP,
    CI_ENCODE_DEFLATE,
    CI_ENCODE_BZIP2,
    CI_ENCODE_BROTLI
};

// Encoding configuration parameters
/**
   Zlib default compression window size to use (between 1 and 15)
   The default value is 15
   \ingroup UTILITY
*/
CI_DECLARE_DATA extern int CI_ZLIB_WINDOW_SIZE;

/**
   Zlib default memory amount level to use (between 1 and 9)
   The default value is 8
   \ingroup UTILITY
*/
CI_DECLARE_DATA extern int CI_ZLIB_MEMLEVEL;

/**
   Brotli default compression quality to use (between 0 and 11).
   The higher quality results to a slower compression. Higher
   values than 4 result in high CPU usage and very slow compression
   procedure and should not be used.
   The default value is 4.
   \ingroup UTILITY
*/
CI_DECLARE_DATA extern int CI_BROTLI_QUALITY;

/**
   Brotli maximum input block size to use (between 16 and 24).
   Bigger input block size consumes more memory but allows better compression.
   The default value is 24.
   \ingroup UTILITY
*/
CI_DECLARE_DATA extern int CI_BROTLI_MAX_INPUT_BLOCK;

/**
   Brotli default compression window size to use (between 10 and 24).
   Bigger window sizes can improve compression quality, but require more memory.
   The default value is 22.
   \ingroup UTILITY
*/
CI_DECLARE_DATA extern int CI_BROTLI_WINDOW;

/**
 * Return the encoding method integer representation from string.
 \ingroup UTILITY
 *
 \param content_encoding The content encoding name
 \return the CI_ENCODE_* representation
*/
CI_DECLARE_FUNC(int) ci_encoding_method(const char *content_encoding);

/**
 * Return the string representation of an encoding method.
 \ingroup UTILITY
 *
 \param encoding The integer representation of content encoding method
 \return the string method
*/
CI_DECLARE_FUNC(const char *) ci_encoding_method_str(int encoding);

/**
 * Uncompress a zipped string.
 \ingroup UTILITY
 *
 \param compress_method CI_ENCODE_GZIP, CI_ENCODED_DEFLATE or CI_CI_ENCODE_BZIP2
 \param buf   is a buffer which holds the zipped data
 \param len is the length of the buffer buf
 \param unzipped_buf  is the buffer where to store unzipped data
 \param unzipped_buf_len  is the length of the buffer to store unzipped data,
 *      and updated with the length of unzipped data
 \return CI_OK on success CI_ERROR on error
 */
CI_DECLARE_FUNC(int) ci_uncompress_preview(int compress_method, const char *buf, int len, char *unzipped_buf, int *unzipped_buf_len);

enum CI_UNCOMPRESS_ERRORS {
    CI_UNCOMP_ERR_BOMB = -4,
    CI_UNCOMP_ERR_CORRUPT = -3,
    CI_UNCOMP_ERR_OUTPUT = -2,
    CI_UNCOMP_ERR_ERROR = -1,
    CI_UNCOMP_ERR_NONE = 0,
    CI_UNCOMP_OK = 1,
};

enum CI_COMPRESS_ERRORS {
    CI_COMP_ERR_BOMB = -4,
    CI_COMP_ERR_CORRUPT = -3,
    CI_COMP_ERR_OUTPUT = -2,
    CI_COMP_ERR_ERROR = -1,
    CI_COMP_ERR_NONE = 0,
    CI_COMP_OK = 1,
};

/**
 * Return a string representation of a decompress error code.
 \ingroup UTILITY
 *
 \param err a CI_UNCOMPRESS_ERRORS error code
*/
CI_DECLARE_FUNC(const char *) ci_decompress_error(int err);

/**
   Deprecated
 */
CI_DECLARE_FUNC(const char *) ci_inflate_error(int err);

struct ci_membuf;
struct ci_simple_file;

/*  Data Decompression core functions */

/**
 * Uncompress any compressed data that c-icap understands and writes the output to the outbuf
 * object, regardless of algorithm
 \ingroup UTILITY
 *
 \param encoding_format   is the enum for the encoding type
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_decompress_to_membuf(int encoding_format, const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_decompress_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_decompress_to_simple_file(int encoding_format, const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress deflate/gzip compressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_inflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_inflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_inflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress bzip2 compressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_bzunzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_bzunzip_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_bzunzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Uncompress brotli compressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the zipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put unzipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_UNCOMP_OK on success, CI_UNCOMP_ERR_NONE, if maxsize exceed, an
 *       CI_UNCOMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_brinflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_brinflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_brinflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/*  Data Compression core functions */

/**
 * Compress any uncompressed data that c-icap understands and writes the output to the outbuf
 * object, regardless of algorithm
 \ingroup UTILITY
 *
 \param encoding_format   is the enum for the encoding type
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_compress_to_membuf(int encoding_format, const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_compress_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_compress_to_simple_file(int encoding_format, const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress deflate uncompressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_deflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_deflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_deflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress gzip uncompressed data and writes the output to the outbuf
 * object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_gzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_deflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_gzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress bzip2 uncompressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_bzzip_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_bzzip_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_bzzip_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Compress brotli uncompressed data and writes the output to the outbuf object
 \ingroup UTILITY
 *
 \param inbuf   is a buffer which holds the unzipped data
 \param inlen is the length of the buffer buf
 \param outbuf where to put zipped data
 \param max_size if it is greater than zero, the output data limit
 \return CI_COMP_OK on success, CI_COMP_ERR_NONE, if maxsize exceed, an
 *       CI_COMPRESS_ERRORS code otherwise
 */
CI_DECLARE_FUNC(int) ci_brdeflate_to_membuf(const char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size);

/**
 \ingroup UTILITY
 \copydoc ci_brdeflate_to_membuf(char *inbuf, size_t inlen, struct ci_membuf *outbuf, ci_off_t max_size)
 */
CI_DECLARE_FUNC(int) ci_brdeflate_to_simple_file(const char *inbuf, size_t inlen, struct ci_simple_file *outbuf, ci_off_t max_size);

/**
 * Decodes a base64 encoded string, and also allocate memory for the result.
 \ingroup UTILITY
 *
 \param str is a buffer which holds the base64 encoded data
 \return a pointer to the decoded string. It uses malloc to allocate space for
 *       the decoded string so the free function should used to release the
 *       allocated memory.
 */
CI_DECLARE_FUNC(char *) ci_base64_decode_dup(const char *str);

#ifdef __cplusplus
}
#endif

#endif // __C_ICAP_ENCODING_H
