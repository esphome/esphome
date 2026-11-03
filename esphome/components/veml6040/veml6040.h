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
  VEML6040Component(VEML6040IntegrationTime integration_time, float glass_attenuation_factor,
                    float color_temperature_offset)
      : glass_attenuation_factor_(glass_attenuation_factor),
        color_temperature_offset_(color_temperature_offset),
        integration_time_(integration_time) {}

  void setup() override;
  void dump_config() override;
  void update() override;

  SUB_SENSOR(red)
  SUB_SENSOR(green)
  SUB_SENSOR(blue)
  SUB_SENSOR(white)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(color_temperature)

 protected:
  uint32_t integration_time_ms_() const { return 40u << this->integration_time_; }

  const float glass_attenuation_factor_;
  const float color_temperature_offset_;
  const VEML6040IntegrationTime integration_time_;
  bool data_ready_{false};
};

}  // namespace esphome::veml6040
