#pragma once

#include <cstdint>

namespace esphome::tuya {

// Tracks monotonic UART RX error telemetry without coupling Tuya to a specific
// implementation class. Wrong-baud noise while probing is not an MCU fault.
class TuyaLowPowerUartErrors {
 public:
  void reset(uint32_t total) { this->observed_ = total; }
  bool observe(uint32_t total, bool autobaud_searching) {
    const bool changed = total != this->observed_;
    this->observed_ = total;
    return changed && !autobaud_searching;
  }
  bool unchanged(uint32_t total) const { return total == this->observed_; }

 private:
  uint32_t observed_{0};
};

}  // namespace esphome::tuya
