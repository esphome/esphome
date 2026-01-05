#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "alpha3.h"

#ifdef USE_ESP32

namespace esphome {
namespace alpha3 {

enum Alpha3ButtonAction : uint8_t {
  ACTION_START = 0,
  ACTION_STOP = 1,
  ACTION_SETPOINT_UP = 2,
  ACTION_SETPOINT_DOWN = 3,
};

class Alpha3Button : public button::Button, public Component {
 public:
  void set_parent(Alpha3 *parent) { this->parent_ = parent; }
  void set_action(Alpha3ButtonAction action) { this->action_ = action; }

 protected:
  void press_action() override;
  Alpha3 *parent_{nullptr};
  Alpha3ButtonAction action_;
};

}  // namespace alpha3
}  // namespace esphome

#endif
