#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace diyless_opentherm {

class CustomSwitch : public Component, public switch_::Switch {
  protected:
    void write_state(bool state) override;
};

} // namespace diyless_opentherm
} // namespace esphome
