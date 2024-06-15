// Based on this datasheet:
// https://www.mouser.ca/datasheet/2/678/AVGO_S_A0002854364_1-2574547.pdf

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace apds9306 {

class APDS9306 : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void dump_config() override;
  void update() override;
  void set_bit_width(uint8_t bit_width) { this->bit_width_ = bit_width; }
  void set_measurement_rate(uint8_t measurement_rate) { this->measurement_rate_ = measurement_rate; }
  void set_ambient_light_gain(uint8_t gain) { this->gain_ = gain; }

 protected:
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};

  uint8_t bit_width_;
  uint8_t bit_width_val_[6] = {20, 19, 18, 17, 16, 13};
  uint8_t measurement_rate_;
  float measurement_time_[7] = {25, 50, 100, 200, 500, 1000, 2000};
  uint8_t gain_;
  float gain_val_[5] = {1, 3, 6, 9, 18};
};

}  // namespace apds9306
}  // namespace esphome
