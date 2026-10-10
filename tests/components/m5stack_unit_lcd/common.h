#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

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
static constexpr size_t CHUNK = M5StackUnitLCD::MAX_PIXELS_PER_TX;

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

/// Fake Unit LCD on a fake I2C bus. Answers READ_ID and READ_BUFCOUNT, records every other
/// transaction so tests can assert on the exact byte stream, and can be told to fail writes.
class FakeUnitLCDBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    this->last_address = address;
    Transaction t;
    t.written.assign(write_buffer, write_buffer + write_count);
    t.read_len = read_count;
    this->log.push_back(t);

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
  uint8_t last_address{0};
  uint8_t id[4] = {0x77, 0x89, 0x01, 0x02};
  uint8_t bufcount{255};
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

}  // namespace esphome::m5stack_unit_lcd::testing
