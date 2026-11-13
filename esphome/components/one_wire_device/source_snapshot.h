#pragma once
#include <atomic>
#include "device_model.h"

namespace esphome::one_wire_device {
// Each supported model consumes one atomic source word. Capture before validating;
// validate and decode this snapshot, never a later atomic load.
struct SourceSnapshot {
  int32_t temperature;
  uint32_t pio;
  static SourceSnapshot IRAM_ATTR capture(const std::atomic<int32_t> &temperature, const std::atomic<uint32_t> &pio) {
    return {temperature.load(std::memory_order_relaxed), pio.load(std::memory_order_relaxed)};
  }
  bool IRAM_ATTR available(uint8_t family) const {
    return family == FAMILY_DS2413 ? (this->pio & 0x300) == 0x300
                                   : family == FAMILY_DS2401 || this->temperature != INT32_MIN;
  }
};
}  // namespace esphome::one_wire_device
