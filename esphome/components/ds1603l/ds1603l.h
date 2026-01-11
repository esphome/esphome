#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ds1603l {

class Ds1603l : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_liquid_level_sensor(sensor::Sensor *liquid_level_sensor) { liquid_level_sensor_ = liquid_level_sensor; }
  void set_liquid_volume_sensor(sensor::Sensor *liquid_volume_sensor) { liquid_volume_sensor_ = liquid_volume_sensor; }
  void set_min_volume(float min_volume) { this->min_volume_ = min_volume; }
  void set_max_volume(float max_volume) { this->max_volume_ = max_volume; }
  void set_min_level(float min_level) { this->min_level_ = min_level; }
  void set_max_level(float max_level) { this->max_level_ = max_level; }

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *liquid_level_sensor_{nullptr};
  sensor::Sensor *liquid_volume_sensor_{nullptr};
  float min_volume_;
  float max_volume_;
  float min_level_;
  float max_level_;

 private:
  uint8_t rx_buffer_[4];  // Buffer for incoming data

  void parse_data_();  // Parse received data
};

}  // namespace ds1603l
}  // namespace esphome
