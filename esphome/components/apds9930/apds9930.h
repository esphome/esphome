#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::apds9930 {

class APDS9930Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(illuminance)
  SUB_SENSOR(proximity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_led_drive(uint8_t level) { this->led_drive_ = level; }
  void set_proximity_gain(uint8_t gain) { this->proximity_gain_ = gain; }
  void set_ambient_gain(uint8_t gain) { this->ambient_gain_ = gain; }

 protected:
  bool write_reg_(uint8_t reg, uint8_t value);

  uint8_t led_drive_{0};
  uint8_t proximity_gain_{0};
  uint8_t ambient_gain_{0};
};

}  // namespace esphome::apds9930
