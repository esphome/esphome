#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstdint>

namespace esphome::ble_client {

/// Reconnect backoff after repeated connect/discovery failures, shared by
/// both engines. 256 ms ticks in a uint16_t keep it 4 bytes; the ~4.7 h tick
/// wrap can at worst reinstate one stale hold-off of a minute.
class ConnectBackoff {
 public:
  bool holding_off() const {
    return this->failures_ != 0 && static_cast<uint16_t>(now() - this->start_) < this->failures_ * STEP_TICKS;
  }
  void register_failure(const char *address_str) {
    if (this->failures_ < MAX_STEPS)
      this->failures_++;
    this->start_ = now();
    esph_log_w("ble_client", "[%s] Holding off reconnect for %u s", address_str, this->failures_ * 10u);
  }
  void reset() { this->failures_ = 0; }

 private:
  // ~10 s per consecutive failure, capped so a flapping peer retries within
  // a minute at worst.
  static constexpr uint16_t STEP_TICKS = 40;  // x 256 ms
  static constexpr uint8_t MAX_STEPS = 6;
  static uint16_t now() { return static_cast<uint16_t>(millis() >> 8); }

  uint16_t start_{0};
  uint8_t failures_{0};
};

}  // namespace esphome::ble_client

#endif  // USE_BLE_GATT_CLIENT
