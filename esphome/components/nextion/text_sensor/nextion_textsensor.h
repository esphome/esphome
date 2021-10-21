#pragma once
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../nextion_component.h"
#include "../nextion_base.h"

namespace esphome {
namespace nextion {
class NextionTextSensor;

class NextionTextSensor : public NextionComponent, public text_sensor::TextSensor, public PollingComponent {
 public:
  NextionTextSensor(NextionBase *nextion) { this->nextion_ = nextion; }
  void update() override;
  void update_component() override { this->update(); }
  void on_state_changed(const std::string &state);

  void process_text(const std::string &variable_name, const std::string &text_value) override;

  void set_state(const std::string &state, bool publish) override { this->set_state(state, publish, true); }
  void set_state(const std::string &state) override { this->set_state(state, true, true); }
  void set_state(const std::string &state, bool publish, bool send_to_nextion) override;

  void send_state_to_nextion() override { this->set_state(this->state, false, true); };
  NextionQueueType get_queue_type() override { return NextionQueueType::TEXT_SENSOR; }
  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {}
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {
    this->set_state(state_value, publish, send_to_nextion);
  }
};
}  // namespace nextion
}  // namespace esphome
