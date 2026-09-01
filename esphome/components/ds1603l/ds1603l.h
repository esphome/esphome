#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::ds1603l {

class DS1603L : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  uint8_t rx_buffer_[4];  // Buffer for incoming data

  bool initialized_{false};

  void parse_data_();  // Parse received data
};

}  // namespace esphome::ds1603l
