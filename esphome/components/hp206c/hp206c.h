#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace hp206c {

const uint8_t HP206C_CHANNEL_OSR_4096 = 0b000;
const uint8_t HP206C_CHANNEL_OSR_2048 = 0b001;
const uint8_t HP206C_CHANNEL_OSR_1024 = 0b010;
const uint8_t HP206C_CHANNEL_OSR_512 = 0b011;
const uint8_t HP206C_CHANNEL_OSR_256 = 0b100;
const uint8_t HP206C_CHANNEL_OSR_128 = 0b101;

enum HP206CFilterMode {
  FILTER_OSR_128 = HP206C_CHANNEL_OSR_128,
  FILTER_OSR_256 = HP206C_CHANNEL_OSR_256,
  FILTER_OSR_512 = HP206C_CHANNEL_OSR_512,
  FILTER_OSR_1024 = HP206C_CHANNEL_OSR_1024,
  FILTER_OSR_2048 = HP206C_CHANNEL_OSR_2048,
  FILTER_OSR_4096 = HP206C_CHANNEL_OSR_4096
};

class HP206CComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_filter_mode(HP206CFilterMode mode) { filter_mode_ = mode; }
  void set_pressure(sensor::Sensor *pressure) { pressure_ = pressure; }

  void update() override;
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  bool start_conversion_(uint8_t mode);
  void read_data_();
  int conversion_time_(uint8_t mode);

  HP206CFilterMode filter_mode_{FILTER_OSR_2048};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *pressure_{nullptr};
};

}  // namespace hp206c
}  // namespace esphome
