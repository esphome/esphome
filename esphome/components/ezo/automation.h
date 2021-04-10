#pragma once
#include "esphome/core/automation.h"
#include "ezo.h"

namespace esphome {
namespace ezo {

class LedTrigger : public Trigger<bool> {
 public:
  explicit LedTrigger(EZOSensor *ezo) {
    ezo->add_led_state_callback([this](bool value) { this->trigger(value); });
  }
};

}  // namespace ezo
}  // namespace esphome
