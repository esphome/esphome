#pragma once

#include "esphome/components/switch/switch.h"
#include "../airton.h"

namespace esphome::airton {

// One of the unit's on/off settings; the parent sends the state and publishes it back
class AirtonSwitch : public switch_::Switch, public Parented<AirtonClimate> {
 public:
  explicit AirtonSwitch(AirtonSetting setting) : setting_(setting) {}

 protected:
  void write_state(bool state) override { this->parent_->set_setting(this->setting_, state); }

  AirtonSetting setting_;
};

}  // namespace esphome::airton
