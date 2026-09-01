#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::ds1603l {

class DS1603L : public sensor::Sensor, public Component, public uart::UARTDevice {
  SUB_SENSOR(liquid_level);
  SUB_SENSOR(liquid_volume);
  SUB_SENSOR(percentage);

 public:
  void set_min_volume(float min_volume) { this->min_volume_ = min_volume; }
  void set_max_volume(float max_volume) { this->max_volume_ = max_volume; }
  void set_min_level(float min_level) { this->min_level_ = min_level; }
  void set_max_level(float max_level) { this->max_level_ = max_level; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  float min_volume_;
  float max_volume_;
  float min_level_;
  float max_level_;

  uint8_t rx_buffer_[4];  // Buffer for incoming data

  bool initialized_{false};

  void parse_data_();  // Parse received data
};

}  // namespace esphome::ds1603l
