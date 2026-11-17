// Raw CC1101 helpers for the #17578 test: register readback and a TX that streams packets longer than the TX FIFO.
#pragma once
#include "esphome/components/cc1101/cc1101.h"
#include "esphome/core/application.h"
#include <algorithm>

namespace ccl {
using esphome::cc1101::CC1101Component;
inline uint8_t rd(CC1101Component *c, uint8_t a) {
  c->enable();
  c->write_byte(a | 0x80);
  uint8_t v = c->read_byte();
  c->disable();
  return v;
}
inline uint8_t rdst(CC1101Component *c, uint8_t a) { return rd(c, a | 0x40); }
inline void wr(CC1101Component *c, uint8_t a, uint8_t v) {
  c->enable();
  c->write_byte(a);
  c->write_byte(v);
  c->disable();
}
inline void strobe(CC1101Component *c, uint8_t s) {
  c->enable();
  c->write_byte(s);
  c->disable();
}
inline void fifo(CC1101Component *c, const uint8_t *d, size_t n) {
  c->enable();
  c->write_byte(0x7F);
  for (size_t i = 0; i < n; i++)
    c->write_byte(d[i]);
  c->disable();
}
// Datasheet 15.2.2: infinite mode with PKTLEN = n mod 256, fixed mode once fewer than 256 bytes remain.
// Returns 0 on success, 1 on an early underflow, 2 on a timeout.
inline int long_tx(CC1101Component *c, const uint8_t *d, size_t n, uint32_t poll_us = 300) {
  c->set_idle();
  const uint8_t p0 = rd(c, 0x08), plen = rd(c, 0x06);
  strobe(c, 0x3B);  // SFTX
  bool inf = n > 255;
  wr(c, 0x06, n & 0xFF);
  wr(c, 0x08, (p0 & ~3) | (inf ? 2 : 0));
  size_t w = std::min<size_t>(n, 64);
  fifo(c, d, w);
  strobe(c, 0x35);  // STX
  int err = 0;
  const uint32_t t0 = millis();
  while (true) {
    uint8_t tb = rdst(c, 0x3A);
    if (tb & 0x80) {
      if (w < n)
        err = 1;
      break;
    }
    tb &= 0x7F;
    const size_t sent = w - tb;
    if (inf && (n & 0xFF) != 0 && n - sent < 256) {
      wr(c, 0x08, p0 & ~3);
      inf = false;
    }
    if (w < n && tb < 48) {
      const size_t k = std::min<size_t>(n - w, 63 - tb);
      fifo(c, d + w, k);
      w += k;
    }
    const uint8_t marc = rdst(c, 0x35) & 0x1F;
    if (w == n && tb == 0 && marc != 0x13 && marc != 0x14)  // not TX or TX_END
      break;
    if (millis() - t0 > 4000) {
      err = 2;
      break;
    }
    esphome::App.feed_wdt();
    delayMicroseconds(poll_us);
  }
  delay(5);
  c->set_idle();
  strobe(c, 0x3B);
  wr(c, 0x06, plen);
  wr(c, 0x08, p0);
  c->begin_rx();
  return err;
}
inline void fill(std::vector<uint8_t> &b, size_t len, int b0, uint8_t seq) {
  b.resize(len);
  for (size_t i = 0; i < len; i++)
    b[i] = (uint8_t) (seq * 31 + i * 7);
  b[0] = (uint8_t) b0;
  if (len > 1)
    b[1] = seq;
}
inline bool check(const std::vector<uint8_t> &x) {
  if (x.size() < 2)
    return true;
  const uint8_t seq = x[1];
  for (size_t i = 2; i < x.size(); i++)
    if (x[i] != (uint8_t) (seq * 31 + i * 7))
      return false;
  return true;
}
}  // namespace ccl
