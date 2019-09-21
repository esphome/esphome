#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_can {

class ESP32Can : public canbus::Canbus {
 public:
  ESP32Can(){};

 protected:
  bool send_internal_(int can_id, uint8_t *data);
  bool setup_internal_();
  canbus::Error set_bitrate_(const canbus::CanSpeed can_speed);
};

}  // namespace esp32_can
}  // namespace esphome
