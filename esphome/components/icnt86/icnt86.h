#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace icnt86 {

using namespace touchscreen;

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

struct ICNT86TouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;
  static void gpio_intr(ICNT86TouchscreenStore *store);
};

class ICNT86Touchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void reset_();
  void I2C_Write_Byte_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_Read_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_Write_(UWORD Reg, char *Data, UBYTE len);
  void I2C_Read_Byte_(UWORD Reg, char *Data, UBYTE len);
  void ICNT_ReadVersion_();
  void reset_touch_sensor_();

  ICNT86TouchscreenStore store_;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_{nullptr};


};

}  // namespace icnt86
}  // namespace esphome
