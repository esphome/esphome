#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart {
class UARTBinarySensor : public uart::UARTDevice, public binary_sensor::BinarySensor, public Component {
 public:
  void set_data(std::vector<uint8_t> &&data) { this->data_ = std::move(data); }
  void set_data(std::initializer_list<uint8_t> data) { this->data_ = std::vector<uint8_t>(data); }
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void read_data_();
  std::vector<uint8_t> data_;
  static size_t max_data_size_;
  static std::vector<uint8_t> buffer_;
  bool first_entity_{};
};

}  // namespace esphome::uart
