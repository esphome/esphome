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
namespace ft63x6 {

// Function Specific Type
enum class TouchStatusEnum : uint8_t {
  RELEASE = 0,
  TOUCH = 1,
  REPEAT = 2,
};

struct TouchPointType {
  TouchStatusEnum status;
  uint16_t x;
  uint16_t y;
};

struct FT63X6TouchPoint {
  uint8_t touch_count;
  TouchPointType tp[2];
};

struct FT63X6TouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(FT63X6TouchscreenStore *store);
};

using namespace touchscreen;

class FT63X6Touchscreen : public Touchscreen, public PollingComponent, public i2c::I2CDevice {
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
  uint16_t read_touch_coordinate_(uint8_t coordinate);
  uint8_t read_touch_id_(uint8_t id_address);

  FT63X6TouchscreenStore store_;
  FT63X6TouchPoint touch_point_;
  bool touched_{false};
};

}  // namespace ft63x6
}  // namespace esphome
