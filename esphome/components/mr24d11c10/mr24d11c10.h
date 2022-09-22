#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome {
namespace mr24d11c10 {
class MR24D11C10Sensor : public binary_sensor::BinarySensor, public Component, public uart::UARTDevice {
 public:
  MR24D11C10Sensor() = default;

  void set_req_key(bool x);

  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  void write_packet(uint8_t function_code, uint8_t address_code_1, uint8_t address_code_2, std::vector<uint8_t> &data);
  void write_packet(uint8_t function_code, uint8_t address_code_1, uint8_t address_code_2);

  void log_packet(std::vector<uint8_t> &packet);

  std::vector<uint8_t> packet;
  bool reading = false;
  uint16_t expected_length = 0;
};
}  // namespace mr24d11c10
}  // namespace esphome