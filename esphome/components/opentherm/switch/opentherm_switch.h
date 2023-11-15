#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace opentherm {

#define SUB_OPENTHERM_SWITCH(name) \
 protected: \
  opentherm::OpenThermSwitch *name##_switch_{nullptr}; \
\
 public: \
  void set_##name##_switch(opentherm::OpenThermSwitch *s) { this->name##_switch_ = s; }

class OpenThermSwitch : public Component, public switch_::Switch {
 protected:
  void write_state(bool state) override;
};

}  // namespace opentherm
}  // namespace esphome
