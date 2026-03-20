#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "mitsubishi_cn105.h"

namespace esphome {
namespace mitsubishi_cn105 {

class MitsubishiCN105Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Climate() : hp_(*this) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }

 private:
  void apply_values_();

  MitsubishiCN105 hp_;
};

}  // namespace mitsubishi_cn105
}  // namespace esphome
