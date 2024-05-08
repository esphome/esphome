#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace apds9306 {

  class APDS9306 : public PollingComponent, public i2c::I2CDevice {
    SUB_SENSOR(light_level)

  public:
    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override;
    void update() override;

    void set_bit_width(uint8_t measurement_bit_width) {this->bit_width_ = measurement_bit_width;}
    void set_measurement_rate(uint8_t measurement_rate) {this->measurement_rate_ = measurement_rate;}
    void set_ambiant_light_gain(uint8_t ambiant_light_gain) {this->gain_ = ambiant_light_gain;}

  protected:
    enum ErrorCode {
      NONE = 0,
      COMMUNICATION_FAILED,
      WRONG_ID,
    } error_code_{NONE};

    uint8_t bit_width_;
    uint8_t measurement_rate_;
    uint8_t gain_;

    void get_light_level_(uint8_t status);
  };

} // namespace apds9306
} // namespace esphome
