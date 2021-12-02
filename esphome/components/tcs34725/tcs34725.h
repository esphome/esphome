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
  TCS34725_INTEGRATION_TIME_120MS = 0xCE,
  TCS34725_INTEGRATION_TIME_154MS = 0xC0,
  TCS34725_INTEGRATION_TIME_180MS = 0xB5,
  TCS34725_INTEGRATION_TIME_199MS = 0xAD,
  TCS34725_INTEGRATION_TIME_240MS = 0x9C,
  TCS34725_INTEGRATION_TIME_300MS = 0x83,
  TCS34725_INTEGRATION_TIME_360MS = 0x6A,
  TCS34725_INTEGRATION_TIME_401MS = 0x59,
  TCS34725_INTEGRATION_TIME_420MS = 0x51,
  TCS34725_INTEGRATION_TIME_480MS = 0x38,
  TCS34725_INTEGRATION_TIME_499MS = 0x30,
  TCS34725_INTEGRATION_TIME_540MS = 0x1F,
  TCS34725_INTEGRATION_TIME_600MS = 0x06,
  TCS34725_INTEGRATION_TIME_614MS = 0x00,
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
  void set_glass_attenuation_factor(float ga);

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
  float integration_time_{2.4};
  float gain_{1.0};
  float glass_attenuation_{1.0};
  float illuminance_;
  float color_temperature_;

 private:
  void calculate_temperature_and_lux_(uint16_t r, uint16_t g, uint16_t b, uint16_t c);
  uint8_t integration_reg_{TCS34725_INTEGRATION_TIME_2_4MS};
  uint8_t gain_reg_{TCS34725_GAIN_1X};
};

}  // namespace tcs34725
}  // namespace esphome
