#pragma once
// Fake SPI pins for a software SPI bus: SPIDelegateBitBash clocks every bit through MosiPin/MisoPin, and CsPin frames
// the chip select windows, so a chip model (SpiChip) sees each byte and answers per byte position. No ESP-IDF
// includes, so it also builds on the host. lock()/unlock() guard the model hooks (a portMUX on the ESP32); after_end()
// runs after the unlock at the end of a window, for work that must not run inside a critical section (esp_timer).
#include <cstdint>
#include "esphome/core/gpio_pin.h"

namespace spibits {
using namespace esphome;

class NopPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_NONE; }
  bool digital_read() override { return false; }
  void digital_write(bool) override {}
};

// One chip select window: bytes in, a reply per byte position
struct SpiChip {
  bool sel{false}, mosi{false};
  uint16_t pos{0};
  uint8_t bit{0}, in{0}, out{0};
  virtual ~SpiChip() = default;
  virtual uint8_t reply_locked(uint16_t pos) = 0;
  virtual void byte_locked(uint16_t pos, uint8_t value) = 0;
  virtual void begin_locked() {}
  virtual void end_locked() = 0;
  virtual void after_end() {}
  virtual void lock() {}
  virtual void unlock() {}
};

class MosiPin : public NopPin {
 public:
  SpiChip *c{nullptr};
  void digital_write(bool v) override { c->mosi = v; }
};
class CsPin : public NopPin {
 public:
  SpiChip *c{nullptr};
  void digital_write(bool high) override {
    c->lock();
    const bool ended = high && c->sel;
    if (!high) {
      c->sel = true;
      c->pos = c->bit = c->in = 0;
      c->begin_locked();
    } else if (c->sel) {
      c->sel = false;
      c->end_locked();
    }
    c->unlock();
    if (ended)
      c->after_end();
  }
};
class MisoPin : public NopPin {
 public:
  SpiChip *c{nullptr};
  bool digital_read() override {
    if (c->bit == 0) {
      c->lock();
      c->out = c->reply_locked(c->pos);
      c->unlock();
    }
    const bool b = (c->out >> (7 - c->bit)) & 1;
    c->in = (c->in << 1) | (c->mosi ? 1 : 0);
    if (++c->bit == 8) {
      c->lock();
      c->byte_locked(c->pos, c->in);
      c->unlock();
      c->bit = 0;
      c->in = 0;
      c->pos++;
    }
    return b;
  }
};
struct PinSet {
  NopPin clk;
  MosiPin mosi;
  MisoPin miso;
  CsPin cs;
  NopPin rst;
  NopPin busy;
  void bind(SpiChip *c) { mosi.c = miso.c = cs.c = c; }
};
}  // namespace spibits
