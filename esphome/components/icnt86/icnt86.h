#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace icnt86 {

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

class ICNT86Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void reset_();
  void I2C_Write_Byte_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_Read_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_Write_(UWORD Reg, char *Data, UBYTE len);
  void I2C_Read_Byte_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_ReadVersion_();
  void reset_touch_sensor_();
  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{nullptr};


};

}  // namespace icnt86
}  // namespace esphome
