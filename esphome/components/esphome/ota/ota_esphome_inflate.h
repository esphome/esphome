#pragma once
// Raw deflate decoder for compressed OTA uploads, cut down from uzlib
// (https://github.com/pfalcon/uzlib, zlib licence, see the .c file).
// Kept in C so it stays close to upstream; the decoder writes through a
// ring window so the image never has to be held in RAM.

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

struct OtaInflateTree {
  uint16_t table[16]; /* table of code length counts */
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

  unsigned int tag;
  unsigned int bitcount;

  /* Output cursor and one past the end of the output buffer */
  unsigned char *dest;
  unsigned char *dest_limit;

  bool eof;

  int btype;
  int bfinal;
  unsigned int curlen;
  int lz_off;
  /* Ring window holding the last dict_size output bytes for back references */
  unsigned char *dict_ring;
  unsigned int dict_size;
  unsigned int dict_idx;

  struct OtaInflateTree ltree; /* dynamic length/symbol tree */
  struct OtaInflateTree dtree; /* dynamic distance tree */
  uint16_t ltrans[288];
  uint16_t dtrans[32]; /* the distance alphabet has 30 symbols, so the tree is kept small */
};

/* dict must be at least as large as the window the encoder used (its max back reference distance) */
void ota_inflate_init(struct OtaInflateState *d, unsigned char *dict, unsigned int dict_len);
/* Produce output until dest reaches dest_limit (OK), the stream ends (DONE) or an error occurs */
int ota_inflate(struct OtaInflateState *d);

#ifdef __cplusplus
}
#endif
