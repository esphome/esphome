#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::veml6040 {

// Register values for the IT bits of the configuration register, taken from the VEML6040 datasheet.
// Each step doubles the integration time and halves the lux per count.
enum VEML6040IntegrationTime : uint8_t {
  VEML6040_INTEGRATION_TIME_40MS = 0b000,
  VEML6040_INTEGRATION_TIME_80MS = 0b001,
  VEML6040_INTEGRATION_TIME_160MS = 0b010,
  VEML6040_INTEGRATION_TIME_320MS = 0b011,
  VEML6040_INTEGRATION_TIME_640MS = 0b100,
  VEML6040_INTEGRATION_TIME_1280MS = 0b101,
};

class VEML6040Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  VEML6040Component(VEML6040IntegrationTime integration_time, float glass_attenuation_factor)
      : glass_attenuation_factor_(glass_attenuation_factor), integration_time_(integration_time) {}

  void setup() override;
  void dump_config() override;
  void update() override;

  void set_red_sensor(sensor::Sensor *sensor) { this->red_sensor_ = sensor; }
  void set_green_sensor(sensor::Sensor *sensor) { this->green_sensor_ = sensor; }
  void set_blue_sensor(sensor::Sensor *sensor) { this->blue_sensor_ = sensor; }
  void set_white_sensor(sensor::Sensor *sensor) { this->white_sensor_ = sensor; }
  void set_illuminance_sensor(sensor::Sensor *sensor) { this->illuminance_sensor_ = sensor; }
  void set_color_temperature_sensor(sensor::Sensor *sensor) { this->color_temperature_sensor_ = sensor; }

 protected:
  bool read_channel_(uint8_t reg, uint16_t &counts);
  uint32_t integration_time_ms_() const { return 40u << this->integration_time_; }

  sensor::Sensor *red_sensor_{nullptr};
  sensor::Sensor *green_sensor_{nullptr};
  sensor::Sensor *blue_sensor_{nullptr};
  sensor::Sensor *white_sensor_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *color_temperature_sensor_{nullptr};
  const float glass_attenuation_factor_;
  const VEML6040IntegrationTime integration_time_;
  bool data_ready_{false};
};

}  // namespace esphome::veml6040
