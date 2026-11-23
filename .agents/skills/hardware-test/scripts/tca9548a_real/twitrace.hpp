#pragma once
// Records every transaction the ESP8266 Wire core puts on the wire while armed (link with -Wl,--wrap=twi_writeTo and
// -Wl,--wrap=twi_readFrom), hashes them and checks mux select, device op, deselect order for a device at 0x68.
#include <Arduino.h>
#include "esphome/core/log.h"
extern "C" uint8_t __real_twi_writeTo(unsigned char a, unsigned char *b, unsigned int n, unsigned char stop);
extern "C" uint8_t __real_twi_readFrom(unsigned char a, unsigned char *b, unsigned int n, unsigned char stop);
namespace twitrace {
inline volatile bool armed = false;
inline uint32_t hash = 2166136261u, ops = 0, mux_sel = 0, mux_desel = 0, dev_w = 0, dev_r = 0, w33 = 0, wbytes = 0,
                bad = 0, errs = 0, maxw = 0;
inline int state = 0;  // 0 idle (deselected), 1 selected, 2 selected and device op done
inline char seq[96];
inline int seqn = 0;
inline void mix(uint8_t v) { hash = (hash ^ v) * 16777619u; }
inline void note(char c) {
  if (seqn < (int) sizeof(seq) - 1)
    seq[seqn++] = c;
}
inline void rec(bool rd, unsigned char a, const unsigned char *b, unsigned int n, unsigned char stop, uint8_t ret) {
  ops++;
  mix(rd);
  mix(a);
  mix(n & 0xFF);
  mix(n >> 8);
  mix(stop);
  mix(ret);
  for (unsigned i = 0; i < n; i++)
    mix(b[i]);
  if (ret != 0)
    errs++;
  if (a == 0x70 && !rd && n == 1) {
    if (b[0] == 0x01) {
      mux_sel++;
      note('S');
      if (state != 0)
        bad++;
      state = 1;
    } else if (b[0] == 0x00) {
      mux_desel++;
      note('D');
      if (state != 2)
        bad++;
      state = 0;
    } else {
      note('?');
      bad++;
    }
  } else if (a == 0x68) {
    if (state == 0)
      bad++;
    else
      state = 2;
    if (rd) {
      dev_r++;
      note('r');
    } else {
      dev_w++;
      wbytes += n;
      if (n > maxw)
        maxw = n;
      if (n == 33) {
        w33++;
        note('B');
      } else
        note('w');
    }
  } else {
    note('x');
    bad++;
  }
}
inline void start() {
  hash = 2166136261u;
  ops = mux_sel = mux_desel = dev_w = dev_r = w33 = wbytes = bad = errs = maxw = 0;
  state = 0;
  seqn = 0;
  armed = true;
}
inline void stop() {
  armed = false;
  seq[seqn] = 0;
}
inline void report(const char *name) {
  ESP_LOGI("twitrace",
           "TRACE %s ops=%u hash=%08X selects=%u deselects=%u dev_writes=%u dev_reads=%u writes33=%u wbytes=%u maxw=%u "
           "order_violations=%u nacks=%u",
           name, ops, hash, mux_sel, mux_desel, dev_w, dev_r, w33, wbytes, maxw, bad, errs);
  ESP_LOGI("twitrace", "SEQ %s first: %s", name, seq);
}
}  // namespace twitrace
extern "C" uint8_t __wrap_twi_writeTo(unsigned char a, unsigned char *b, unsigned int n, unsigned char stop) {
  uint8_t r = __real_twi_writeTo(a, b, n, stop);
  if (twitrace::armed)
    twitrace::rec(false, a, b, n, stop, r);
  return r;
}
extern "C" uint8_t __wrap_twi_readFrom(unsigned char a, unsigned char *b, unsigned int n, unsigned char stop) {
  uint8_t r = __real_twi_readFrom(a, b, n, stop);
  if (twitrace::armed)
    twitrace::rec(true, a, b, n, stop, r);
  return r;
}
