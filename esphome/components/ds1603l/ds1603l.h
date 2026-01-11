#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ds1603l {

class Ds1603l : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_ds1603l_liquid_level_sensor(sensor::Sensor *ds1603l_liquid_level_sensor) { ds1603l_liquid_level_sensor_ = ds1603l_liquid_level_sensor; }
  void set_ds1603l_liquid_volume_sensor(sensor::Sensor *ds1603l_liquid_volume_sensor) { ds1603l_liquid_volume_sensor_ = ds1603l_liquid_volume_sensor; }
  void set_ds1603l_min_volume(float ds1603l_min_volume) { this->ds1603l_min_volume_ = ds1603l_min_volume; }
  void set_ds1603l_max_volume(float ds1603l_max_volume) { this->ds1603l_max_volume_ = ds1603l_max_volume; }
  void set_ds1603l_min_level(float ds1603l_min_level) { this->ds1603l_min_level_ = ds1603l_min_level; }
  void set_ds1603l_max_level(float ds1603l_max_level) { this->ds1603l_max_level_ = ds1603l_max_level; }

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *ds1603l_liquid_level_sensor_{nullptr};
  sensor::Sensor *ds1603l_liquid_volume_sensor_{nullptr};
  float ds1603l_min_volume_;
  float ds1603l_max_volume_;
  float ds1603l_min_level_;
  float ds1603l_max_level_;

 private:
  uint8_t rx_buffer_[4];  // Buffer for incoming data

  void parse_data_();  // Parse received data
};

}  // namespace ds1603l
}  // namespace esphome
