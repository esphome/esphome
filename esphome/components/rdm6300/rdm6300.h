#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace rdm6300 {

class RDM6300BinarySensor;
class RDM6300Trigger;

class RDM6300Component : public Component, public uart::UARTDevice {
 public:
  void loop() override;

  void register_card(RDM6300BinarySensor *obj) { this->cards_.push_back(obj); }
  void register_trigger(RDM6300Trigger *trig) { this->triggers_.push_back(trig); }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  int8_t read_state_{-1};
  uint8_t buffer_[6]{};
  std::vector<RDM6300BinarySensor *> cards_;
  std::vector<RDM6300Trigger *> triggers_;
  uint32_t last_id_{0};
};

class RDM6300BinarySensor : public binary_sensor::BinarySensorInitiallyOff {
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

class RDM6300Trigger : public Trigger<uint32_t> {
 public:
  void process(uint32_t uid) { this->trigger(uid); }
};

}  // namespace rdm6300
}  // namespace esphome
