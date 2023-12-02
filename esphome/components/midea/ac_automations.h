#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/automation.h"
#include "air_conditioner.h"

namespace esphome {
namespace midea {
namespace ac {

template<typename... Ts> class MideaActionBase : public Action<Ts...> {
 public:
  void set_parent(AirConditioner *parent) { this->parent_ = parent; }

 protected:
  AirConditioner *parent_;
};

template<typename... Ts> class FollowMeAction : public MideaActionBase<Ts...> {
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(bool, beeper)

  void play(Ts... x) override {
    this->parent_->do_follow_me(this->temperature_.value(x...), this->beeper_.value(x...));
  }
};

template<typename... Ts> class SwingStepAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_swing_step(); }
};

template<typename... Ts> class DisplayToggleAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_display_toggle(); }
};

template<typename... Ts> class BeeperOnAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_beeper_on(); }
};

template<typename... Ts> class BeeperOffAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_beeper_off(); }
};

template<typename... Ts> class PowerOnAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_power_on(); }
};

template<typename... Ts> class PowerOffAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_power_off(); }
};

template<typename... Ts> class PowerToggleAction : public MideaActionBase<Ts...> {
 public:
  void play(Ts... x) override { this->parent_->do_power_toggle(); }
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
