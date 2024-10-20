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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t queue;
static uint32_t qbits; /* number of bits in queue, < 16 */
static int pos;

static const int base_len[] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
	15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
	67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const int base_dist[] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
	33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
	1025, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

/* ensures we always have at least 16 bits in the queue or that end is reached.
 * It returns 0 when end is reached and last bit was read.
 */
int refill_queue()
{
	int c;

	if (qbits >= 16)
		return 1;

	do {
		c = getchar();
		if (c == EOF)
			return qbits ? 1 : 0;
		queue += (uint32_t)c << qbits;
		qbits += 8;
		pos++;
	} while (qbits < 16);
	return 1;
}

/* returns position in the stream of the current queue being read */
int get_pos()
{
	return pos - 1 - (qbits / 8);
}

int get_bit()
{
	return 8 - (qbits & 7);
}

int swap_bits(uint32_t x, int bits)
{
	uint32_t y = 0;

	while (bits) {
		y <<= 1;
		y += x & 1;
		x >>= 1;
		bits--;
	}
	return y;
}

void advance_queue(int bits)
{
	queue >>= bits;
	qbits -= bits;
}

void next_boundary()
{
	if (!refill_queue())
		return;

	advance_queue(qbits & 7);
}

/* reads next code, returns the code or -1 when end is reached */
int get_huff_code()
{
	int code = 0;

	if (!refill_queue())
		return -1;

	switch (swap_bits(queue & 0x3F, 6)) {
	case 0x00 ... 0x0B: /* 7 bits, 256-279 */
		code = swap_bits(queue, 7) - 0x00 + 256;
		advance_queue(7);
		break;
	case 0x0C ... 0x2F: /* 8 bits, 0-143 */
		code = swap_bits(queue, 8) - 0x30 + 0;
		advance_queue(8);
		break;
	case 0x30 ... 0x31: /* 8 bits, 280-287 */
		code = swap_bits(queue, 8) - 0xc0 + 280;
		advance_queue(8);
		break;
	case 0x32 ... 0x3F: /* 9 bits, 144-255 */
		code = swap_bits(queue, 9) - 0x190 + 144;
		advance_queue(9);
		break;
	}
	return code;
}

/* returns -1 if eof, 1 if BFINAL is set, otherwise 0 */
int get_bfinal()
{
	int ret = queue & 1;

	if (!refill_queue())
		return -1;

	advance_queue(1);
	return ret;
}

/* returns -1 if eof, 0..3 for BTYPE */
int get_btype()
{
	int ret = queue & 3;

	if (!refill_queue())
		return -1;

	advance_queue(2);
	return ret;
}

/* returns -1 if eof, or >0 after reading <bits> bits */
int get_bits(int bits)
{
	int ret;

	if (!refill_queue())
		return -1;

	ret = queue & ((1 << bits) - 1);
	advance_queue(bits);
	return ret;
}

int get_len(int code, int extra)
{
	if (code < 257 || code > 285)
		return 0;
	return base_len[code - 257] + extra;
}

int get_dist(int code, int extra)
{
	if (code < 0 || code > 29)
		return 0;
	return base_dist[code] + extra;
}

int main(void)
{
	int bfinal = 0;
	int btype = 0;
	int code = 0;
	int ofs = 0;

	getchar(); // ID1
	getchar(); // ID2
	getchar(); // CM
	getchar(); // FLG
	getchar(); getchar(); getchar(); getchar(); // MTIME
	getchar(); // XFL
	getchar(); // OS

	while (!bfinal) {
		printf("@0x%04x_%d: ", get_pos(), get_bit());
		bfinal = get_bfinal();
		btype = get_btype();
		printf("BTYPE = %1x BFINAL = %d\n", btype, bfinal);

		switch (btype) {
		case 0: /* no compression : <boundary> LEN16 | NLEN16 | LEN bytes */
			printf("  no compression <not implemented>\n");
			exit(1);
			break;
		case 1:
			printf("  compressed with huffman\n");
			while (1) {
				printf("    @z: 0x%04x_%d @raw: %d [0x%06x]: ", get_pos(), get_bit(), ofs, queue);
				code = get_huff_code();
				printf("code = %3d", code);
				if (code < 0) {
					putchar('\n');
					exit(1);
				}

				if (code == 256) {
					printf("=> EOB\n");
					break;
				}

				if (code > 256) {
					int xbits;
					int data;
					int len;

					xbits = ((code - 257) / 4) - 1;
					if (xbits < 0)
						xbits = 0;
					else if (xbits > 5)
						xbits = 0;
					data = get_bits(xbits);
					len = get_len(code, data);
					printf (" => ref: xbits=%d [%d] len=%d", xbits, data, len);
					ofs += len;

					code = get_bits(5);  // 5 bits
					code = swap_bits(code, 5); // distance is encoded with bits swapped

					xbits = (code / 2) - 1;
					if (xbits < 0)
						xbits = 0;
					data = get_bits(xbits);
					printf (" code=%d xbits=%d [%d] dist=%d\n", code, xbits, data, get_dist(code, data));
				}
				else {
					ofs++;
					printf(" (0x%03x)\n", code);
				}
			}
			break;
		case 2:
			printf("  compressed with dynamic huffman <not implemented>\n");
			exit(1);
			break;
		case 3:
			printf("  reserved <not implemented>\n");
			exit(1);
			break;
		}
	}
	return 0;
}
