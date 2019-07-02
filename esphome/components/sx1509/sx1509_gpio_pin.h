#pragma once

#include "sx1509.h"

namespace esphome {
namespace sx1509 {

class SX1509Component;

class SX1509GPIOPin : public GPIOPin {
 public:
  SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted = false, uint16_t t_on = 0,
                uint16_t t_off = 0, uint8_t on_intensity = 255, uint8_t off_intensity = 0, uint16_t t_rise = 0,
                uint16_t t_fall = 0);
  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  SX1509Component *parent_;
  uint16_t t_on_ = {0};
  uint16_t t_off_ = {0};
  uint16_t t_rise_ = {0};
  uint16_t t_fall_ = {0};
  uint8_t on_intensity_ = {0};
  uint8_t off_intensity_ = {0};

  uint8_t calculate_led_t_register_(uint16_t ms);
  uint8_t calculate_slope_register_(uint16_t ms, uint8_t on_intensity, uint8_t off_intensity);
};

}  // namespace sx1509
}  // namespace esphome
