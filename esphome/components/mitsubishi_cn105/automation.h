#pragma once

#include "mitsubishi_cn105_component.h"

#include "esphome/core/automation.h"

namespace esphome::mitsubishi_cn105 {

template<typename... Ts>
class SetRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Component> {
 public:
  TEMPLATABLE_VALUE(float, temperature)

  void play(const Ts &...x) override { this->parent_->set_remote_temperature(this->temperature_.value(x...)); }
};

template<typename... Ts>
class ClearRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Component> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_remote_temperature(); }
};

}  // namespace esphome::mitsubishi_cn105
