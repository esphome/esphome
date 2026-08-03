#pragma once

#include "esphome/core/defines.h"

#ifdef USE_LN882H_BLE

#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::ln882h_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
};

class LN882HBLE final : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// Bring up the LN882H BLE stack (one-time; the SDK has no teardown path).
  /// Requires setup() to have run: rw_init() is handed the address resolved
  /// there. Blocks ~120 ms across the SDK's settle points, so calling it from
  /// loop() (enable_on_boot: false) trips the loop-blocking warning once.
  void enable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  /// Controller BLE address in the SDK's ln_bd_addr_t order: least-significant
  /// octet first (the BLE/HCI convention). Reverse for printable form — the
  /// byte order is in the name so platform analogs cannot be confused
  /// (the bk72xx sibling exposes the same accessor).
  void get_mac_lsb_first(uint8_t out[6]) const;

 protected:
  void resolve_mac_();

  uint8_t ble_mac_[6]{0};  // controller (LSB-first) order, as ln_bd_addr_t stores it
  BLEComponentState state_{BLEComponentState::STATE_OFF};
  bool enable_on_boot_{false};
};

}  // namespace esphome::ln882h_ble

#endif  // USE_LN882H_BLE
