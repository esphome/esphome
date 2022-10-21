#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace opentherm {

class CustomSwitch : public Component, public switch_::Switch {
 protected:
  void write_state(bool state) override;
};

}  // namespace opentherm
}  // namespace esphome
