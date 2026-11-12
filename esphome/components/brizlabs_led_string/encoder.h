#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Brizlabs uses "Data over Power" LED modules that are signaled by "dipping" their 24VDC
// supply lines per this protocol.
// We run SPI at 100 kHz, giving 10 us time slots to pull either high (1) or low (0)
// A 0 bit is encoded as 10, and a 1 bit is encoded as 110
// Each individual field is sent LSB first, and we pack everything into bytes for a single SPI write.
//
// ┌───────────────┬──────┬───────────┐
// │     Field     │ Bits │   Slots   │
// ├───────────────┼──────┼───────────┤
// │ LED id        │ 6    │ 12–18     │
// ├───────────────┼──────┼───────────┤
// │ zone          │ 4    │ 8–12      │
// ├───────────────┼──────┼───────────┤
// │ header (0xFF) │ 8    │ 24        │
// ├───────────────┼──────┼───────────┤
// │ R             │ 8    │ 16–24     │
// ├───────────────┼──────┼───────────┤
// │ G             │ 8    │ 16–24     │
// ├───────────────┼──────┼───────────┤
// │ B             │ 8    │ 16–24     │
// ├───────────────┼──────┼───────────┤
// │ trailer       │ —    │ 8         │
// └───────────────┴──────┴───────────┘
//
// - LED id - a seemingly-random factory ID assigned to each LED module, which technically means every LED is
// individually addressable. However, 6 bits means there's guaranteed conflicts, and short of manually mapping every
// module's (zone, ID) there's no real use for them. As such, we always send an LED id of 0, which effectively means
// "broadcast". LED control is then done by...
// - zone - Each LED module also has a zone number, which starts at 1 and increments up to 2^4 - 1 (15) as you proceed
// down the string of lights. Once the max is reached, it loops back to 1. 0 again is "broadcast", so the whole string.
// - header - fixed, always 0xFF
// - R, G, B brightness where 0 = off
// - trailer is always 11110000

namespace esphome::brizlabs_led_string {

static constexpr size_t FRAME_BYTES = 17;  // Worst case (all bits as 110), longest frame = 42 bits * 3 + 8 = 134 slots
static_assert(FRAME_BYTES * 8 >= 42 * 3 + 8, "frame buffer too small for the longest frame");
static_assert(FRAME_BYTES <= 64, "frame exceeds the SPI FIFO; transfers would need DMA");

struct Writer {
  uint8_t *out;
  size_t n = 0;
  void slot(uint8_t level) {
    if (level)
      this->out[this->n >> 3] |= 0x80 >> (this->n & 7);
    this->n++;
  }
  void field(uint32_t v, int bits) {
    for (int b = 0; b < bits; b++) {
      this->slot(1);
      if ((v >> b) & 1)
        this->slot(1);
      this->slot(0);
    }
  }
};

// Fills out[0..FRAME_BYTES) and returns the number of bits to transmit.
inline size_t encode(uint8_t *out, uint8_t id, uint8_t zone, uint8_t hdr, uint8_t r, uint8_t g, uint8_t b) {
  memset(out, 0, FRAME_BYTES);
  Writer w{out};
  w.field(id, 6);
  w.field(zone, 4);
  w.field(hdr, 8);
  w.field(r, 8);
  w.field(g, 8);
  w.field(b, 8);
  for (int k = 0; k < 4; k++)
    w.slot(1);
  for (int k = 0; k < 4; k++)
    w.slot(0);
  return w.n;
}

}  // namespace esphome::brizlabs_led_string
