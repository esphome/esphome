#pragma once
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../nextion_component.h"
#include "../nextion_base.h"

namespace esphome {
namespace nextion {
class NextionBinarySensor;

class NextionBinarySensor : public NextionComponent,
                            public binary_sensor::BinarySensorInitiallyOff,
                            public PollingComponent {
 public:
  NextionBinarySensor(NextionBase *nextion) { this->nextion_ = nextion; }

  void update_component() override { this->update(); }
  void update() override;
  void send_state_to_nextion() override { this->set_state(this->state, false); };
  void process_bool(const std::string &variable_name, bool state) override;
  void process_touch(uint8_t page_id, uint8_t component_id, bool state) override;

  // Set the components page id for Nextion Touch Component
  void set_page_id(uint8_t page_id) { page_id_ = page_id; }
  // Set the components component id for Nextion Touch Component
  void set_component_id(uint8_t component_id) { component_id_ = component_id; }

  void set_state(bool state) override { this->set_state(state, true, true); }
  void set_state(bool state, bool publish) override { this->set_state(state, publish, true); }
  void set_state(bool state, bool publish, bool send_to_nextion) override;

  NextionQueueType get_queue_type() override { return NextionQueueType::BINARY_SENSOR; }
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {}
  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {
    this->set_state(state_value != 0, publish, send_to_nextion);
  }

 protected:
  uint8_t page_id_;
};
}  // namespace nextion
}  // namespace esphome
