#pragma once

#ifdef USE_ZEPHYR

#include <cstdint>

#include "esphome/core/component.h"

extern "C" {
#include <zephyr/device.h>
#include <zephyr/drivers/serial/uart_emul.h>
}

namespace esphome::zephyr {

/// Watches TX bytes written by the ESPHome device and answers back with canned
/// responses configured via `uart: emulation: responses:`, keyed by trigger byte
/// pattern. Also exposes `push_rx()` so an unrelated automation can inject bytes
/// into the device's RX path directly (unprompted, not tied to a trigger match).
class ZephyrUartEmulator : public Component {
 public:
  ZephyrUartEmulator(const device *uart_dev, size_t trigger_count) : uart_dev_(uart_dev) {
    this->triggers_.init(trigger_count);
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS + 1.0f; }

  /// Register a new trigger pattern; subsequent add_response() calls attach to it.
  void add_trigger(const uint8_t *pattern, uint8_t pattern_len, size_t response_count);
  /// Attach a (possibly cycling) response entry to the most-recently-added trigger.
  void add_response(const uint8_t *data, uint16_t len);
  /// Inject bytes directly into the device's RX path, independent of trigger matching.
  void push_rx(const uint8_t *data, size_t len);

 protected:
  struct ResponseEntry {
    const uint8_t *data{nullptr};
    uint16_t len{0};
  };
  struct TriggerEntry {
    const uint8_t *pattern{nullptr};
    uint8_t pattern_len{0};
    uint8_t match_pos{0};
    // KMP failure function for `pattern` -- lets feed_byte_() backtrack to the longest
    // proper prefix that's also a suffix of what's matched so far, instead of dropping
    // back to 0 and missing self-overlapping matches (e.g. pattern "ABAB" in "AABABX").
    FixedVector<uint8_t> failure;
    FixedVector<ResponseEntry> responses;
    uint8_t cycle_index{0};
  };

  static void tx_data_ready_s(const device *dev, size_t size, void *user_data);
  void tx_data_ready_(size_t size);
  void feed_byte_(uint8_t byte);

  const device *uart_dev_;
  FixedVector<TriggerEntry> triggers_;
};

}  // namespace esphome::zephyr

#endif
