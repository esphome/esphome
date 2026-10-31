#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/helpers.h"

namespace esphome::modbus_monitor {

/** One register a platform wants from the bus.
 *
 * Kept abstract so the decoder carries no dependency on any platform: a sensor is one
 * implementation, and a text sensor or binary sensor would be others.
 */
class MonitorItem {
 public:
  MonitorItem(uint8_t address, uint16_t reg, uint8_t register_count)
      : reg(reg), address(address), register_count(register_count) {}
  virtual ~MonitorItem() = default;

  /// The item's register_count registers, big-endian as on the wire, from an exchange that covered all of
  /// them: a read's reply, or a write the device confirmed.
  virtual void parse_and_publish(std::span<const uint8_t> data) = 0;

  uint16_t reg;
  uint8_t address;
  uint8_t register_count;
};

/** Publishes items from the exchanges one hub sees.
 *
 * One per hub, fed by a single on_response callback however many items it holds. The hub pairs
 * requests with replies and knows nothing about what they mean; this is where that meaning lives.
 */
class RegisterDecoder {
 public:
  /// Sized once, to the number of add_item() calls that follow. An item past that is dropped.
  void init_items(size_t count) { this->items_.init(count); }
  void add_item(MonitorItem *item) { this->items_.push_back(item); }
  void on_response(uint8_t address, std::span<const uint8_t> request, std::span<const uint8_t> response);

 protected:
  FixedVector<MonitorItem *> items_;
};

}  // namespace esphome::modbus_monitor
