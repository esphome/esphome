#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>
#include <vector>

namespace esphome::rdm6300 {

class RDM6300BinarySensor;

class RDM6300Component final : public Component, public uart::UARTDevice {
 public:
  void loop() override;

  void register_card(RDM6300BinarySensor *obj) { this->cards_.push_back(obj); }
#ifdef RDM6300_ON_TAG_COUNT
  void add_on_tag_automation(Automation<uint32_t> *automation) { this->tag_automations_.add(automation); }
#endif

 protected:
  int8_t read_state_{-1};
  uint8_t buffer_[6]{};
  std::vector<RDM6300BinarySensor *> cards_;
#ifdef RDM6300_ON_TAG_COUNT
  AutomationList<RDM6300_ON_TAG_COUNT, uint32_t> tag_automations_;
#endif
  uint32_t last_id_{0};
};

class RDM6300BinarySensor final : public binary_sensor::BinarySensorInitiallyOff {
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

}  // namespace esphome::rdm6300
