#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace esp32_can {
class ESP32Can : public canbus::Canbus {
 public:
  ESP32Can() {};
  ESP32Can(const std::string &name){};
  void send(int can_id, uint8_t *data){};
};
}  // namespace esp32_can
}  // namespace esphome