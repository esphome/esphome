#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

struct TemperatureMapping {
  float (*to_mitsubishi)(float);
  float (*from_mitsubishi)(float);

  static TemperatureMapping identity();
  static TemperatureMapping fahrenheit();
};

class MitsubishiCN105Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Climate() : hp_(*this) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }
  void set_use_fahrenheit_conversion(bool value) {
    this->temperature_mapping_ = value ? TemperatureMapping::fahrenheit() : TemperatureMapping::identity();
  }

 private:
  void apply_values_();

  MitsubishiCN105 hp_;
  TemperatureMapping temperature_mapping_{TemperatureMapping::identity()};
};

}  // namespace esphome::mitsubishi_cn105
