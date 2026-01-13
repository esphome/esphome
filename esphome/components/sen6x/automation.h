#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome::sen6x {

template<typename... Ts> class StartFanAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start_fan_cleaning(); }
};

}  // namespace esphome::sen6x
