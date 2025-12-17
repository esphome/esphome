#pragma once

#include <optional>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace adafruit_seesaw_soil {

class AdafruitSeesawSoil : public PollingComponent, public i2c::I2CDevice {
 public:
  struct Version {
    uint16_t pid{0};
    uint8_t year{0};
    uint8_t month{0};
    uint8_t day{0};
  };

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  std::optional<Version> get_version();
  std::optional<float> get_temperature_c();
  std::optional<uint16_t> get_moisture();

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  std::optional<Version> version_;
  unsigned read_count_{0};
  uint8_t hardware_type_{0};
};

}  // namespace adafruit_seesaw_soil
}  // namespace esphome
