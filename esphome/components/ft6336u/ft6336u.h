/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ft6336u {

// Function Specific Type
enum class TouchStatusEnum : uint8_t {
  TOUCH = 0,
  STREAM,
  RELEASE,
};

struct TouchPointType {
  TouchStatusEnum status;
  uint16_t x;
  uint16_t y;
};

struct FT6336UTouchPoint {
  uint8_t touch_count;
  TouchPointType tp[2];
};

struct FT6336UTouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(FT6336UTouchscreenStore *store);
};

using namespace touchscreen;

class FT6336UTouchscreen : public Touchscreen, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void hard_reset_();
  uint8_t read_byte_(uint8_t addr);
  void check_touch_();

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint16_t x_resolution_;
  uint16_t y_resolution_;

  uint8_t read_touch_count_();
  uint16_t read_touch1_x_();
  uint16_t read_touch1_y_();
  uint8_t read_touch1_id_();
  uint16_t read_touch2_x_();
  uint16_t read_touch2_y_();
  uint8_t read_touch2_id_();

  FT6336UTouchscreenStore store_;
  FT6336UTouchPoint touchPoint;
};

}  // namespace ft6336u
}  // namespace esphome
