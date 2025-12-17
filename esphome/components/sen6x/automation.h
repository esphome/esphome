#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome {
namespace sen6x {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(const Ts &...x) override { this->sen6x_->start_fan_cleaning(); }

 protected:
  SEN6XComponent *sen6x_;
};

}  // namespace sen6x
}  // namespace esphome
