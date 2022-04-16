#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "scd4x.h"

namespace esphome {
namespace scd4x {

template<typename... Ts> class PerformForcedCalibrationAction : public Action<Ts...>, public Parented<SCD4XComponent> {
 public:
  void set_value_template(std::function<uint16_t(Ts...)> func) {
    this->value_func_ = func;
    this->static_ = false;
  }
  void set_value_static(uint16_t value) {
    this->value_static_ = value;
    this->static_ = true;
  }

  void play(Ts... x) override {
    if (this->static_) {
      this->parent_->perform_forced_calibration(value_static_);
    } else {
      auto val = this->value_func_(x...);
      this->parent_->perform_forced_calibration(val);
    }
  }

 protected:
  bool static_{false};
  std::function<uint16_t(Ts...)> value_func_{};
  uint16_t value_static_;
};

template<typename... Ts> class FactoryResetAction : public Action<Ts...> {
 public:
  explicit FactoryResetAction(SCD4XComponent *scd4x) : scd4x_(scd4x) {}

  void play(Ts... x) override { this->scd4x_->factory_reset(); }

 protected:
  SCD4XComponent *scd4x_;
};

}  // namespace scd4x
}  // namespace esphome
