#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_SWITCH)

#include "ecocomfort2_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome::ecocomfort2 {

class Ecocomfort2Switch final : public switch_::Switch, public Parented<Ecocomfort2Hub> {
 public:
  explicit Ecocomfort2Switch(SwitchSetting setting) : setting_(setting) {}

 protected:
  void write_state(bool state) override {
    if (this->parent_->write_switch(this->setting_, state))
      this->publish_state(state);
  }

  const SwitchSetting setting_;
};

}  // namespace esphome::ecocomfort2

#endif
