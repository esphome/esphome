#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>
#include <vector>

namespace esphome {
namespace ws18x0_uart {

class WS18x0UARTBinarySensor;
class WS18x0UARTTrigger;

class WS18x0UARTComponent : public Component, public uart::UARTDevice {
 public:
  void loop() override;

  void register_card(WS18x0UARTBinarySensor *obj) { this->cards_.push_back(obj); }
  void register_trigger(WS18x0UARTTrigger *trig) { this->triggers_.push_back(trig); }

  float get_setup_priority() const override { return setup_priority::DATA; }

  uint32_t id() { return (uint32_t) raw_; }
  uint64_t raw() { return raw_; }

 protected:
  int8_t read_state_{-1};
  uint64_t raw_{0};
  uint64_t last_raw_{0};
  std::vector<WS18x0UARTBinarySensor *> cards_;
  std::vector<WS18x0UARTTrigger *> triggers_;
};

class WS18x0UARTBinarySensor : public binary_sensor::BinarySensorInitiallyOff {
 public:
  void set_id(uint32_t id) { id_ = id; }

  bool process(uint32_t id) {
    if (this->id_ == id) {
      this->publish_state(true);
      yield();
      this->publish_state(false);
      return true;
    }
    return false;
  }

 protected:
  uint32_t id_;
};

class WS18x0UARTTrigger : public Trigger<uint32_t> {
 public:
  void process(uint32_t uid) { this->trigger(uid); }
};

}  // namespace ws18x0_uart
}  // namespace esphome
