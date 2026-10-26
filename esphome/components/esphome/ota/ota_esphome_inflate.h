#pragma once
// Raw deflate decoder cut down from uzlib (https://github.com/pfalcon/uzlib,
// zlib licence, see the .c file); output goes through a ring window.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum OtaInflateResult {
  OTA_INFLATE_OK = 0,   /* more data produced, call again */
  OTA_INFLATE_DONE = 1, /* end of compressed stream reached */
  OTA_INFLATE_DATA_ERROR = -3,
  OTA_INFLATE_DICT_ERROR = -5,
};

/* Code bits each lookup table resolves; longer codes take the canonical decode.
   The distance alphabet has 30 symbols, so its table is kept small */
enum { OTA_INFLATE_LIT_FAST_BITS = 9, OTA_INFLATE_DIST_FAST_BITS = 6 };

struct OtaInflateTree {
  uint16_t table[16]; /* table of code length counts */
  uint16_t first[16]; /* first canonical code of each length */
  uint16_t index[16]; /* trans index of each length's first symbol */
  uint16_t *fast;     /* (len << 9) | symbol by the next input bits, 0 = longer code */
  uint16_t *trans;    /* code -> symbol translation table, size entries */
  uint16_t size;
};

struct OtaInflateState {
  /* Next byte in the input buffer and one past its end */
  const unsigned char *source;
  const unsigned char *source_limit;
  /* Called when source is exhausted; returns the next byte or -1 at EOF.
     It may refill source/source_limit for buffered operation. */
  int (*source_read_cb)(struct OtaInflateState *d);
  /* Output cursor in dict_ring: each call fills the ring from here */
  unsigned char *dest;
  /* Ring window holding the last dict_size output bytes for back references */
  unsigned char *dict_ring;

  unsigned int tag;
  unsigned int curlen;
  int lz_off;
  unsigned int dict_size;

  /* btype is -1 between blocks; bitcount is the number of valid bits in tag (<= 32) */
  int8_t btype;
  uint8_t bfinal;
  uint8_t bitcount;
  bool eof;

  struct OtaInflateTree ltree; /* dynamic length/symbol tree */
  struct OtaInflateTree dtree; /* dynamic distance tree */
  uint16_t ltrans[288];
  uint16_t dtrans[32]; /* the distance alphabet has 30 symbols, so the tree is kept small */
  uint16_t lfast[1 << OTA_INFLATE_LIT_FAST_BITS];
  uint16_t dfast[1 << OTA_INFLATE_DIST_FAST_BITS];
};

/* dict must cover the encoder's window (its largest back reference) */
void ota_inflate_init(struct OtaInflateState *d, unsigned char *dict, unsigned int dict_len);
/* Inflates into dict until it is full (OK) or the stream ends (DONE); the output
   is dict up to dest. The next call after OK starts dict over, so take its
   contents first */
int ota_inflate(struct OtaInflateState *d);

#ifdef __cplusplus
}
#endif
