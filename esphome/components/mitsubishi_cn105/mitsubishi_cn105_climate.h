#pragma once

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

}  // namespace esphome::mitsubishi_cn105
