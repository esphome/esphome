#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen5x.h"

namespace esphome {
namespace sen5x {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN5XComponent *sen5x) : sen5x_(sen5x) {}

  void play(Ts... x) override { this->sen5x_->start_fan_cleaning(); }

 protected:
  SEN5XComponent *sen5x_;
};

}  // namespace sen5x
}  // namespace esphome
