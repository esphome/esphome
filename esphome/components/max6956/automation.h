#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/max6956/max6956.h"

namespace esphome {
namespace max6956 {

template<typename... Ts> class SetCurrentGlobalAction : public Action<Ts...> {
 public:
  SetCurrentGlobalAction(MAX6956 *max6956) : max6956_(max6956) {}

  TEMPLATABLE_VALUE(uint8_t, brightness_global)

  void play(Ts... x) override {
    this->max6956_->set_brightness_global(this->brightness_global_.value(x...));
    this->max6956_->write_brightness_global();
  }

 protected:
  MAX6956 *max6956_;
};

template<typename... Ts> class SetCurrentModeAction : public Action<Ts...> {
 public:
  SetCurrentModeAction(MAX6956 *max6956) : max6956_(max6956) {}

  TEMPLATABLE_VALUE(max6956::MAX6956CURRENTMODE, brightness_mode)

  void play(Ts... x) override {
    this->max6956_->set_brightness_mode(this->brightness_mode_.value(x...));
    this->max6956_->write_brightness_mode();
  }

 protected:
  MAX6956 *max6956_;
};
}  // namespace max6956
}  // namespace esphome
