#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"

#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/m5stack_unit_lcd/m5stack_unit_lcd.h"

namespace esphome::m5stack_unit_lcd::testing {

// Unit LCD command bytes, duplicated here so the tests check the wire protocol independently
// of the constants in the driver.
static constexpr uint8_t CMD_READ_ID = 0x04;
static constexpr uint8_t CMD_READ_BUFCOUNT = 0x09;
static constexpr uint8_t CMD_INVOFF = 0x20;
static constexpr uint8_t CMD_INVON = 0x21;
static constexpr uint8_t CMD_BRIGHTNESS = 0x22;
static constexpr uint8_t CMD_CASET = 0x2A;
static constexpr uint8_t CMD_RASET = 0x2B;
static constexpr uint8_t CMD_ROTATE = 0x36;
static constexpr uint8_t CMD_SET_POWER = 0x38;
static constexpr uint8_t CMD_SET_SLEEP = 0x39;
static constexpr uint8_t CMD_WRITE_RAW_16 = 0x42;
static constexpr uint8_t CMD_FILLRECT_16 = 0x6A;

static constexpr int PANEL_WIDTH = 135;
static constexpr int PANEL_HEIGHT = 240;
static constexpr size_t FRAME_BYTES = PANEL_WIDTH * PANEL_HEIGHT * 2;
static constexpr size_t CHUNK = M5StackUnitLCD::MAX_PIXELS_PER_TX;

/// The settings transaction the driver sends at boot and after a reconnect.
inline std::vector<uint8_t> config_bytes(uint8_t brightness, bool invert = false, bool sleep = false) {
  return {CMD_SET_POWER,
          1,
          CMD_ROTATE,
          0,
          CMD_BRIGHTNESS,
          brightness,
          invert ? CMD_INVON : CMD_INVOFF,
          CMD_SET_SLEEP,
          static_cast<uint8_t>(sleep)};
}

/// One I2C transaction as seen by the fake bus: the bytes written, and how many were read back.
struct Transaction {
  std::vector<uint8_t> written;
  size_t read_len{0};

  bool is_query() const { return this->read_len > 0; }
  bool is_pixel_data() const {
    return !this->written.empty() &&
           (this->written[0] == CMD_WRITE_RAW_16 || (this->written.size() > 6 && this->written[6] == CMD_WRITE_RAW_16));
  }
  bool has_window() const { return !this->written.empty() && this->written[0] == CMD_CASET; }
  /// Number of RGB565 pixels carried by a WRITE_RAW_16 transaction.
  size_t pixel_count() const {
    const size_t header = this->has_window() ? 7 : 1;
    return (this->written.size() - header) / 2;
  }
  /// Pixel bytes carried by a WRITE_RAW_16 transaction (after the command byte(s)).
  std::vector<uint8_t> pixels() const {
    const size_t header = this->has_window() ? 7 : 1;
    return {this->written.begin() + header, this->written.end()};
  }
};

/// What the unit shows, modelled on its firmware (m5stack/M5Unit-LCD-Internal-FW,
/// src/command_processor.cpp). Pixels go into an off-screen canvas at a write cursor that only
/// CASET, RASET and FILLRECT move; the cursor survives the end of a transaction and any other
/// command in between. A transaction is a run of commands, and WRITE_RAW_16 takes the rest of it.
struct PanelModel {
  PanelModel() : ram(FRAME_BYTES, 0) {}

  void apply(const std::vector<uint8_t> &tx) {
    size_t i = 0;
    auto param = [&tx, &i]() -> uint8_t {
      if (i >= tx.size()) {
        ADD_FAILURE() << "command truncated";
        return 0;
      }
      return tx[i++];
    };
    while (i < tx.size()) {
      const uint8_t cmd = tx[i++];
      switch (cmd) {
        case CMD_CASET: {
          uint8_t xs = param(), xe = param();
          if (xs > xe)
            std::swap(xs, xe);
          this->xs = xs;
          this->xe = xe;
          this->xptr = xs;
          this->yptr = this->ys;
          break;
        }
        case CMD_RASET: {
          uint8_t ys = param(), ye = param();
          if (ys > ye)
            std::swap(ys, ye);
          this->ys = ys;
          this->ye = ye;
          this->yptr = ys;
          this->xptr = this->xs;
          break;
        }
        case CMD_WRITE_RAW_16:
          for (; i + 1 < tx.size(); i += 2)
            this->write_pixel(tx[i], tx[i + 1]);
          return;
        case CMD_FILLRECT_16: {
          this->xs = param();
          this->ys = param();
          this->xe = param();
          this->ye = param();
          const uint8_t hi = param(), lo = param();
          this->xptr = this->xs;
          this->yptr = this->ys;
          for (int y = this->ys; y <= this->ye; y++) {
            for (int x = this->xs; x <= this->xe; x++)
              this->set(x, y, hi, lo);
          }
          break;
        }
        case CMD_BRIGHTNESS:
          this->brightness = param();
          break;
        case CMD_INVON:
        case CMD_INVOFF:
          this->inverted = cmd == CMD_INVON;
          break;
        case CMD_SET_SLEEP:
          this->sleeping = param() != 0;
          break;
        case CMD_SET_POWER:
        case CMD_ROTATE:
          param();
          break;
        default:
          ADD_FAILURE() << "unexpected command 0x" << std::hex << int(cmd);
          return;
      }
    }
  }

  /// The unit lost power and booted again: blank canvas, default settings.
  void reboot() {
    std::fill(this->ram.begin(), this->ram.end(), 0);
    this->xs = this->xe = this->ys = this->ye = this->xptr = this->yptr = 0;
    this->brightness = 128;
    this->inverted = false;
    this->sleeping = false;
  }

  void set(int x, int y, uint8_t hi, uint8_t lo) {
    if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT)
      return;
    const size_t i = (static_cast<size_t>(y) * PANEL_WIDTH + x) * 2;
    this->ram[i] = hi;
    this->ram[i + 1] = lo;
  }

  void write_pixel(uint8_t hi, uint8_t lo) {
    this->set(this->xptr, this->yptr, hi, lo);
    if (++this->xptr > this->xe) {
      this->xptr = this->xs;
      if (++this->yptr > this->ye)
        this->yptr = this->ys;
    }
  }

  std::vector<uint8_t> ram;
  int xs{0}, xe{0}, ys{0}, ye{0}, xptr{0}, yptr{0};
  uint8_t brightness{128};
  bool inverted{false};
  bool sleeping{false};
};

/// Fake Unit LCD on a fake I2C bus. Answers READ_ID and READ_BUFCOUNT, records every transaction
/// so tests can assert on the exact byte stream, applies successful writes to a PanelModel, and
/// can be told to fail writes or to act unplugged.
class FakeUnitLCDBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    this->last_address = address;
    Transaction t;
    t.written.assign(write_buffer, write_buffer + write_count);
    t.read_len = read_count;
    this->log.push_back(t);

    if (this->unplugged)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (write_count == 1 && write_buffer[0] == CMD_READ_ID && read_count == 4) {
      if (this->fail_id_read)
        return i2c::ERROR_TIMEOUT;
      memcpy(read_buffer, this->id, 4);
      return i2c::ERROR_OK;
    }
    if (write_count == 1 && write_buffer[0] == CMD_READ_BUFCOUNT && read_count == 1) {
      read_buffer[0] = this->bufcount;
      return i2c::ERROR_OK;
    }
    if (this->fail_writes)
      return i2c::ERROR_TIMEOUT;
    if (t.is_pixel_data()) {
      if (this->fail_after_pixel_writes >= 0 && this->pixel_writes_seen++ >= this->fail_after_pixel_writes)
        return i2c::ERROR_TIMEOUT;
      if (this->delay_per_write_ms > 0)
        delay(this->delay_per_write_ms);  // NOLINT - emulate a slow bus so loop() yields after one chunk
    }
    this->panel.apply(t.written);
    return i2c::ERROR_OK;
  }

  /// Transactions that carried data to the panel (queries excluded).
  std::vector<Transaction> writes() const {
    std::vector<Transaction> out;
    for (const auto &t : this->log) {
      if (!t.is_query())
        out.push_back(t);
    }
    return out;
  }
  std::vector<Transaction> pixel_writes() const {
    std::vector<Transaction> out;
    for (const auto &t : this->log) {
      if (!t.is_query() && t.is_pixel_data())
        out.push_back(t);
    }
    return out;
  }
  size_t pixels_sent() const {
    size_t n = 0;
    for (const auto &t : this->pixel_writes())
      n += t.pixel_count();
    return n;
  }
  size_t query_count(uint8_t cmd) const {
    size_t n = 0;
    for (const auto &t : this->log) {
      if (t.is_query() && t.written.size() == 1 && t.written[0] == cmd)
        n++;
    }
    return n;
  }
  void clear() { this->log.clear(); }

  std::vector<Transaction> log;
  PanelModel panel;
  uint8_t last_address{0};
  uint8_t id[4] = {0x77, 0x89, 0x01, 0x02};
  uint8_t bufcount{255};
  bool unplugged{false};
  bool fail_id_read{false};
  bool fail_writes{false};
  /// When >= 0, the Nth (0-based) successful pixel write and every later one fail until reset.
  int fail_after_pixel_writes{-1};
  int pixel_writes_seen{0};
  /// Wall-clock time burnt per pixel write, to exercise the loop() time slicing deterministically.
  uint32_t delay_per_write_ms{0};
};

/// Exposes the component state so setup() can be driven the way Application drives it. The
/// host test binary has no running scheduler, so Component::call() cannot be used directly.
class TestableM5StackUnitLCD : public M5StackUnitLCD {
 public:
  /// Enter the SETUP state (is_ready() becomes true) and run setup(), as Component::call() does.
  void run_setup() {
    this->set_component_state_(COMPONENT_STATE_SETUP);
    this->setup();
  }

  /// Reallocate both buffers in small chunks, as a fragmented heap would. Chunk sizes that are
  /// not a multiple of a row (or even of a pixel) put boundaries mid-row and mid-pixel. Both
  /// buffers come back black, which matches the panel blanked by setup().
  void use_split_buffers(size_t frame_chunk, size_t shadow_chunk) {
    ASSERT_TRUE(this->buffer_.init(FRAME_BYTES, frame_chunk));
    ASSERT_TRUE(this->prev_.init(FRAME_BYTES, shadow_chunk));
    ASSERT_GT(this->buffer_.get_buffer_count(), 1u);
    ASSERT_GT(this->prev_.get_buffer_count(), 1u);
  }

  /// The frame buffer as drawn, in panel byte order.
  std::vector<uint8_t> frame() {
    std::vector<uint8_t> out(FRAME_BYTES);
    for (size_t i = 0; i < FRAME_BYTES; i++)
      out[i] = this->buffer_[i];
    return out;
  }
};

/// A display wired to a fake bus, with a helper to run the loop() state machine to completion.
struct Fixture {
  Fixture() {
    this->display.set_i2c_bus(&this->bus);
    this->display.set_i2c_address(0x3E);
    this->display.set_auto_clear(false);
  }

  /// Run the component through setup and drop the boot-time traffic from the log.
  void boot() {
    this->display.run_setup();
    ASSERT_FALSE(this->display.is_failed());
    this->bus.clear();
  }

  /// Run loop() until the pending transfer is complete. Returns the number of loop() calls.
  int drain(int max_iterations = 10000) {
    int n = 0;
    while (this->display.is_transfer_pending() && n < max_iterations) {
      this->display.loop();
      n++;
    }
    return n;
  }

  /// Set the cached loop clock that the component reads, as the main loop would.
  static void at(uint32_t now_ms) { LoopBlockingGuard dispatch{nullptr, nullptr, now_ms}; }

  /// True when what the panel shows equals what was drawn.
  bool panel_in_sync() { return this->bus.panel.ram == this->display.frame(); }

  FakeUnitLCDBus bus;
  TestableM5StackUnitLCD display;
};

inline std::vector<uint8_t> rgb565_bytes(uint8_t r, uint8_t g, uint8_t b, size_t count = 1) {
  const uint16_t c = display::ColorUtil::color_to_565(Color(r, g, b));
  std::vector<uint8_t> out;
  for (size_t i = 0; i < count; i++) {
    out.push_back(c >> 8);
    out.push_back(c & 0xFF);
  }
  return out;
}

/// Fill the first `rows` rows with deterministic noise. No pixel is black, so every one of them
/// differs from a freshly blanked panel.
inline void draw_noise(display::Display &it, uint32_t seed, int rows = PANEL_HEIGHT) {
  uint32_t s = seed * 2654435761u + 1;
  for (int y = 0; y < std::min(rows, it.get_height()); y++) {
    for (int x = 0; x < it.get_width(); x++) {
      s = s * 1664525u + 1013904223u;
      it.draw_pixel_at(x, y, Color((s >> 24) | 0x08, s >> 16, s >> 8));
    }
  }
}

}  // namespace esphome::m5stack_unit_lcd::testing
