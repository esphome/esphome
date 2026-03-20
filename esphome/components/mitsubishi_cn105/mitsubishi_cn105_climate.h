#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "mitsubishi_cn105.h"

namespace esphome {
namespace mitsubishi_cn105 {

struct UARTTransport : MitsubishiCN105Transport {
  explicit UARTTransport(uart::UARTDevice &device) : device_(device) {}

  size_t available() override { return this->device_.available(); }
  void flush() override { this->device_.flush(); }
  bool read_byte(uint8_t *data) override { return this->device_.read_byte(data); }
  void write_array(const uint8_t *data, size_t len) override { this->device_.write_array(data, len); }

 private:
  uart::UARTDevice &device_;
};

class MitsubishiCN105Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Climate() : transport_(*this), hp_(transport_) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }

 private:
  void apply_values_();

  UARTTransport transport_;
  MitsubishiCN105 hp_;

  uint8_t failed_connect_attempts_{0};
};

}  // namespace mitsubishi_cn105
}  // namespace esphome
