/*
 * Copyright (C) 2013-2015 Willy Tarreau <w@1wt.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _SLZ_H
#define _SLZ_H

#include <inttypes.h>

/* We have two macros UNALIGNED_LE_OK and UNALIGNED_FASTER. The latter indicates
 * that using unaligned data is faster than a simple shift. On x86 32-bit at
 * least it is not the case as the per-byte access is 30% faster. A core2-duo on
 * x86_64 is 7% faster to read one byte + shifting by 8 than to read one word,
 * but a core i5 is 7% faster doing the unaligned read, so we privilege more
 * recent implementations here.
 */
#if defined(__x86_64__)
#define UNALIGNED_LE_OK
#define UNALIGNED_FASTER
#define USE_64BIT_QUEUE
#define HAVE_FAST_MULT
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
#define UNALIGNED_LE_OK
//#define UNALIGNED_FASTER
#elif defined(__ARMEL__) && defined(__ARM_ARCH_7A__)
#define UNALIGNED_LE_OK
#define UNALIGNED_FASTER
#elif defined(__ARM_ARCH_8A) || defined(__ARM_FEATURE_UNALIGNED)
#define UNALIGNED_LE_OK
#define UNALIGNED_FASTER
#define HAVE_FAST_MULT
#endif

/* Log2 of the size of the hash table used for the references table. */
#define HASH_BITS 13

enum slz_state {
	SLZ_ST_INIT,  /* stream initialized */
	SLZ_ST_EOB,   /* header or end of block already sent */
	SLZ_ST_FIXED, /* inside a fixed huffman sequence */
	SLZ_ST_LAST,  /* last block, BFINAL sent */
	SLZ_ST_DONE,  /* BFINAL+EOB sent BFINAL */
	SLZ_ST_END    /* end sent (BFINAL, EOB, CRC + len) */
};

enum {
	SLZ_FMT_GZIP,    /* RFC1952: gzip envelope and crc32 for CRC */
	SLZ_FMT_ZLIB,    /* RFC1950: zlib envelope and adler-32 for CRC */
	SLZ_FMT_DEFLATE, /* RFC1951: raw deflate, and no crc */
};

struct slz_stream {
#ifdef USE_64BIT_QUEUE
	uint64_t queue; /* last pending bits, LSB first */
#else
	uint32_t queue; /* last pending bits, LSB first */
#endif
	uint32_t qbits; /* number of bits in queue, < 8 on 32-bit, < 32 on 64-bit */
	unsigned char *outbuf; /* set by encode() */
	uint16_t state; /* one of slz_state */
	uint8_t level:1; /* 0 = no compression, 1 = compression */
	uint8_t format:2; /* SLZ_FMT_* */
	uint8_t unused1; /* unused for now */
	uint32_t crc32;
	uint32_t ilen;
};

/* Functions specific to rfc1951 (deflate) */
void slz_prepare_dist_table(); /* obsolete, not needed anymore */
long slz_rfc1951_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more);
int slz_rfc1951_init(struct slz_stream *strm, int level);
int slz_rfc1951_flush(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1951_finish(struct slz_stream *strm, unsigned char *buf);

/* Functions specific to rfc1952 (gzip) */
void slz_make_crc_table(void); /* obsolete, not needed anymore */
uint32_t slz_crc32_by1(uint32_t crc, const unsigned char *buf, int len);
uint32_t slz_crc32_by4(uint32_t crc, const unsigned char *buf, int len);
long slz_rfc1952_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more);
int slz_rfc1952_send_header(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1952_init(struct slz_stream *strm, int level);
int slz_rfc1952_flush(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1952_finish(struct slz_stream *strm, unsigned char *buf);

/* Functions specific to rfc1950 (zlib) */
uint32_t slz_adler32_by1(uint32_t crc, const unsigned char *buf, int len);
uint32_t slz_adler32_block(uint32_t crc, const unsigned char *buf, long len);
long slz_rfc1950_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more);
int slz_rfc1950_send_header(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1950_init(struct slz_stream *strm, int level);
int slz_rfc1950_flush(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1950_finish(struct slz_stream *strm, unsigned char *buf);

/* generic functions */

/* Initializes stream <strm>. It will configure the stream to use format
 * <format> for the data, which must be one of SLZ_FMT_*. The compression level
 * passed in <level> is set. This value can only be 0 (no compression) or 1
 * (compression) and other values will lead to unpredictable behaviour. The
 * function should always return 0.
 */
static inline int slz_init(struct slz_stream *strm, int level, int format)
{
	int ret;

	if (format == SLZ_FMT_GZIP)
		ret = slz_rfc1952_init(strm, level);
	else if (format == SLZ_FMT_ZLIB)
		ret = slz_rfc1950_init(strm, level);
	else { /* deflate for anything else */
		ret = slz_rfc1951_init(strm, level);
		strm->format = format;
	}
	return ret;
}

/* Encodes the block according to the format used by the stream. This means
 * that the CRC of the input block may be computed according to the CRC32 or
 * adler-32 algorithms. The number of output bytes is returned.
 */
static inline long slz_encode(struct slz_stream *strm, void *out,
                              const void *in, long ilen, int more)
{
	long ret;

	if (strm->format == SLZ_FMT_GZIP)
		ret = slz_rfc1952_encode(strm, (unsigned char *) out, (const unsigned char *) in, ilen, more);
	else if (strm->format == SLZ_FMT_ZLIB)
		ret = slz_rfc1950_encode(strm, (unsigned char *) out, (const unsigned char *) in, ilen, more);
	else /* deflate for other ones */
		ret = slz_rfc1951_encode(strm, (unsigned char *) out, (const unsigned char *) in, ilen, more);

	return ret;
}

/* Flushes pending bits and sends the trailer for stream <strm> into buffer
 * <buf> if needed. When it's done, the stream state is updated to SLZ_ST_END.
 * It returns the number of bytes emitted. The trailer consists in flushing the
 * possibly pending bits from the queue (up to 24 bits), rounding to the next
 * byte, then 4 bytes for the CRC when doing zlib/gzip, then another 4 bytes
 * for the input length for gzip. That may amount to 4+4+4 = 12 bytes, that the
 * caller must ensure are available before calling the function. Note that if
 * the initial header was never sent, it will be sent first as well (up to 10
 * extra bytes).
 */
static inline int slz_finish(struct slz_stream *strm, void *buf)
{
	int ret;

	if (strm->format == SLZ_FMT_GZIP)
		ret = slz_rfc1952_finish(strm, (unsigned char *) buf);
	else if (strm->format == SLZ_FMT_ZLIB)
		ret = slz_rfc1950_finish(strm, (unsigned char *) buf);
	else /* deflate for other ones */
		ret = slz_rfc1951_finish(strm, (unsigned char *) buf);

	return ret;
}

/* Flushes any pending data for stream <strm> into buffer <buf>, then emits an
 * empty literal block to byte-align the output, allowing to completely flush
 * the queue. Note that if the initial header was never sent, it will be sent
 * first as well (0, 2 or 10 extra bytes). This requires that the output buffer
 * still has this plus the size of the queue available (up to 4 bytes), plus
 * one byte for (BFINAL,BTYPE), plus 4 bytes for LEN+NLEN, or a total of 19
 * bytes in the worst case. The number of bytes emitted is returned. It is
 * guaranteed that the queue is empty on return. This may cause some overhead
 * by adding needless 5-byte blocks if called to often.
 */
static inline int slz_flush(struct slz_stream *strm, void *buf)
{
	int ret;

	if (strm->format == SLZ_FMT_GZIP)
		ret = slz_rfc1952_flush(strm, (unsigned char *) buf);
	else if (strm->format == SLZ_FMT_ZLIB)
		ret = slz_rfc1950_flush(strm, (unsigned char *) buf);
	else /* deflate for other ones */
		ret = slz_rfc1951_flush(strm, (unsigned char *) buf);

	return ret;
}

#endif
