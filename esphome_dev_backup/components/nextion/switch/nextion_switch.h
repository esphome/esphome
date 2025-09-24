#pragma once
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../nextion_component.h"
#include "../nextion_base.h"

namespace esphome {
namespace nextion {
class NextionSwitch;

class NextionSwitch : public NextionComponent, public switch_::Switch, public PollingComponent {
 public:
  NextionSwitch(NextionBase *nextion) { this->nextion_ = nextion; }

  void update() override;
  void update_component() override { this->update(); }
  void process_bool(const std::string &variable_name, bool on) override;

  void set_state(bool state) override { this->set_state(state, true, true); }
  void set_state(bool state, bool publish) override { this->set_state(state, publish, true); }
  void set_state(bool state, bool publish, bool send_to_nextion) override;

  void send_state_to_nextion() override { this->set_state(this->state, false, true); };
  NextionQueueType get_queue_type() override { return NextionQueueType::SWITCH; }
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {}
  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {
    this->set_state(state_value != 0, publish, send_to_nextion);
  }

 protected:
  void write_state(bool state) override;
};
}  // namespace nextion
}  // namespace esphome
