#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

#include <TM1651.h>

namespace esphome {
namespace tm1651 {

class TM1651Display : public Component {

 public:
  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { dio_pin_ = pin; }

  void setup() override;
  void dump_config() override;

  void set_level(float);
  void set_brightness(uint8_t);

  uint8_t calculate_level(float);
  uint8_t calculate_brightness(uint8_t);

  void repaint();

 protected:
  TM1651 *batteryDisplay_;
  GPIOPin *clk_pin_;
  GPIOPin *dio_pin_;

  uint8_t brightness_;
  uint8_t level_;
};

}  // namespace tm1651
}  // namespace esphome
