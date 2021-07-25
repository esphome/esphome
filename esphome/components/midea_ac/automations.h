#pragma once
#include "esphome/core/automation.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

template<typename... Ts> class MideaActionBase : public Action<Ts...> {
 public:
  void set_parent(MideaAC *parent) { this->parent_ = parent; }

 protected:
  MideaAC *parent_;
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

}  // namespace midea_ac
}  // namespace esphome
