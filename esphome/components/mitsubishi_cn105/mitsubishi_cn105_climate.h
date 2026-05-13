#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Climate() : hp_(*this) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }
  void set_current_temperature_min_interval(uint32_t ms) { this->hp_.set_room_temperature_min_interval(ms); }

  void set_remote_temperature(float temperature) { this->hp_.set_remote_temperature(temperature); }
  void clear_remote_temperature() { this->hp_.clear_remote_temperature(); }

 protected:
  void apply_values_();

  MitsubishiCN105 hp_;
};

template<typename... Ts>
class SetRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Climate> {
 public:
  TEMPLATABLE_VALUE(float, temperature)

  void play(const Ts &...x) override { this->parent_->set_remote_temperature(this->temperature_.value(x...)); }
};

template<typename... Ts>
class ClearRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Climate> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_remote_temperature(); }
};

}  // namespace esphome::mitsubishi_cn105
