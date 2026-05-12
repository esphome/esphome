#pragma once

#include "esphome/core/defines.h"  // Must be included before conditional includes

#ifdef USE_RP2040_BLE

#include "esphome/core/component.h"

#include <btstack.h>

namespace esphome::rp2040_ble {

enum class BLEComponentState : uint8_t {
  OFF = 0,
  ENABLING,
  ACTIVE,
  DISABLING,
  DISABLED,
};

class RP2040BLE : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void enable();
  void disable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

 protected:
  static void packet_handler_(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size);

  btstack_packet_callback_registration_t hci_event_callback_registration_{};
  btstack_packet_callback_registration_t sm_event_callback_registration_{};

  BLEComponentState state_{BLEComponentState::OFF};
  bool enable_on_boot_{true};
  bool btstack_initialized_{false};
  bool active_logged_{false};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern RP2040BLE *global_ble;

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE
