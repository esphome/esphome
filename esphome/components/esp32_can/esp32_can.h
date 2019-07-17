#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_can {

class ESP32Can : public canbus::Canbus {
 public:
  ESP32Can(){};

 protected:
  bool send_internal_(int can_id, uint8_t *data) override;
  bool setup_internal_() override;
  ERROR set_bitrate_(const CAN_SPEED canSpeed) override;
};
}  // namespace esp32_can
}  // namespace esphome