#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace somfyprogbutton {

class SomfyProgButton : public button::Button, public Component{
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_remote_id(uint32_t remote_id) { this->remote_id_ = remote_id; }
  void set_rolling_code_(globals::RestoringGlobalsComponent<int>* rollingcode){
    this->rollingCodeStorage_ = rollingcode;
  }

 protected:
  void press_action() override;
  uint32_t remote_id_;
  globals::RestoringGlobalsComponent<int> *rollingCodeStorage_;
};

}  // namespace somfyprogswitch
}  // namespace esphome
