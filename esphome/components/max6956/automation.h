#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/max6956/max6956.h"

namespace esphome {
namespace max6956 {


template<typename... Ts> class SetCurrentGlobalAction : public Action<Ts...> {
 public:
  SetCurrentGlobalAction(MAX6956 *max6956) : max6956_(max6956) {}

  TEMPLATABLE_VALUE(float, brightness_global)

  void play(Ts... x) override { this->max6956_->set_brightness_global(16 * this->brightness_global_.value(x...)); }

 protected:
  MAX6956 *max6956_;
};

}  // namespace max6956
}  // namespace esphome
