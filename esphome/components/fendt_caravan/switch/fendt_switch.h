#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/fendt_caravan_hub_base.h"
#include "esphome/components/switch/switch.h"

namespace esphome::fendt_caravan {

class FendtSwitch : public switch_::Switch, public Parented<FendtCaravanHubBase> {
 public:
 protected:
  void write_state(bool state) { this->state_callback_.call(state); };

 private:
};

}  // namespace esphome::fendt_caravan
#endif
