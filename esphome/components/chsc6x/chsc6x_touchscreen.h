#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace chsc6x {

static const char *const TAG = "chsc6x.touchscreen";

static const uint8_t CHSC6X_REG_STATUS = 0x00;
static const uint8_t CHSC6X_REG_STATUS_TOUCH = 0x00;
static const uint8_t CHSC6X_REG_STATUS_X_COR = 0x02;
static const uint8_t CHSC6X_REG_STATUS_Y_COR = 0x04;
static const uint8_t CHSC6X_REG_STATUS_LEN = 0x05;
static const uint8_t CHSC6X_CHIP_ID = 0x2e;

class CHSC6XTouchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  InternalGPIOPin *interrupt_pin_{};
};

}  // namespace chsc6x
}  // namespace esphome
