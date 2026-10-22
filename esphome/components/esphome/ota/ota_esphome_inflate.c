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
 * removed, and the public names prefixed with ota_inflate.
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

/* every output byte also goes into the ring window */
#define TINF_PUT(d, c) \
  { \
    *d->dest++ = c; \
    d->dict_ring[d->dict_idx++] = c; \
    if (d->dict_idx == d->dict_size) \
      d->dict_idx = 0; \
  }

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

/* given an array of code lengths, build a tree */
static void tinf_build_tree(TINF_TREE *t, const unsigned char *lengths, unsigned int num) {
  unsigned short offs[16];
  unsigned int i, sum;

  /* clear code length count table */
  for (i = 0; i < 16; ++i)
    t->table[i] = 0;

  /* scan symbol lengths, and sum code length counts */
  for (i = 0; i < num; ++i)
    t->table[lengths[i]]++;

  /* In the lengths array, 0 means unused code. So, t->table[0] now contains
     number of unused codes. But table's purpose is to contain # of codes of
     particular length, and there're 0 codes of length 0. */
  t->table[0] = 0;

  /* compute offset table for distribution sort */
  for (sum = 0, i = 0; i < 16; ++i) {
    offs[i] = sum;
    sum += t->table[i];
  }

  /* create code->symbol translation table (symbols sorted by code) */
  for (i = 0; i < num; ++i) {
    if (lengths[i])
      t->trans[offs[lengths[i]]++] = i;
  }
}

/* ---------------------- *
 * -- decode functions -- *
 * ---------------------- */

static unsigned char uzlib_get_byte(TINF_DATA *d) {
  /* If end of source buffer is not reached, return next byte from source
     buffer. */
  if (d->source < d->source_limit) {
    return *d->source++;
  }

  /* Otherwise if there's callback and we haven't seen EOF yet, try to
     read next byte using it. (Note: the callback can also update ->source
     and ->source_limit). */
  if (!d->eof) {
    int val = d->source_read_cb(d);
    if (val >= 0) {
      return (unsigned char) val;
    }
  }

  /* Otherwise, we hit EOF (either from ->source_read_cb() or from exhaustion
     of the buffer), and it will be "sticky", i.e. further calls to this
     function will end up here too. */
  d->eof = true;

  return 0;
}

/* get one bit from source stream */
static int tinf_getbit(TINF_DATA *d) {
  unsigned int bit;

  /* check if tag is empty */
  if (!d->bitcount--) {
    /* load next tag */
    d->tag = uzlib_get_byte(d);
    d->bitcount = 7;
  }

  /* shift bit out of tag */
  bit = d->tag & 0x01;
  d->tag >>= 1;

  return bit;
}

/* read a num bit value from a stream and add base */
static unsigned int tinf_read_bits(TINF_DATA *d, int num, int base) {
  unsigned int val = 0;

  /* read num bits */
  if (num) {
    unsigned int limit = 1 << (num);
    unsigned int mask;

    for (mask = 1; mask < limit; mask *= 2)
      if (tinf_getbit(d))
        val += mask;
  }

  return val + base;
}

/* given a data stream and a tree, decode a symbol */
static int tinf_decode_symbol(TINF_DATA *d, TINF_TREE *t) {
  int sum = 0, cur = 0, len = 0;

  /* get more bits while code value is above sum */
  do {
    cur = 2 * cur + tinf_getbit(d);

    if (++len == TINF_ARRAY_SIZE(t->table)) {
      return TINF_DATA_ERROR;
    }

    sum += t->table[len];
    cur -= t->table[len];

  } while (cur >= 0);

  sum += cur;
  if (sum < 0 || sum >= t->size) {
    return TINF_DATA_ERROR;
  }

  return t->trans[sum];
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

/* ----------------------------- *
 * -- block inflate functions -- *
 * ----------------------------- */

/* given a stream and two trees, inflate next chunk of output (a byte or more) */
static int tinf_inflate_block_data(TINF_DATA *d, TINF_TREE *lt, TINF_TREE *dt) {
  if (d->curlen == 0) {
    unsigned int offs;
    int dist;
    int sym = tinf_decode_symbol(d, lt);

    if (d->eof) {
      return TINF_DATA_ERROR;
    }

    if (sym < 0) {
      return sym;
    }

    /* literal byte */
    if (sym < 256) {
      TINF_PUT(d, sym);
      return TINF_OK;
    }

    /* end of block */
    if (sym == 256) {
      return TINF_DONE;
    }

    /* substring from sliding dictionary */
    sym -= 257;
    if (sym >= 29) {
      return TINF_DATA_ERROR;
    }

    /* possibly get more bits from length code */
    d->curlen = tinf_read_bits(d, LENGTH_BITS[sym], LENGTH_BASE[sym]);

    dist = tinf_decode_symbol(d, dt);
    if (dist < 0 || dist >= 30) {
      return TINF_DATA_ERROR;
    }

    /* possibly get more bits from distance code */
    offs = tinf_read_bits(d, DIST_BITS[dist], DIST_BASE[dist]);

    /* calculate and validate actual LZ offset to use */
    if (offs > d->dict_size) {
      return TINF_DICT_ERROR;
    }
    /* Note: we don't try to catch offset which points to not yet filled
       part of the dictionary here. Doing so would require keeping another
       variable to track "filled in" size of the dictionary. Appearance of
       such an offset cannot lead to accessing memory outside of the
       dictionary buffer, and clients which don't want to leak unrelated
       information, should explicitly initialize dictionary buffer passed
       to uzlib. */

    d->lz_off = d->dict_idx - offs;
    if (d->lz_off < 0) {
      d->lz_off += d->dict_size;
    }
  }

  /* copy next byte from dict substring */
  TINF_PUT(d, d->dict_ring[d->lz_off]);
  if ((unsigned) ++d->lz_off == d->dict_size) {
    d->lz_off = 0;
  }
  d->curlen--;
  return TINF_OK;
}

/* inflate next byte from uncompressed block of data */
static int tinf_inflate_uncompressed_block(TINF_DATA *d) {
  if (d->curlen == 0) {
    unsigned int length, invlength;

    /* get length */
    length = uzlib_get_byte(d);
    length += 256 * uzlib_get_byte(d);
    /* get one's complement of length */
    invlength = uzlib_get_byte(d);
    invlength += 256 * uzlib_get_byte(d);
    /* check length */
    if (length != (~invlength & 0x0000ffff))
      return TINF_DATA_ERROR;

    /* increment length to properly return TINF_DONE below, without
       producing data at the same time */
    d->curlen = length + 1;

    /* make sure we start next block on a byte boundary */
    d->bitcount = 0;
  }

  if (--d->curlen == 0) {
    return TINF_DONE;
  }

  unsigned char c = uzlib_get_byte(d);
  TINF_PUT(d, c);
  return TINF_OK;
}

/* ---------------------- *
 * -- public functions -- *
 * ---------------------- */

/* initialize decompression structure */
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
  d->dict_idx = 0;
  d->curlen = 0;
  d->ltree.trans = d->ltrans;
  d->ltree.size = TINF_ARRAY_SIZE(d->ltrans);
  d->dtree.trans = d->dtrans;
  d->dtree.size = TINF_ARRAY_SIZE(d->dtrans);
}

/* inflate next output bytes from compressed stream */
int ota_inflate(TINF_DATA *d) {
  do {
    int res;

    /* start a new block */
    if (d->btype == -1) {
      int old_btype;
    next_blk:
      old_btype = d->btype;
      /* read final block flag */
      d->bfinal = tinf_getbit(d);
      /* read block type (2 bits) */
      d->btype = tinf_read_bits(d, 2, 0);

      if (d->btype == 1 && old_btype != 1) {
        /* build fixed huffman trees */
        tinf_build_fixed_trees(&d->ltree, &d->dtree);
      } else if (d->btype == 2) {
        /* decode trees from stream */
        res = tinf_decode_trees(d, &d->ltree, &d->dtree);
        if (res != TINF_OK) {
          return res;
        }
      }
    }

    /* process current block */
    switch (d->btype) {
      case 0:
        /* decompress uncompressed block */
        res = tinf_inflate_uncompressed_block(d);
        break;
      case 1:
      case 2:
        /* decompress block with fixed/dynamic huffman trees */
        /* trees were decoded previously, so it's the same routine for both */
        res = tinf_inflate_block_data(d, &d->ltree, &d->dtree);
        break;
      default:
        return TINF_DATA_ERROR;
    }

    if (res == TINF_DONE && !d->bfinal) {
      /* the block has ended (without producing more data), but we
         can't return without data, so start procesing next block */
      goto next_blk;
    }

    if (res != TINF_OK) {
      return res;
    }

  } while (d->dest < d->dest_limit);

  return TINF_OK;
}
