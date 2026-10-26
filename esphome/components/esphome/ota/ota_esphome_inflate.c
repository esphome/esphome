/*
 * uzlib  -  tiny deflate/inflate library (deflate, gzip, zlib)
 *
 * Copyright (c) 2003 by Joergen Ibsen / Jibz
 * All Rights Reserved
 * http://www.ibsensoftware.com/
 *
 * Copyright (c) 2014-2018 by Paul Sokolovsky
 *
 * This software is provided 'as-is', without any express
 * or implied warranty.  In no event will the authors be
 * held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be
 *    misrepresented; you must not claim that you
 *    wrote the original software. If you use this
 *    software in a product, an acknowledgment in
 *    the product documentation would be appreciated
 *    but is not required.
 *
 * 2. Altered source versions must be plainly marked
 *    as such, and must not be misrepresented as
 *    being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */

/*
 * Altered for ESPHome: this is the raw deflate decoder from uzlib's
 * tinflate.c (v2.9.5) with the gzip/zlib header parsers, checksums,
 * runtime table builder and in-memory (non ring window) output path
 * removed, and the public names prefixed with ota_inflate. Symbols are
 * decoded from a 32 bit bit buffer through 9 bit lookup tables, with a
 * fast loop for when a whole symbol fits in both the input and the output,
 * and the output is written straight into the ring window.
 */

#include "ota_esphome_inflate.h"

#include <stddef.h>

#define TINF_OK OTA_INFLATE_OK
#define TINF_DONE OTA_INFLATE_DONE
#define TINF_DATA_ERROR OTA_INFLATE_DATA_ERROR
#define TINF_DICT_ERROR OTA_INFLATE_DICT_ERROR
#define TINF_DATA struct OtaInflateState
#define TINF_TREE struct OtaInflateTree
#define TINF_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

/* the output cursor is the ring position */
#define TINF_PUT(d, c) (*d->dest++ = (c))

/* --------------------------------------------------- *
 * -- constant tables (upstream builds them at runtime) -- *
 * --------------------------------------------------- */

static const unsigned char LENGTH_BITS[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
                                              2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
static const unsigned short LENGTH_BASE[30] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                               31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};

static const unsigned char DIST_BITS[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const unsigned short DIST_BASE[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                             33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                             1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

/* special ordering of code length codes */
static const unsigned char CLCIDX[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* ----------------------- *
 * -- utility functions -- *
 * ----------------------- */

#define FAST_BITS OTA_INFLATE_FAST_BITS
#define FAST_MASK ((1u << FAST_BITS) - 1)

/* given an array of code lengths, build a tree and its fast lookup table */
static void tinf_build_tree(TINF_TREE *t, const unsigned char *lengths, unsigned int num) {
  unsigned short offs[16], next[16];
  unsigned int i, sum, code;

  for (i = 0; i < 16; ++i)
    t->table[i] = 0;
  for (i = 0; i < num; ++i)
    t->table[lengths[i]]++;
  t->table[0] = 0;

  for (sum = 0, code = 0, i = 0; i < 16; ++i) {
    offs[i] = sum;
    t->index[i] = sum;
    sum += t->table[i];
    if (i > 0)
      code = (code + t->table[i - 1]) << 1;
    t->first[i] = code;
    next[i] = code;
  }

  for (i = 0; i <= FAST_MASK; ++i)
    t->fast[i] = 0;

  for (i = 0; i < num; ++i) {
    unsigned int len = lengths[i];
    if (!len)
      continue;
    t->trans[offs[len]++] = i;
    if (len <= FAST_BITS) {
      /* codes arrive MSB first, the bit buffer is LSB first */
      unsigned int c = next[len]++, rev = 0, j;
      for (j = 0; j < len; ++j, c >>= 1)
        rev = (rev << 1) | (c & 1);
      for (j = rev & FAST_MASK; j <= FAST_MASK; j += 1u << len)
        t->fast[j] = (unsigned short) ((len << 9) | i);
    }
  }
}

/* ---------------------- *
 * -- decode functions -- *
 * ---------------------- */

/* top the bit buffer up from the input buffer, without blocking for more */
static inline void tinf_refill(TINF_DATA *d) {
  while (d->bitcount <= 24 && d->source < d->source_limit) {
    d->tag |= (unsigned int) *d->source++ << d->bitcount;
    d->bitcount += 8;
  }
}

/* make at least num (<= 16) bits available; past the end of input reads zeros and sets eof */
static inline void tinf_need(TINF_DATA *d, unsigned int num) {
  while (d->bitcount < num) {
    int val = -1;
    if (d->source < d->source_limit) {
      val = *d->source++;
    } else if (!d->eof) {
      val = d->source_read_cb(d);
    }
    if (val < 0) {
      d->eof = true;
      val = 0;
    }
    d->tag |= (unsigned int) val << d->bitcount;
    d->bitcount += 8;
  }
}

static inline unsigned int tinf_take(TINF_DATA *d, unsigned int num) {
  unsigned int val = d->tag & ((1u << num) - 1);
  d->tag >>= num;
  d->bitcount -= num;
  return val;
}

static inline int tinf_getbit(TINF_DATA *d) {
  tinf_need(d, 1);
  return tinf_take(d, 1);
}

/* read a num bit value from a stream and add base */
static inline unsigned int tinf_read_bits(TINF_DATA *d, int num, int base) {
  if (!num)
    return base;
  tinf_need(d, num);
  return tinf_take(d, num) + base;
}

/* given a data stream and a tree, decode a symbol */
static int tinf_decode_symbol(TINF_DATA *d, TINF_TREE *t) {
  unsigned int code = 0, len;

  tinf_refill(d);
  if (d->bitcount >= FAST_BITS) {
    unsigned int e = t->fast[d->tag & FAST_MASK];
    if (e) {
      tinf_take(d, e >> 9);
      return e & 0x1ff;
    }
  }
  /* longer code, or too few bits left in the stream for a table lookup */
  for (len = 1; len < 16; ++len) {
    code = (code << 1) | tinf_getbit(d);
    if (code - t->first[len] < t->table[len]) {
      unsigned int sum = t->index[len] + code - t->first[len];
      if (sum >= t->size)
        return TINF_DATA_ERROR;
      return t->trans[sum];
    }
  }
  return TINF_DATA_ERROR;
}

/* given a data stream, decode dynamic trees from it */
static int tinf_decode_trees(TINF_DATA *d, TINF_TREE *lt, TINF_TREE *dt) {
  /* code lengths for 288 literal/len symbols and 32 dist symbols */
  unsigned char lengths[288 + 32];
  unsigned int hlit, hdist, hclen, hlimit;
  unsigned int i, num, length;

  /* get 5 bits HLIT (257-286) */
  hlit = tinf_read_bits(d, 5, 257);

  /* get 5 bits HDIST (1-32) */
  hdist = tinf_read_bits(d, 5, 1);

  /* get 4 bits HCLEN (4-19) */
  hclen = tinf_read_bits(d, 4, 4);

  for (i = 0; i < 19; ++i)
    lengths[i] = 0;

  /* read code lengths for code length alphabet */
  for (i = 0; i < hclen; ++i) {
    /* get 3 bits code length (0-7) */
    unsigned int clen = tinf_read_bits(d, 3, 0);

    lengths[CLCIDX[i]] = clen;
  }

  /* build code length tree, temporarily use length tree */
  tinf_build_tree(lt, lengths, 19);

  /* decode code lengths for the dynamic trees */
  hlimit = hlit + hdist;
  for (num = 0; num < hlimit;) {
    int sym = tinf_decode_symbol(d, lt);
    unsigned char fill_value = 0;
    int lbits, lbase = 3;

    /* error decoding */
    if (sym < 0)
      return sym;

    switch (sym) {
      case 16:
        /* copy previous code length 3-6 times (read 2 bits) */
        if (num == 0)
          return TINF_DATA_ERROR;
        fill_value = lengths[num - 1];
        lbits = 2;
        break;
      case 17:
        /* repeat code length 0 for 3-10 times (read 3 bits) */
        lbits = 3;
        break;
      case 18:
        /* repeat code length 0 for 11-138 times (read 7 bits) */
        lbits = 7;
        lbase = 11;
        break;
      default:
        /* values 0-15 represent the actual code lengths */
        lengths[num++] = sym;
        /* continue the for loop */
        continue;
    }

    /* special code length 16-18 are handled here */
    length = tinf_read_bits(d, lbits, lbase);
    if (num + length > hlimit)
      return TINF_DATA_ERROR;
    for (; length; --length) {
      lengths[num++] = fill_value;
    }
  }

  /* Check that there's "end of block" symbol */
  if (lengths[256] == 0) {
    return TINF_DATA_ERROR;
  }

  /* build dynamic trees */
  tinf_build_tree(lt, lengths, hlit);
  tinf_build_tree(dt, lengths + hlit, hdist);

  return TINF_OK;
}

/* build the fixed huffman trees (RFC 1951 3.2.6) through the generic tree
   builder; altered from upstream, which unrolls them by hand */
static void tinf_build_fixed_trees(TINF_TREE *lt, TINF_TREE *dt) {
  unsigned char lengths[288];
  unsigned int i;

  for (i = 0; i < 144; ++i)
    lengths[i] = 8;
  for (; i < 256; ++i)
    lengths[i] = 9;
  for (; i < 280; ++i)
    lengths[i] = 7;
  for (; i < 288; ++i)
    lengths[i] = 8;
  tinf_build_tree(lt, lengths, 288);

  for (i = 0; i < 32; ++i)
    lengths[i] = 5;
  tinf_build_tree(dt, lengths, 32);
}

/* decode a code longer than FAST_BITS from bits already in the buffer; len gets its length */
static int tinf_decode_long(const TINF_TREE *t, unsigned int tag, unsigned int *len) {
  unsigned int code = 0, l;
  for (l = 1; l < 16; ++l, tag >>= 1) {
    code = (code << 1) | (tag & 1);
    if (code - t->first[l] < t->table[l]) {
      unsigned int sum = t->index[l] + code - t->first[l];
      *len = l;
      return sum < t->size ? t->trans[sum] : TINF_DATA_ERROR;
    }
  }
  return TINF_DATA_ERROR;
}

/* fewest input bytes and output bytes for one pass of the fast loop: three
   refills of up to 4 bytes, and the longest match */
enum { FAST_IN_MIN = 12, FAST_OUT_MIN = 258 };

/* inflate with the bit buffer and both cursors in registers while there is room
   for a whole symbol on both sides; returns TINF_OK to hand over to the careful path */
static int tinf_inflate_fast(TINF_DATA *d, const TINF_TREE *lt, const TINF_TREE *dt) {
  const unsigned char *src = d->source;
  const unsigned char *const src_end = d->source_limit;
  unsigned char *out = d->dest;
  unsigned char *const out_end = d->dest_limit;
  unsigned char *const ring = d->dict_ring;
  const unsigned int ring_size = d->dict_size;
  const uint16_t *const lfast = lt->fast;
  const uint16_t *const dfast = dt->fast;
  unsigned int tag = d->tag, bits = d->bitcount;
  int res = TINF_OK;

#define REFILL() \
  while (bits <= 24) { \
    tag |= (unsigned int) *src++ << bits; \
    bits += 8; \
  }
#define DROP(n) (tag >>= (n), bits -= (n))

  while (src_end - src >= FAST_IN_MIN && out_end - out >= FAST_OUT_MIN) {
    unsigned int e, len;
    int sym;

    REFILL();
    e = lfast[tag & FAST_MASK];
    if (e) {
      len = e >> 9;
      sym = e & 0x1ff;
    } else {
      sym = tinf_decode_long(lt, tag, &len);
      if (sym < 0) {
        res = sym;
        break;
      }
    }
    DROP(len);

    if (sym < 256) {
      *out++ = sym;
      continue;
    }
    if (sym == 256) {
      res = TINF_DONE;
      break;
    }
    sym -= 257;
    if (sym >= 29) {
      res = TINF_DATA_ERROR;
      break;
    }
    /* the lit/len code and its extra bits fit the refilled buffer together */
    unsigned int length = LENGTH_BASE[sym] + (tag & ((1u << LENGTH_BITS[sym]) - 1));
    DROP(LENGTH_BITS[sym]);

    REFILL();
    e = dfast[tag & FAST_MASK];
    if (e) {
      len = e >> 9;
      sym = e & 0x1ff;
    } else {
      sym = tinf_decode_long(dt, tag, &len);
    }
    if (sym < 0 || sym >= 30) {
      res = TINF_DATA_ERROR;
      break;
    }
    DROP(len);
    REFILL();
    unsigned int offs = DIST_BASE[sym] + (tag & ((1u << DIST_BITS[sym]) - 1));
    DROP(DIST_BITS[sym]);
    if (offs > ring_size) {
      res = TINF_DICT_ERROR;
      break;
    }

    const unsigned char *from = out - offs;
    if (from < ring)
      from += ring_size;
    const unsigned char *const ring_end = ring + ring_size;
    do {
      *out++ = *from++;
      if (from == ring_end)
        from = ring;
    } while (--length);
  }
#undef REFILL
#undef DROP

  d->source = src;
  d->dest = out;
  d->tag = tag;
  d->bitcount = bits;
  return res;
}

/* ----------------------------- *
 * -- block inflate functions -- *
 * ----------------------------- */

/* given a stream and two trees, inflate until dest is full or the block ends */
static int tinf_inflate_block_data(TINF_DATA *d, TINF_TREE *lt, TINF_TREE *dt) {
  while (d->dest < d->dest_limit) {
    if (d->curlen != 0) {
      unsigned int n = d->dest_limit - d->dest;
      if (n > d->curlen)
        n = d->curlen;
      d->curlen -= n;
      do {
        unsigned char c = d->dict_ring[d->lz_off];
        TINF_PUT(d, c);
        if ((unsigned) ++d->lz_off == d->dict_size)
          d->lz_off = 0;
      } while (--n);
      continue;
    }

    if (d->source_limit - d->source >= FAST_IN_MIN && d->dest_limit - d->dest >= FAST_OUT_MIN) {
      int res = tinf_inflate_fast(d, lt, dt);
      if (res != TINF_OK)
        return res;
      continue;
    }

    unsigned int offs;
    int dist;
    int sym = tinf_decode_symbol(d, lt);

    if (d->eof)
      return TINF_DATA_ERROR;
    if (sym < 0)
      return sym;

    /* literal byte */
    if (sym < 256) {
      TINF_PUT(d, sym);
      continue;
    }

    /* end of block */
    if (sym == 256)
      return TINF_DONE;

    /* substring from sliding dictionary */
    sym -= 257;
    if (sym >= 29)
      return TINF_DATA_ERROR;

    d->curlen = tinf_read_bits(d, LENGTH_BITS[sym], LENGTH_BASE[sym]);

    dist = tinf_decode_symbol(d, dt);
    if (dist < 0 || dist >= 30)
      return TINF_DATA_ERROR;

    offs = tinf_read_bits(d, DIST_BITS[dist], DIST_BASE[dist]);
    if (offs > d->dict_size)
      return TINF_DICT_ERROR;

    d->lz_off = (int) (d->dest - d->dict_ring) - (int) offs;
    if (d->lz_off < 0)
      d->lz_off += d->dict_size;
  }
  return TINF_OK;
}

/* inflate next byte from uncompressed block of data */
static int tinf_inflate_uncompressed_block(TINF_DATA *d) {
  if (d->curlen == 0) {
    unsigned int length, invlength;

    /* the block starts on a byte boundary */
    tinf_take(d, d->bitcount & 7);
    length = tinf_read_bits(d, 16, 0);
    invlength = tinf_read_bits(d, 16, 0);
    if (length != (~invlength & 0x0000ffff))
      return TINF_DATA_ERROR;

    d->curlen = length + 1;
  }

  if (--d->curlen == 0)
    return TINF_DONE;

  unsigned char c = tinf_read_bits(d, 8, 0);
  TINF_PUT(d, c);
  return TINF_OK;
}

/* ---------------------- *
 * -- public functions -- *
 * ---------------------- */

void ota_inflate_init(TINF_DATA *d, unsigned char *dict, unsigned int dict_len) {
  d->source = NULL;
  d->source_limit = NULL;
  d->tag = 0;
  d->eof = 0;
  d->bitcount = 0;
  d->lz_off = 0;
  d->bfinal = 0;
  d->btype = -1;
  d->dict_size = dict_len;
  d->dict_ring = dict;
  d->curlen = 0;
  d->ltree.trans = d->ltrans;
  d->ltree.size = TINF_ARRAY_SIZE(d->ltrans);
  d->ltree.fast = d->lfast;
  d->dtree.trans = d->dtrans;
  d->dtree.size = TINF_ARRAY_SIZE(d->dtrans);
  d->dtree.fast = d->dfast;
}

int ota_inflate(TINF_DATA *d) {
  do {
    int res;

    if (d->btype == -1) {
      int old_btype;
    next_blk:
      old_btype = d->btype;
      d->bfinal = tinf_getbit(d);
      d->btype = tinf_read_bits(d, 2, 0);

      if (d->btype == 1 && old_btype != 1) {
        tinf_build_fixed_trees(&d->ltree, &d->dtree);
      } else if (d->btype == 2) {
        res = tinf_decode_trees(d, &d->ltree, &d->dtree);
        if (res != TINF_OK)
          return res;
      }
    }

    switch (d->btype) {
      case 0:
        res = tinf_inflate_uncompressed_block(d);
        break;
      case 1:
      case 2:
        res = tinf_inflate_block_data(d, &d->ltree, &d->dtree);
        break;
      default:
        return TINF_DATA_ERROR;
    }

    if (res == TINF_DONE && !d->bfinal)
      goto next_blk;

    if (res != TINF_OK)
      return res;

  } while (d->dest < d->dest_limit);

  return TINF_OK;
}
