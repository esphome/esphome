#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tcs34725 {

enum TCS34725IntegrationTime {
  TCS34725_INTEGRATION_TIME_2_4MS = 0xFF,
  TCS34725_INTEGRATION_TIME_24MS = 0xF6,
  TCS34725_INTEGRATION_TIME_50MS = 0xEB,
  TCS34725_INTEGRATION_TIME_101MS = 0xD5,
  TCS34725_INTEGRATION_TIME_154MS = 0xC0,
  TCS34725_INTEGRATION_TIME_700MS = 0x00,
};

enum TCS34725Gain {
  TCS34725_GAIN_1X = 0x00,
  TCS34725_GAIN_4X = 0x01,
  TCS34725_GAIN_16X = 0x02,
  TCS34725_GAIN_60X = 0x03,
};

class TCS34725Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_integration_time(TCS34725IntegrationTime integration_time);
  void set_gain(TCS34725Gain gain);

  void set_clear_sensor(sensor::Sensor *clear_sensor) { clear_sensor_ = clear_sensor; }
  void set_red_sensor(sensor::Sensor *red_sensor) { red_sensor_ = red_sensor; }
  void set_green_sensor(sensor::Sensor *green_sensor) { green_sensor_ = green_sensor; }
  void set_blue_sensor(sensor::Sensor *blue_sensor) { blue_sensor_ = blue_sensor; }
  void set_illuminance_sensor(sensor::Sensor *illuminance_sensor) { illuminance_sensor_ = illuminance_sensor; }
  void set_color_temperature_sensor(sensor::Sensor *color_temperature_sensor) {
    color_temperature_sensor_ = color_temperature_sensor;
  }

  void setup() override;
  float get_setup_priority() const override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *clear_sensor_{nullptr};
  sensor::Sensor *red_sensor_{nullptr};
  sensor::Sensor *green_sensor_{nullptr};
  sensor::Sensor *blue_sensor_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *color_temperature_sensor_{nullptr};
  TCS34725IntegrationTime integration_time_{TCS34725_INTEGRATION_TIME_2_4MS};
  TCS34725Gain gain_{TCS34725_GAIN_1X};
};

}  // namespace tcs34725
}  // namespace esphome
