#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "aic3204.h"

namespace esphome {
namespace aic3204 {

template<typename... Ts> class SetAutoMuteAction : public Action<Ts...> {
 public:
  explicit SetAutoMuteAction(AIC3204 *aic3204) : aic3204_(aic3204) {}

  TEMPLATABLE_VALUE(uint8_t, auto_mute_mode)

  void play(Ts... x) override { this->aic3204_->set_auto_mute_mode(this->auto_mute_mode_.value(x...)); }

 protected:
  AIC3204 *aic3204_;
};

}  // namespace aic3204
}  // namespace esphome
