#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst92xx {

static const char *const TAG = "cst92xx.touchscreen";

static const uint16_t CST9217_CHIP_ID = 0x9217;
static const uint16_t CST9220_CHIP_ID = 0x9220;

#define CST92XX_DATA_REG 0xD000
#define CST92XX_PROJECT_ID_REG 0xD204
#define CST92XX_CMD_MODE_REG 0xD101
#define CST92XX_CHECKCODE_REG 0xD1FC
#define CST92XX_RESOLUTION_REG 0xD1F8

#define CST92XX_ACK_VALUE 0xAB
#define CST92XX_MAX_TOUCH_POINTS 3
#define CST92XX_DATA_LENGTH (CST92XX_MAX_TOUCH_POINTS * 5 + 5)

class CST92xxButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST92xxTouchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  bool read16_(uint16_t addr, uint8_t *data, size_t len);
  void continue_setup_();

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint16_t chip_id_{};
};

}  // namespace cst92xx
}  // namespace esphome
