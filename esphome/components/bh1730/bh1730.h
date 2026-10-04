#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bh1730 {

enum BH1730Gain : uint8_t {
  BH1730_GAIN_X1 = 1,
  BH1730_GAIN_X2 = 2,
  BH1730_GAIN_X64 = 64,
  BH1730_GAIN_X128 = 128,
};

/// This class implements support for the i2c-based BH1730 ambient light sensor.
class BH1730Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_lux_light_sensor(sensor::Sensor *sensor) { lux_light_sensor = sensor; }
  void set_visible_light_sensor(sensor::Sensor *sensor) { visible_light_sensor = sensor; }
  void set_infrared_light_sensor(sensor::Sensor *sensor) { infrared_light_sensor = sensor; }

  /// Set the integration time, 0 is manual integration, 1-255, default 0xDA
  // void set_integration_time(uint8_t integration_time);
  void set_gain(BH1730Gain gain) { this->gain = gain; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  sensor::Sensor *lux_light_sensor{nullptr};
  sensor::Sensor *visible_light_sensor{nullptr};
  sensor::Sensor *infrared_light_sensor{nullptr};
  BH1730Gain gain{BH1730Gain::BH1730_GAIN_X1};
};

}  // namespace bh1730
}  // namespace esphome
