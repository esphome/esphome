#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstdint>

namespace esphome::ble_client {

/// Reconnect backoff after repeated connect/discovery failures, shared by
/// both engines (wrap-safe start+failures pair; the duration is derived).
class ConnectBackoff {
 public:
  bool holding_off() const { return this->failures_ != 0 && millis() - this->start_ < this->failures_ * STEP_MS; }
  void register_failure(const char *address_str) {
    if (this->failures_ < MAX_STEPS)
      this->failures_++;
    this->start_ = millis();
    esph_log_w("ble_client", "[%s] Holding off reconnect for %u s", address_str, this->failures_ * (STEP_MS / 1000));
  }
  void reset() { this->failures_ = 0; }

 private:
  // Capped so a flapping peer retries within a minute at worst.
  static constexpr uint32_t STEP_MS = 10000;
  static constexpr uint8_t MAX_STEPS = 6;

  uint32_t start_{0};
  uint8_t failures_{0};
};

}  // namespace esphome::ble_client

#endif  // USE_BLE_GATT_CLIENT
