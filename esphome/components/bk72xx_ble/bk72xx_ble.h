#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE

#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::bk72xx_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
};

class BK72xxBLE final : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// Bring up the BDK BLE stack (one-time; the BDK has no teardown path).
  void enable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  /// Controller BLE address, least-significant octet first (BLE convention).
  void get_mac_lsb_first(uint8_t out[6]) const;

 protected:
  void resolve_mac_();

  uint8_t ble_mac_[6]{0};  // LSB-first (BLE convention)
  BLEComponentState state_{BLEComponentState::STATE_OFF};
  bool enable_on_boot_{false};
};

}  // namespace esphome::bk72xx_ble

#endif  // USE_BK72XX_BLE
