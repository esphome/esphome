/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ft6336u {

// Function Specific Type
typedef enum {
    touch = 0, 
    stream, 
    release, 
} TouchStatusEnum; 
typedef struct {
    TouchStatusEnum status; 
    uint16_t x; 
    uint16_t y; 
} TouchPointType;
typedef struct {
    uint8_t touch_count; 
    TouchPointType tp[2]; 
} FT6336U_TouchPointType; 

using namespace touchscreen;

class FT6336UTouchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void hard_reset_();
  uint8_t readByte(uint8_t addr); 
  void writeByte(uint8_t addr, uint8_t data); 
  FT6336U_TouchPointType scan(void);

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_;
  uint16_t x_resolution_;
  uint16_t y_resolution_;

  uint8_t read_td_status(void);
  uint16_t read_touch1_x(void);
  uint16_t read_touch1_y(void);
  uint8_t read_touch1_id(void);
  uint16_t read_touch2_x(void);
  uint16_t read_touch2_y(void);
  uint8_t read_touch2_id(void);

  FT6336U_TouchPointType touchPoint; 
};

}  // namespace ft6336u
}  // namespace esphome
