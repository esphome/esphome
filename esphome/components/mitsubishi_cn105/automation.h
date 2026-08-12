#pragma once

#include "mitsubishi_cn105_component.h"

#include "esphome/core/automation.h"

#include <type_traits>

namespace esphome::mitsubishi_cn105 {

template<typename... Ts>
class SetRemoteTemperatureAction final : public Action<Ts...>, public Parented<MitsubishiCN105Component> {
 public:
  TEMPLATABLE_VALUE(float, temperature)

  void play(const Ts &...x) override { this->parent_->set_remote_temperature(this->temperature_.value(x...)); }
};

template<typename... Ts>
class ClearRemoteTemperatureAction final : public Action<Ts...>, public Parented<MitsubishiCN105Component> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_remote_temperature(); }
};

template<typename... Ts> class VaneControlAction final : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(VaneCall &, const std::remove_cvref_t<Ts> &...);

  VaneControlAction(MitsubishiCN105Component *parent, ApplyFn apply) : parent_(parent), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->parent_->make_vane_call();
    this->apply_(call, x...);
    call.perform();
  }

 protected:
  MitsubishiCN105Component *parent_;
  ApplyFn apply_;
};

}  // namespace esphome::mitsubishi_cn105
