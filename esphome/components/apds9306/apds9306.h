#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace apds9306 {

  class APDS9306 : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
  public:
    void setup() override;
    void dump_config() override;
    void update() override;

    void set_bit_width(uint8_t measurement_bit_width) {this->bit_width_ = measurement_bit_width;}
    void set_measurement_rate(uint8_t measurement_rate) {this->measurement_rate_ = measurement_rate;}
    void set_ambient_light_gain(uint8_t ambient_light_gain) {this->gain_ = ambient_light_gain;}

  protected:
    enum ErrorCode {
      NONE = 0,
      COMMUNICATION_FAILED,
      WRONG_ID,
    } error_code_{NONE};

    uint8_t bit_width_;
    uint8_t bit_width_val_;
    uint8_t measurement_rate_;
    uint16_t rate_val_;
    uint8_t gain_;
    uint8_t gain_val_;
  };

} // namespace apds9306
} // namespace esphome
