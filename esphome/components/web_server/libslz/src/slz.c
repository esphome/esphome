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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "slz.h"
#include "tables.h"

/* First, RFC1951-specific declarations and extracts from the RFC.
 *
 * RFC1951 - deflate stream format


             * Data elements are packed into bytes in order of
               increasing bit number within the byte, i.e., starting
               with the least-significant bit of the byte.
             * Data elements other than Huffman codes are packed
               starting with the least-significant bit of the data
               element.
             * Huffman codes are packed starting with the most-
               significant bit of the code.

      3.2.3. Details of block format

         Each block of compressed data begins with 3 header bits
         containing the following data:

            first bit       BFINAL
            next 2 bits     BTYPE

         Note that the header bits do not necessarily begin on a byte
         boundary, since a block does not necessarily occupy an integral
         number of bytes.

         BFINAL is set if and only if this is the last block of the data
         set.

         BTYPE specifies how the data are compressed, as follows:

            00 - no compression
            01 - compressed with fixed Huffman codes
            10 - compressed with dynamic Huffman codes
            11 - reserved (error)

      3.2.4. Non-compressed blocks (BTYPE=00)

         Any bits of input up to the next byte boundary are ignored.
         The rest of the block consists of the following information:

              0   1   2   3   4...
            +---+---+---+---+================================+
            |  LEN  | NLEN  |... LEN bytes of literal data...|
            +---+---+---+---+================================+

         LEN is the number of data bytes in the block.  NLEN is the
         one's complement of LEN.

      3.2.5. Compressed blocks (length and distance codes)

         As noted above, encoded data blocks in the "deflate" format
         consist of sequences of symbols drawn from three conceptually
         distinct alphabets: either literal bytes, from the alphabet of
         byte values (0..255), or <length, backward distance> pairs,
         where the length is drawn from (3..258) and the distance is
         drawn from (1..32,768).  In fact, the literal and length
         alphabets are merged into a single alphabet (0..285), where
         values 0..255 represent literal bytes, the value 256 indicates
         end-of-block, and values 257..285 represent length codes
         (possibly in conjunction with extra bits following the symbol
         code) as follows:

Length encoding :
                Extra               Extra               Extra
            Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
            ---- ---- ------     ---- ---- -------   ---- ---- -------
             257   0     3       267   1   15,16     277   4   67-82
             258   0     4       268   1   17,18     278   4   83-98
             259   0     5       269   2   19-22     279   4   99-114
             260   0     6       270   2   23-26     280   4  115-130
             261   0     7       271   2   27-30     281   5  131-162
             262   0     8       272   2   31-34     282   5  163-194
             263   0     9       273   3   35-42     283   5  195-226
             264   0    10       274   3   43-50     284   5  227-257
             265   1  11,12      275   3   51-58     285   0    258
             266   1  13,14      276   3   59-66

Distance encoding :
                  Extra           Extra               Extra
             Code Bits Dist  Code Bits   Dist     Code Bits Distance
             ---- ---- ----  ---- ----  ------    ---- ---- --------
               0   0    1     10   4     33-48    20    9   1025-1536
               1   0    2     11   4     49-64    21    9   1537-2048
               2   0    3     12   5     65-96    22   10   2049-3072
               3   0    4     13   5     97-128   23   10   3073-4096
               4   1   5,6    14   6    129-192   24   11   4097-6144
               5   1   7,8    15   6    193-256   25   11   6145-8192
               6   2   9-12   16   7    257-384   26   12  8193-12288
               7   2  13-16   17   7    385-512   27   12 12289-16384
               8   3  17-24   18   8    513-768   28   13 16385-24576
               9   3  25-32   19   8   769-1024   29   13 24577-32768

      3.2.6. Compression with fixed Huffman codes (BTYPE=01)

         The Huffman codes for the two alphabets are fixed, and are not
         represented explicitly in the data.  The Huffman code lengths
         for the literal/length alphabet are:

                   Lit Value    Bits        Codes
                   ---------    ----        -----
                     0 - 143     8          00110000 through
                                            10111111
                   144 - 255     9          110010000 through
                                            111111111
                   256 - 279     7          0000000 through
                                            0010111
                   280 - 287     8          11000000 through
                                            11000111

         The code lengths are sufficient to generate the actual codes,
         as described above; we show the codes in the table for added
         clarity.  Literal/length values 286-287 will never actually
         occur in the compressed data, but participate in the code
         construction.

         Distance codes 0-31 are represented by (fixed-length) 5-bit
         codes, with possible additional bits as shown in the table
         shown in Paragraph 3.2.5, above.  Note that distance codes 30-
         31 will never actually occur in the compressed data.

*/

/* back references, built in a way that is optimal for 32/64 bits */
union ref {
	struct {
		uint32_t pos;
		uint32_t word;
	} by32;
	uint64_t by64;
};

#if defined(USE_64BIT_QUEUE) && defined(UNALIGNED_LE_OK)

/* enqueue code x of <xbits> bits (LSB aligned, at most 24) and copy complete
 * 32-bit words into output buffer. X must not contain non-zero bits above
 * xbits.
 */
static inline void enqueue24(struct slz_stream *strm, uint64_t x, uint32_t xbits)
{
	uint64_t queue = strm->queue + (x << strm->qbits);
	uint32_t qbits = strm->qbits + xbits;

	if (__builtin_expect(qbits >= 32, 1)) {
		*(uint32_t *)strm->outbuf = queue;
		queue >>= 32;
		qbits -= 32;
		strm->outbuf += 4;
	}

	strm->queue = queue;
	strm->qbits = qbits;
}

#define enqueue8 enqueue24

/* flush the queue and align to next byte */
static inline void flush_bits(struct slz_stream *strm)
{
	if (strm->qbits > 0)
		*strm->outbuf++ = strm->queue;

	if (strm->qbits > 8)
		*strm->outbuf++ = strm->queue >> 8;

	if (strm->qbits > 16)
		*strm->outbuf++ = strm->queue >> 16;

	if (strm->qbits > 24)
		*strm->outbuf++ = strm->queue >> 24;

	strm->queue = 0;
	strm->qbits = 0;
}

#else /* non-64 bit or aligned or big endian */

/* enqueue code x of <xbits> bits (LSB aligned, at most 24) and copy complete
 * bytes into out buf. X must not contain non-zero bits above xbits. Prefer
 * enqueue8() when xbits is known for being 8 or less.
 */
static void enqueue24(struct slz_stream *strm, uint32_t x, uint32_t xbits)
{
	uint32_t queue = strm->queue + (x << strm->qbits);
	uint32_t qbits = strm->qbits + xbits;

	if (qbits >= 16) {
#ifndef UNALIGNED_LE_OK
		strm->outbuf[0] = queue;
		strm->outbuf[1] = queue >> 8;
#else
		*(uint16_t *)strm->outbuf = queue;
#endif
		strm->outbuf += 2;
		queue >>= 16;
		qbits -= 16;
	}

	if (qbits >= 8) {
		qbits -= 8;
		*strm->outbuf++ = queue;
		queue >>= 8;
	}
	strm->qbits = qbits;
	strm->queue = queue;
	return;
}

/* enqueue code x of <xbits> bits (at most 8) and copy complete bytes into
 * out buf. X must not contain non-zero bits above xbits.
 */
static inline void enqueue8(struct slz_stream *strm, uint32_t x, uint32_t xbits)
{
	uint32_t queue = strm->queue + (x << strm->qbits);
	uint32_t qbits = strm->qbits + xbits;

	if (__builtin_expect((signed)(qbits - 8) >= 0, 1)) {
		qbits -= 8;
		*strm->outbuf++ = queue;
		queue >>= 8;
	}

	strm->qbits = qbits;
	strm->queue = queue;
}

/* align to next byte */
static inline void flush_bits(struct slz_stream *strm)
{
	if (strm->qbits > 0)
		*strm->outbuf++ = strm->queue;

	if (strm->qbits > 8)
		*strm->outbuf++ = strm->queue >> 8;

	strm->queue = 0;
	strm->qbits = 0;
}
#endif


/* only valid if buffer is already aligned */
static inline void copy_8b(struct slz_stream *strm, uint32_t x)
{
	*strm->outbuf++ = x;
}

/* only valid if buffer is already aligned */
static inline void copy_16b(struct slz_stream *strm, uint32_t x)
{
	strm->outbuf[0] = x;
	strm->outbuf[1] = x >> 8;
	strm->outbuf += 2;
}

/* only valid if buffer is already aligned */
static inline void copy_32b(struct slz_stream *strm, uint32_t x)
{
	strm->outbuf[0] = x;
	strm->outbuf[1] = x >> 8;
	strm->outbuf[2] = x >> 16;
	strm->outbuf[3] = x >> 24;
	strm->outbuf += 4;
}

/* Using long because faster on 64-bit (can save one shift) */
static inline void send_huff(struct slz_stream *strm, unsigned long code)
{
	uint32_t bits;

	code = fixed_huff[code];
	bits = code & 15;
	code >>= 4;
	enqueue24(strm, code, bits);
}

static inline void send_eob(struct slz_stream *strm)
{
	enqueue8(strm, 0, 7); // direct encoding of 256 = EOB (cf RFC1951)
}

/* copies <len> litterals from <buf>. <more> indicates that there are data past
 * buf + <len>. <len> must not be null.
 */
static void copy_lit(struct slz_stream *strm, const void *buf, uint32_t len, int more)
{
	uint32_t len2;

	do {
		len2 = len;
		if (__builtin_expect(len2 > 65535, 0))
			len2 = 65535;

		len -= len2;

		if (strm->state != SLZ_ST_EOB)
			send_eob(strm);

		strm->state = (more || len) ? SLZ_ST_EOB : SLZ_ST_DONE;

		enqueue8(strm, !(more || len), 3); // BFINAL = !more ; BTYPE = 00
		flush_bits(strm);
		copy_16b(strm, len2);  // len2
		copy_16b(strm, ~len2); // nlen2
		memcpy(strm->outbuf, buf, len2);
		buf += len2;
		strm->outbuf += len2;
	} while (len);
}

/* copies <len> litterals from <buf>. <more> indicates that there are data past
 * buf + <len>. <len> must not be null.
 */
static void copy_lit_huff(struct slz_stream *strm, const unsigned char *buf, uint32_t len, int more)
{
	uint32_t pos;

	/* This ugly construct limits the mount of tests and optimizes for the
	 * most common case (more > 0).
	 */
	if (strm->state == SLZ_ST_EOB) {
	eob:
		strm->state = more ? SLZ_ST_FIXED : SLZ_ST_LAST;
		enqueue8(strm, 2 + !more, 3); // BFINAL = !more ; BTYPE = 01
	}
	else if (!more) {
		send_eob(strm);
		goto eob;
	}

	pos = 0;
	do {
		send_huff(strm, buf[pos++]);
	} while (pos < len);
}

/* format:
 * bit0..31  = word
 * bit32..63 = last position in buffer of similar content
 */

/* This hash provides good average results on HTML contents, and is among the
 * few which provide almost optimal results on various different pages.
 */
static inline uint32_t slz_hash(uint32_t a)
{
#if defined(__ARM_FEATURE_CRC32)
#  if defined(__ARM_ARCH_ISA_A64)
	// 64 bit mode
	__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(a) : "r"(0));
#  else
	// 32 bit mode (e.g. armv7 compiler building for armv8
	__asm__ volatile("crc32w %0,%0,%1" : "+r"(a) : "r"(0));
#  endif
	return a >> (32 - HASH_BITS);
#elif defined(__SSE4_2__) && defined(USE_CRC32C_HASH)
	// SSE 4.2 offers CRC32C which is a bit slower than the multiply
	// but provides a slightly smoother hash
	__asm__ volatile("crc32l %1,%0" : "+r"(a) : "r"(0));
	return a >> (32 - HASH_BITS);
#elif defined(HAVE_FAST_MULT)
	// optimal factor for HASH_BITS=12 and HASH_BITS=13 among 48k tested: 0x1af42f
	return (a * 0x1af42f) >> (32 - HASH_BITS);
#else
	return ((a << 19) + (a << 6) - a) >> (32 - HASH_BITS);
#endif
}

/* This function compares buffers <a> and <b> and reads 32 or 64 bits at a time
 * during the approach. It makes us of unaligned little endian memory accesses
 * on capable architectures. <max> is the maximum number of bytes that can be
 * read, so both <a> and <b> must have at least <max> bytes ahead. <max> may
 * safely be null or negative if that simplifies computations in the caller.
 */
static inline long memmatch(const unsigned char *a, const unsigned char *b, long max)
{
	long len = 0;

#ifdef UNALIGNED_LE_OK
	unsigned long xor;

	while (1) {
		if ((long)(len + 2 * sizeof(long)) > max) {
			while (len < max) {
				if (a[len] != b[len])
					break;
				len++;
			}
			return len;
		}

		xor = *(long *)&a[len] ^ *(long *)&b[len];
		if (xor)
			break;
		len += sizeof(long);

		xor = *(long *)&a[len] ^ *(long *)&b[len];
		if (xor)
			break;
		len += sizeof(long);
	}

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
	/* x86 has bsf. We know that xor is non-null here */
	asm("bsf %1,%0\n" : "=r"(xor) : "0" (xor));
	return len + xor / 8;
#else
	if (sizeof(long) > 4 && !(xor & 0xffffffff)) {
		/* This code is optimized out on 32-bit archs, but we still
		 * need to shift in two passes to avoid a warning. It is
		 * properly optimized out as a single shift.
		 */
		xor >>= 16; xor >>= 16;
		if (xor & 0xffff) {
			if (xor & 0xff)
				return len + 4;
			return len + 5;
		}
		if (xor & 0xffffff)
			return len + 6;
		return len + 7;
	}

	if (xor & 0xffff) {
		if (xor & 0xff)
			return len;
		return len + 1;
	}
	if (xor & 0xffffff)
		return len + 2;
	return len + 3;
#endif // x86

#else // UNALIGNED_LE_OK
	/* This is the generic version for big endian or unaligned-incompatible
	 * architectures.
	 */
	while (len < max) {
		if (a[len] != b[len])
			break;
		len++;
	}
	return len;

#endif
}

/* sets <count> BYTES to -32769 in <refs> so that any uninitialized entry will
 * verify (pos-last-1 >= 32768) and be ignored. <count> must be a multiple of
 * 128 bytes and <refs> must be at least one count in length. It's supposed to
 * be applied to 64-bit aligned data exclusively, which makes it slightly
 * faster than the regular memset() since no alignment check is performed.
 */
static void reset_refs(union ref *refs, long count)
{
	/* avoid a shift/mask by casting to void* */
	union ref *end = (void *)refs + count;

	do {
		refs[ 0].by64 = -32769;
		refs[ 1].by64 = -32769;
		refs[ 2].by64 = -32769;
		refs[ 3].by64 = -32769;
		refs[ 4].by64 = -32769;
		refs[ 5].by64 = -32769;
		refs[ 6].by64 = -32769;
		refs[ 7].by64 = -32769;
		refs[ 8].by64 = -32769;
		refs[ 9].by64 = -32769;
		refs[10].by64 = -32769;
		refs[11].by64 = -32769;
		refs[12].by64 = -32769;
		refs[13].by64 = -32769;
		refs[14].by64 = -32769;
		refs[15].by64 = -32769;
		refs += 16;
	} while (refs < end);
}

/* Compresses <ilen> bytes from <in> into <out> according to RFC1951. The
 * output result may be up to 5 bytes larger than the input, to which 2 extra
 * bytes may be added to send the last chunk due to BFINAL+EOB encoding (10
 * bits) when <more> is not set. The caller is responsible for ensuring there
 * is enough room in the output buffer for this. The amount of output bytes is
 * returned, and no CRC is computed.
 */
long slz_rfc1951_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more)
{
	long rem = ilen;
	unsigned long pos = 0;
	unsigned long last;
	uint32_t word = 0;
	long mlen;
	uint32_t h;
	uint64_t ent;

	uint32_t plit = 0;
	uint32_t bit9 = 0;
	uint32_t dist, code;
	union ref refs[1 << HASH_BITS];

	if (!strm->level) {
		/* force to send as literals (eg to preserve CPU) */
		strm->outbuf = out;
		plit = pos = ilen;
		bit9 = 52; /* force literal dump */
		goto final_lit_dump;
	}

	reset_refs(refs, sizeof(refs));

	strm->outbuf = out;

#ifndef UNALIGNED_FASTER
	word = ((unsigned char)in[pos] << 8) + ((unsigned char)in[pos + 1] << 16) + ((unsigned char)in[pos + 2] << 24);
#endif
	while (rem >= 4) {
#ifndef UNALIGNED_FASTER
		word = ((unsigned char)in[pos + 3] << 24) + (word >> 8);
#else
		word = *(uint32_t *)&in[pos];
#endif
		h = slz_hash(word);
		asm volatile ("" ::); // prevent gcc from trying to be smart with the prefetch

		if (sizeof(long) >= 8) {
			ent = refs[h].by64;
			last = (uint32_t)ent;
			ent >>= 32;
			refs[h].by64 = ((uint64_t)pos) + ((uint64_t)word << 32);
		} else {
			ent  = refs[h].by32.word;
			last = refs[h].by32.pos;
			refs[h].by32.pos = pos;
			refs[h].by32.word = word;
		}

#ifdef FIND_OPTIMAL_MATCH
		/* Experimental code to see what could be saved with an ideal
		 * longest match lookup algorithm. This one is very slow but
		 * scans the whole window. In short, here are the savings :
		 *   file        orig     fast(ratio)  optimal(ratio)
		 *  README       5185    3419 (65.9%)    3165 (61.0%)  -7.5%
		 *  index.html  76799   35662 (46.4%)   29875 (38.9%) -16.3%
		 *  rfc1952.c   29383   13442 (45.7%)   11793 (40.1%) -12.3%
		 *
		 * Thus the savings to expect for large files is at best 16%.
		 *
		 * A non-colliding hash gives 33025 instead of 35662 (-7.4%),
		 * and keeping the last two entries gives 31724 (-11.0%).
		 */
		unsigned long scan;
		int saved = 0;
		int bestpos = 0;
		int bestlen = 0;
		int firstlen = 0;
		int max_lookup = 2; // 0 = no limit

		for (scan = pos - 1; scan < pos && (unsigned long)(pos - scan - 1) < 32768; scan--) {
			int len;

			if (*(uint32_t *)(in + scan) != word)
				continue;

			len = memmatch(in + pos, in + scan, rem);
			if (!bestlen)
				firstlen = len;

			if (len > bestlen) {
				bestlen = len;
				bestpos = scan;
			}
			if (!--max_lookup)
				break;
		}
		if (bestlen) {
			//printf("pos=%d last=%d bestpos=%d word=%08x ent=%08x len=%d\n",
			//       (int)pos, (int)last, (int)bestpos, (int)word, (int)ent, bestlen);
			last = bestpos;
			ent  = word;
			saved += bestlen - firstlen;
		}
		//fprintf(stderr, "first=%d best=%d saved_total=%d\n", firstlen, bestlen, saved);
#endif

		if ((uint32_t)ent != word) {
		send_as_lit:
			rem--;
			plit++;
			bit9 += ((unsigned char)word >= 144);
			pos++;
			continue;
		}

		/* We reject pos = last and pos > last+32768 */
		if ((unsigned long)(pos - last - 1) >= 32768)
			goto send_as_lit;

		/* Note: cannot encode a length larger than 258 bytes */
		mlen = memmatch(in + pos + 4, in + last + 4, (rem > 258 ? 258 : rem) - 4) + 4;

		/* found a matching entry */

		if (bit9 >= 52 && mlen < 6)
			goto send_as_lit;

		/* compute the output code, its size and the length's size in
		 * bits to know if the reference is cheaper than literals.
		 */
		code = len_fh[mlen];

		/* direct mapping of dist->huffman code */
		dist = fh_dist_table[pos - last - 1];

		/* if encoding the dist+length is more expensive than sending
		 * the equivalent as bytes, lets keep the literals.
		 */
		if ((dist & 0x1f) + (code >> 16) + 8 >= 8 * mlen + bit9)
			goto send_as_lit;

		/* first, copy pending literals */
		if (plit) {
			/* Huffman encoding requires 9 bits for octets 144..255, so this
			 * is a waste of space for binary data. Switching between Huffman
			 * and no-comp then huffman consumes 52 bits (7 for EOB + 3 for
			 * block type + 7 for alignment + 32 for LEN+NLEN + 3 for next
			 * block. Only use plain literals if there are more than 52 bits
			 * to save then.
			 */
			if (bit9 >= 52)
				copy_lit(strm, in + pos - plit, plit, 1);
			else
				copy_lit_huff(strm, in + pos - plit, plit, 1);

			plit = 0;
		}

		/* use mode 01 - fixed huffman */
		if (strm->state == SLZ_ST_EOB) {
			strm->state = SLZ_ST_FIXED;
			enqueue8(strm, 0x02, 3); // BTYPE = 01, BFINAL = 0
		}

		/* copy the length first */
		enqueue24(strm, code & 0xFFFF, code >> 16);

		/* in fixed huffman mode, dist is fixed 5 bits */
		enqueue24(strm, dist >> 5, dist & 0x1f);
		bit9 = 0;
		rem -= mlen;
		pos += mlen;

#ifndef UNALIGNED_FASTER
#ifdef UNALIGNED_LE_OK
		word = *(uint32_t *)&in[pos - 1];
#else
		word = ((unsigned char)in[pos] << 8) + ((unsigned char)in[pos + 1] << 16) + ((unsigned char)in[pos + 2] << 24);
#endif
#endif
	}

	if (__builtin_expect(rem, 0)) {
		/* we're reading the 1..3 last bytes */
		plit += rem;
		do {
			bit9 += ((unsigned char)in[pos++] >= 144);
		} while (--rem);
	}

 final_lit_dump:
	/* now copy remaining literals or mark the end */
	if (plit) {
		if (bit9 >= 52)
			copy_lit(strm, in + pos - plit, plit, more);
		else
			copy_lit_huff(strm, in + pos - plit, plit, more);

		plit = 0;
	}

	strm->ilen += ilen;
	return strm->outbuf - out;
}

/* Initializes stream <strm> for use with raw deflate (rfc1951). The CRC is
 * unused but set to zero. The compression level passed in <level> is set. This
 * value can only be 0 (no compression) or 1 (compression) and other values
 * will lead to unpredictable behaviour. The function always returns 0.
 */
int slz_rfc1951_init(struct slz_stream *strm, int level)
{
	strm->state = SLZ_ST_EOB; // no header
	strm->level = level;
	strm->format = SLZ_FMT_DEFLATE;
	strm->crc32 = 0;
	strm->ilen  = 0;
	strm->qbits = 0;
	strm->queue = 0;
	return 0;
}

/* Flushes any pending data for stream <strm> into buffer <buf>, then emits an
 * empty literal block to byte-align the output, allowing to completely flush
 * the queue. This requires that the output buffer still has the size of the
 * queue available (up to 4 bytes), plus one byte for (BFINAL,BTYPE), plus 4
 * bytes for LEN+NLEN, or a total of 9 bytes in the worst case. The number of
 * bytes emitted is returned. It is guaranteed that the queue is empty on
 * return. This may cause some overhead by adding needless 5-byte blocks if
 * called to often.
 */
int slz_rfc1951_flush(struct slz_stream *strm, unsigned char *buf)
{
	strm->outbuf = buf;

	/* The queue is always empty on INIT, DONE, and END */
	if (!strm->qbits)
		return 0;

	/* we may need to terminate a huffman output. Lit is always in EOB state */
	if (strm->state != SLZ_ST_EOB) {
		strm->state = (strm->state == SLZ_ST_LAST) ? SLZ_ST_DONE : SLZ_ST_EOB;
		send_eob(strm);
	}

	/* send BFINAL according to state, and BTYPE=00 (lit) */
	enqueue8(strm, (strm->state == SLZ_ST_DONE) ? 1 : 0, 3);
	flush_bits(strm);             // emit pending bits
	copy_32b(strm, 0xFFFF0000U);  // len=0, nlen=~0

	/* Now the queue is empty, EOB was sent, BFINAL might have been sent if
	 * we completed the last block, and a zero-byte block was sent to byte-
	 * align the output. The last state reflects all this. Let's just
	 * return the number of bytes added to the output buffer.
	 */
	return strm->outbuf - buf;
}

/* Flushes any pending for stream <strm> into buffer <buf>, then sends BTYPE=1
 * and BFINAL=1 if needed. The stream ends in SLZ_ST_DONE. It returns the number
 * of bytes emitted. The trailer consists in flushing the possibly pending bits
 * from the queue (up to 7 bits), then possibly EOB (7 bits), then 3 bits, EOB,
 * a rounding to the next byte, which amounts to a total of 4 bytes max, that
 * the caller must ensure are available before calling the function.
 */
int slz_rfc1951_finish(struct slz_stream *strm, unsigned char *buf)
{
	strm->outbuf = buf;

	if (strm->state == SLZ_ST_FIXED || strm->state == SLZ_ST_LAST) {
		strm->state = (strm->state == SLZ_ST_LAST) ? SLZ_ST_DONE : SLZ_ST_EOB;
		send_eob(strm);
	}

	if (strm->state != SLZ_ST_DONE) {
		/* send BTYPE=1, BFINAL=1 */
		enqueue8(strm, 3, 3);
		send_eob(strm);
		strm->state = SLZ_ST_DONE;
	}

	flush_bits(strm);
	return strm->outbuf - buf;
}

/* Now RFC1952-specific declarations and extracts from RFC.
 * From RFC1952 about the GZIP file format :

A gzip file consists of a series of "members" ...

2.3. Member format

      Each member has the following structure:

         +---+---+---+---+---+---+---+---+---+---+
         |ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
         +---+---+---+---+---+---+---+---+---+---+

      (if FLG.FEXTRA set)

         +---+---+=================================+
         | XLEN  |...XLEN bytes of "extra field"...| (more-->)
         +---+---+=================================+

      (if FLG.FNAME set)

         +=========================================+
         |...original file name, zero-terminated...| (more-->)
         +=========================================+

      (if FLG.FCOMMENT set)

         +===================================+
         |...file comment, zero-terminated...| (more-->)
         +===================================+

      (if FLG.FHCRC set)

         +---+---+
         | CRC16 |
         +---+---+

         +=======================+
         |...compressed blocks...| (more-->)
         +=======================+

           0   1   2   3   4   5   6   7
         +---+---+---+---+---+---+---+---+
         |     CRC32     |     ISIZE     |
         +---+---+---+---+---+---+---+---+


2.3.1. Member header and trailer

         ID1 (IDentification 1)
         ID2 (IDentification 2)
            These have the fixed values ID1 = 31 (0x1f, \037), ID2 = 139
            (0x8b, \213), to identify the file as being in gzip format.

         CM (Compression Method)
            This identifies the compression method used in the file.  CM
            = 0-7 are reserved.  CM = 8 denotes the "deflate"
            compression method, which is the one customarily used by
            gzip and which is documented elsewhere.

         FLG (FLaGs)
            This flag byte is divided into individual bits as follows:

               bit 0   FTEXT
               bit 1   FHCRC
               bit 2   FEXTRA
               bit 3   FNAME
               bit 4   FCOMMENT
               bit 5   reserved
               bit 6   reserved
               bit 7   reserved

            Reserved FLG bits must be zero.

         MTIME (Modification TIME)
            This gives the most recent modification time of the original
            file being compressed.  The time is in Unix format, i.e.,
            seconds since 00:00:00 GMT, Jan.  1, 1970.  (Note that this
            may cause problems for MS-DOS and other systems that use
            local rather than Universal time.)  If the compressed data
            did not come from a file, MTIME is set to the time at which
            compression started.  MTIME = 0 means no time stamp is
            available.

         XFL (eXtra FLags)
            These flags are available for use by specific compression
            methods.  The "deflate" method (CM = 8) sets these flags as
            follows:

               XFL = 2 - compressor used maximum compression,
                         slowest algorithm
               XFL = 4 - compressor used fastest algorithm

         OS (Operating System)
            This identifies the type of file system on which compression
            took place.  This may be useful in determining end-of-line
            convention for text files.  The currently defined values are
            as follows:

                 0 - FAT filesystem (MS-DOS, OS/2, NT/Win32)
                 1 - Amiga
                 2 - VMS (or OpenVMS)
                 3 - Unix
                 4 - VM/CMS
                 5 - Atari TOS
                 6 - HPFS filesystem (OS/2, NT)
                 7 - Macintosh
                 8 - Z-System
                 9 - CP/M
                10 - TOPS-20
                11 - NTFS filesystem (NT)
                12 - QDOS
                13 - Acorn RISCOS
               255 - unknown

 ==> A file compressed using "gzip -1" on Unix-like systems can be :

        1F 8B 08 00  00 00 00 00  04 03
        <deflate-compressed stream>
        crc32 size32
*/

static const unsigned char gzip_hdr[] = { 0x1F, 0x8B,   // ID1, ID2
                                          0x08, 0x00,   // Deflate, flags (none)
                                          0x00, 0x00, 0x00, 0x00, // mtime: none
                                          0x04, 0x03 }; // fastest comp, OS=Unix

static inline uint32_t crc32_char(uint32_t crc, uint8_t x)
{
#if defined(__ARM_FEATURE_CRC32)
	crc = ~crc;
#  if defined(__ARM_ARCH_ISA_A64)
	// 64 bit mode
	__asm__ volatile("crc32b %w0,%w0,%w1" : "+r"(crc) : "r"(x));
#  else
	// 32 bit mode (e.g. armv7 compiler building for armv8
	__asm__ volatile("crc32b %0,%0,%1" : "+r"(crc) : "r"(x));
#  endif
	crc = ~crc;
#else
	crc = crc32_fast[0][(crc ^ x) & 0xff] ^ (crc >> 8);
#endif
	return crc;
}

static inline uint32_t crc32_uint32(uint32_t data)
{
#if defined(__ARM_FEATURE_CRC32)
#  if defined(__ARM_ARCH_ISA_A64)
	// 64 bit mode
	__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(data) : "r"(~0UL));
#  else
	// 32 bit mode (e.g. armv7 compiler building for armv8
	__asm__ volatile("crc32w %0,%0,%1" : "+r"(data) : "r"(~0UL));
#  endif
	data = ~data;
#else
	data = crc32_fast[3][(data >>  0) & 0xff] ^
	       crc32_fast[2][(data >>  8) & 0xff] ^
	       crc32_fast[1][(data >> 16) & 0xff] ^
	       crc32_fast[0][(data >> 24) & 0xff];
#endif
	return data;
}

/* Modified version originally from RFC1952, working with non-inverting CRCs */
uint32_t slz_crc32_by1(uint32_t crc, const unsigned char *buf, int len)
{
	int n;

	for (n = 0; n < len; n++)
		crc = crc32_char(crc, buf[n]);
	return crc;
}

/* This version computes the crc32 of <buf> over <len> bytes, doing most of it
 * in 32-bit chunks.
 */
uint32_t slz_crc32_by4(uint32_t crc, const unsigned char *buf, int len)
{
	const unsigned char *end = buf + len;

	while (buf <= end - 16) {
#ifdef UNALIGNED_LE_OK
#if defined(__ARM_FEATURE_CRC32)
		crc = ~crc;
#  if defined(__ARM_ARCH_ISA_A64)
	// 64 bit mode
		__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(crc) : "r"(*(uint32_t*)(buf)));
		__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 4)));
		__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 8)));
		__asm__ volatile("crc32w %w0,%w0,%w1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 12)));
#  else
	// 32 bit mode (e.g. armv7 compiler building for armv8
		__asm__ volatile("crc32w %0,%0,%1" : "+r"(crc) : "r"(*(uint32_t*)(buf)));
		__asm__ volatile("crc32w %0,%0,%1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 4)));
		__asm__ volatile("crc32w %0,%0,%1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 8)));
		__asm__ volatile("crc32w %0,%0,%1" : "+r"(crc) : "r"(*(uint32_t*)(buf + 12)));
#  endif
		crc = ~crc;
#else
		crc ^= *(uint32_t *)buf;
		crc = crc32_uint32(crc);

		crc ^= *(uint32_t *)(buf + 4);
		crc = crc32_uint32(crc);

		crc ^= *(uint32_t *)(buf + 8);
		crc = crc32_uint32(crc);

		crc ^= *(uint32_t *)(buf + 12);
		crc = crc32_uint32(crc);
#endif
#else
		crc = crc32_fast[3][(buf[0] ^ (crc >>  0)) & 0xff] ^
		      crc32_fast[2][(buf[1] ^ (crc >>  8)) & 0xff] ^
		      crc32_fast[1][(buf[2] ^ (crc >> 16)) & 0xff] ^
		      crc32_fast[0][(buf[3] ^ (crc >> 24)) & 0xff];

		crc = crc32_fast[3][(buf[4] ^ (crc >>  0)) & 0xff] ^
		      crc32_fast[2][(buf[5] ^ (crc >>  8)) & 0xff] ^
		      crc32_fast[1][(buf[6] ^ (crc >> 16)) & 0xff] ^
		      crc32_fast[0][(buf[7] ^ (crc >> 24)) & 0xff];

		crc = crc32_fast[3][(buf[8] ^ (crc >>  0)) & 0xff] ^
		      crc32_fast[2][(buf[9] ^ (crc >>  8)) & 0xff] ^
		      crc32_fast[1][(buf[10] ^ (crc >> 16)) & 0xff] ^
		      crc32_fast[0][(buf[11] ^ (crc >> 24)) & 0xff];

		crc = crc32_fast[3][(buf[12] ^ (crc >>  0)) & 0xff] ^
		      crc32_fast[2][(buf[13] ^ (crc >>  8)) & 0xff] ^
		      crc32_fast[1][(buf[14] ^ (crc >> 16)) & 0xff] ^
		      crc32_fast[0][(buf[15] ^ (crc >> 24)) & 0xff];
#endif
		buf += 16;
	}

	while (buf <= end - 4) {
#ifdef UNALIGNED_LE_OK
		crc ^= *(uint32_t *)buf;
		crc = crc32_uint32(crc);
#else
		crc = crc32_fast[3][(buf[0] ^ (crc >>  0)) & 0xff] ^
		      crc32_fast[2][(buf[1] ^ (crc >>  8)) & 0xff] ^
		      crc32_fast[1][(buf[2] ^ (crc >> 16)) & 0xff] ^
		      crc32_fast[0][(buf[3] ^ (crc >> 24)) & 0xff];
#endif
		buf += 4;
	}

	while (buf < end)
		crc = crc32_char(crc, *buf++);
	return crc;
}

/* uses the most suitable crc32 function to update crc on <buf, len> */
static inline uint32_t update_crc(uint32_t crc, const void *buf, int len)
{
	return slz_crc32_by4(crc, buf, len);
}

/* Sends the gzip header for stream <strm> into buffer <buf>. When it's done,
 * the stream state is updated to SLZ_ST_EOB. It returns the number of bytes
 * emitted which is always 10. The caller is responsible for ensuring there's
 * always enough room in the buffer.
 */
int slz_rfc1952_send_header(struct slz_stream *strm, unsigned char *buf)
{
	memcpy(buf, gzip_hdr, sizeof(gzip_hdr));
	strm->state = SLZ_ST_EOB;
	return sizeof(gzip_hdr);
}

/* Encodes the block according to rfc1952. This means that the CRC of the input
 * block is computed according to the CRC32 algorithm. If the header was never
 * sent, it may be sent first. The number of output bytes is returned.
 */
long slz_rfc1952_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more)
{
	long ret = 0;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		ret += slz_rfc1952_send_header(strm, out);

	strm->crc32 = update_crc(strm->crc32, in, ilen);
	ret += slz_rfc1951_encode(strm, out + ret, in, ilen, more);
	return ret;
}

/* Initializes stream <strm> for use with the gzip format (rfc1952). The
 * compression level passed in <level> is set. This value can only be 0 (no
 * compression) or 1 (compression) and other values will lead to unpredictable
 * behaviour. The function always returns 0.
 */
int slz_rfc1952_init(struct slz_stream *strm, int level)
{
	strm->state  = SLZ_ST_INIT;
	strm->level  = level;
	strm->format = SLZ_FMT_GZIP;
	strm->crc32  = 0;
	strm->ilen   = 0;
	strm->qbits  = 0;
	strm->queue  = 0;
	return 0;
}

/* Flushes any pending data for stream <strm> into buffer <buf>, then emits an
 * empty literal block to byte-align the output, allowing to completely flush
 * the queue. Note that if the initial header was never sent, it will be sent
 * first as well (10 extra bytes). This requires that the output buffer still
 * has this plus the size of the queue available (up to 4 bytes), plus one byte
 * for (BFINAL,BTYPE), plus 4 bytes for LEN+NLEN, or a total of 19 bytes in the
 * worst case. The number of bytes emitted is returned. It is guaranteed that
 * the queue is empty on return. This may cause some overhead by adding
 * needless 5-byte blocks if called to often.
 */
int slz_rfc1952_flush(struct slz_stream *strm, unsigned char *buf)
{
	int sent = 0;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		sent = slz_rfc1952_send_header(strm, buf);

	sent += slz_rfc1951_flush(strm, buf + sent);
	return sent;
}

/* Flushes pending bits and sends the gzip trailer for stream <strm> into
 * buffer <buf>. When it's done, the stream state is updated to SLZ_ST_END. It
 * returns the number of bytes emitted. The trailer consists in flushing the
 * possibly pending bits from the queue (up to 24 bits), rounding to the next
 * byte, then 4 bytes for the CRC and another 4 bytes for the input length.
 * That may amount to 4+4+4 = 12 bytes, that the caller must ensure are
 * available before calling the function. Note that if the initial header was
 * never sent, it will be sent first as well (10 extra bytes).
 */
int slz_rfc1952_finish(struct slz_stream *strm, unsigned char *buf)
{
	strm->outbuf = buf;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		strm->outbuf += slz_rfc1952_send_header(strm, strm->outbuf);

	slz_rfc1951_finish(strm, strm->outbuf);
	copy_32b(strm, strm->crc32);
	copy_32b(strm, strm->ilen);
	strm->state = SLZ_ST_END;

	return strm->outbuf - buf;
}


/* RFC1950-specific stuff. This is for the Zlib stream format.
 * From RFC1950 (zlib) :
 *

   2.2. Data format

      A zlib stream has the following structure:

           0   1
         +---+---+
         |CMF|FLG|   (more-->)
         +---+---+


      (if FLG.FDICT set)

           0   1   2   3
         +---+---+---+---+
         |     DICTID    |   (more-->)
         +---+---+---+---+

         +=====================+---+---+---+---+
         |...compressed data...|    ADLER32    |
         +=====================+---+---+---+---+

      Any data which may appear after ADLER32 are not part of the zlib
      stream.

      CMF (Compression Method and flags)
         This byte is divided into a 4-bit compression method and a 4-
         bit information field depending on the compression method.

            bits 0 to 3  CM     Compression method
            bits 4 to 7  CINFO  Compression info

      CM (Compression method)
         This identifies the compression method used in the file. CM = 8
         denotes the "deflate" compression method with a window size up
         to 32K.  This is the method used by gzip and PNG (see
         references [1] and [2] in Chapter 3, below, for the reference
         documents).  CM = 15 is reserved.  It might be used in a future
         version of this specification to indicate the presence of an
         extra field before the compressed data.

      CINFO (Compression info)
         For CM = 8, CINFO is the base-2 logarithm of the LZ77 window
         size, minus eight (CINFO=7 indicates a 32K window size). Values
         of CINFO above 7 are not allowed in this version of the
         specification.  CINFO is not defined in this specification for
         CM not equal to 8.

      FLG (FLaGs)
         This flag byte is divided as follows:

            bits 0 to 4  FCHECK  (check bits for CMF and FLG)
            bit  5       FDICT   (preset dictionary)
            bits 6 to 7  FLEVEL  (compression level)

         The FCHECK value must be such that CMF and FLG, when viewed as
         a 16-bit unsigned integer stored in MSB order (CMF*256 + FLG),
         is a multiple of 31.


      FDICT (Preset dictionary)
         If FDICT is set, a DICT dictionary identifier is present
         immediately after the FLG byte. The dictionary is a sequence of
         bytes which are initially fed to the compressor without
         producing any compressed output. DICT is the Adler-32 checksum
         of this sequence of bytes (see the definition of ADLER32
         below).  The decompressor can use this identifier to determine
         which dictionary has been used by the compressor.

      FLEVEL (Compression level)
         These flags are available for use by specific compression
         methods.  The "deflate" method (CM = 8) sets these flags as
         follows:

            0 - compressor used fastest algorithm
            1 - compressor used fast algorithm
            2 - compressor used default algorithm
            3 - compressor used maximum compression, slowest algorithm

         The information in FLEVEL is not needed for decompression; it
         is there to indicate if recompression might be worthwhile.

      compressed data
         For compression method 8, the compressed data is stored in the
         deflate compressed data format as described in the document
         "DEFLATE Compressed Data Format Specification" by L. Peter
         Deutsch. (See reference [3] in Chapter 3, below)

         Other compressed data formats are not specified in this version
         of the zlib specification.

      ADLER32 (Adler-32 checksum)
         This contains a checksum value of the uncompressed data
         (excluding any dictionary data) computed according to Adler-32
         algorithm. This algorithm is a 32-bit extension and improvement
         of the Fletcher algorithm, used in the ITU-T X.224 / ISO 8073
         standard. See references [4] and [5] in Chapter 3, below)

         Adler-32 is composed of two sums accumulated per byte: s1 is
         the sum of all bytes, s2 is the sum of all s1 values. Both sums
         are done modulo 65521. s1 is initialized to 1, s2 to zero.  The
         Adler-32 checksum is stored as s2*65536 + s1 in most-
         significant-byte first (network) order.

  ==> The stream can start with only 2 bytes :
        - CM  = 0x78 : CMINFO=7 (32kB window),  CM=8 (deflate)
        - FLG = 0x01 : FLEVEL = 0 (fastest), FDICT=0 (no dict), FCHECK=1 so
          that 0x7801 is a multiple of 31 (30721 = 991 * 31).

  ==> and it ends with only 4 bytes, the Adler-32 checksum in big-endian format.

 */

static const unsigned char zlib_hdr[] = { 0x78, 0x01 };   // 32k win, deflate, chk=1


/* Original version from RFC1950, verified and works OK */
uint32_t slz_adler32_by1(uint32_t crc, const unsigned char *buf, int len)
{
	uint32_t s1 = crc & 0xffff;
	uint32_t s2 = (crc >> 16) & 0xffff;
	int n;

	for (n = 0; n < len; n++) {
		s1 = (s1 + buf[n]) % 65521;
		s2 = (s2 + s1)     % 65521;
	}
	return (s2 << 16) + s1;
}

/* Computes the adler32 sum on <buf> for <len> bytes. It avoids the expensive
 * modulus by retrofitting the number of bytes missed between 65521 and 65536
 * which is easy to count : For every sum above 65536, the modulus is offset
 * by (65536-65521) = 15. So for any value, we can count the accumulated extra
 * values by dividing the sum by 65536 and multiplying this value by
 * (65536-65521). That's easier with a drawing with boxes and marbles. It gives
 * this :
 *          x % 65521 = (x % 65536) + (x / 65536) * (65536 - 65521)
 *                    = (x & 0xffff) + (x >> 16) * 15.
 */
uint32_t slz_adler32_block(uint32_t crc, const unsigned char *buf, long len)
{
	long s1 = crc & 0xffff;
	long s2 = (crc >> 16);
	long blk;
	long n;

	do {
		blk = len;
		/* ensure we never overflow s2 (limit is about 2^((32-8)/2) */
		if (blk > (1U << 12))
			blk = 1U << 12;
		len -= blk;

		for (n = 0; n < blk; n++) {
			s1 = (s1 + buf[n]);
			s2 = (s2 + s1);
		}

		/* Largest value here is 2^12 * 255 = 1044480 < 2^20. We can
		 * still overflow once, but not twice because the right hand
		 * size is 225 max, so the total is 65761. However we also
		 * have to take care of the values between 65521 and 65536.
		 */
		s1 = (s1 & 0xffff) + 15 * (s1 >> 16);
		if (s1 >= 65521)
			s1 -= 65521;

		/* For s2, the largest value is estimated to 2^32-1 for
		 * simplicity, so the right hand side is about 15*65535
		 * = 983025. We can overflow twice at most.
		 */
		s2 = (s2 & 0xffff) + 15 * (s2 >> 16);
		s2 = (s2 & 0xffff) + 15 * (s2 >> 16);
		if (s2 >= 65521)
			s2 -= 65521;

		buf += blk;
	} while (len);
	return (s2 << 16) + s1;
}

/* Sends the zlib header for stream <strm> into buffer <buf>. When it's done,
 * the stream state is updated to SLZ_ST_EOB. It returns the number of bytes
 * emitted which is always 2. The caller is responsible for ensuring there's
 * always enough room in the buffer.
 */
int slz_rfc1950_send_header(struct slz_stream *strm, unsigned char *buf)
{
	memcpy(buf, zlib_hdr, sizeof(zlib_hdr));
	strm->state = SLZ_ST_EOB;
	return sizeof(zlib_hdr);
}

/* Encodes the block according to rfc1950. This means that the CRC of the input
 * block is computed according to the ADLER32 algorithm. If the header was never
 * sent, it may be sent first. The number of output bytes is returned.
 */
long slz_rfc1950_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more)
{
	long ret = 0;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		ret += slz_rfc1950_send_header(strm, out);

	strm->crc32 = slz_adler32_block(strm->crc32, in, ilen);
	ret += slz_rfc1951_encode(strm, out + ret, in, ilen, more);
	return ret;
}

/* Initializes stream <strm> for use with the zlib format (rfc1950). The
 * compression level passed in <level> is set. This value can only be 0 (no
 * compression) or 1 (compression) and other values will lead to unpredictable
 * behaviour. The function always returns 0.
 */
int slz_rfc1950_init(struct slz_stream *strm, int level)
{
	strm->state  = SLZ_ST_INIT;
	strm->level  = level;
	strm->format = SLZ_FMT_ZLIB;
	strm->crc32  = 1; // rfc1950/zlib starts with initial crc=1
	strm->ilen   = 0;
	strm->qbits  = 0;
	strm->queue  = 0;
	return 0;
}

/* Flushes any pending data for stream <strm> into buffer <buf>, then emits an
 * empty literal block to byte-align the output, allowing to completely flush
 * the queue. Note that if the initial header was never sent, it will be sent
 * first as well (2 extra bytes). This requires that the output buffer still
 * has this plus the size of the queue available (up to 4 bytes), plus one byte
 * for (BFINAL,BTYPE), plus 4 bytes for LEN+NLEN, or a total of 11 bytes in the
 * worst case. The number of bytes emitted is returned. It is guaranteed that
 * the queue is empty on return. This may cause some overhead by adding
 * needless 5-byte blocks if called to often.
 */
int slz_rfc1950_flush(struct slz_stream *strm, unsigned char *buf)
{
	int sent = 0;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		sent = slz_rfc1950_send_header(strm, buf);

	sent += slz_rfc1951_flush(strm, buf + sent);
	return sent;
}

/* Flushes pending bits and sends the gzip trailer for stream <strm> into
 * buffer <buf>. When it's done, the stream state is updated to SLZ_ST_END. It
 * returns the number of bytes emitted. The trailer consists in flushing the
 * possibly pending bits from the queue (up to 24 bits), rounding to the next
 * byte, then 4 bytes for the CRC. That may amount to 4+4 = 8 bytes, that the
 * caller must ensure are available before calling the function. Note that if
 * the initial header was never sent, it will be sent first as well (2 extra
 * bytes).
 */
int slz_rfc1950_finish(struct slz_stream *strm, unsigned char *buf)
{
	strm->outbuf = buf;

	if (__builtin_expect(strm->state == SLZ_ST_INIT, 0))
		strm->outbuf += slz_rfc1950_send_header(strm, strm->outbuf);

	slz_rfc1951_finish(strm, strm->outbuf);
	copy_8b(strm, (strm->crc32 >> 24) & 0xff);
	copy_8b(strm, (strm->crc32 >> 16) & 0xff);
	copy_8b(strm, (strm->crc32 >>  8) & 0xff);
	copy_8b(strm, (strm->crc32 >>  0) & 0xff);
	strm->state = SLZ_ST_END;
	return strm->outbuf - buf;
}

/* This used to be the function called to build the CRC table at init time.
 * Now it does nothing, it's only kept for API/ABI compatibility.
 */
void slz_make_crc_table(void)
{
}

/* does nothing anymore, only kept for ABI compatibility */
void slz_prepare_dist_table()
{
}

__attribute__((constructor))
static void __slz_initialize(void)
{
#if !defined(__ARM_FEATURE_CRC32)
	__slz_make_crc_table();
#endif
	__slz_prepare_dist_table();
}
