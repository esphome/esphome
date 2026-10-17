#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::st7123 {

// Sitronix ST7123 capacitive touch controller.
// Registers are addressed with a 16-bit big-endian address (sent MSB first).
static constexpr uint16_t ST7123_REG_STATUS = 0x0001;  // [7:4] error code, [3:0] device status
static constexpr uint16_t ST7123_REG_MAX_X = 0x0005;   // 0x0005..0x0006 X resolution, 0x0007..0x0008 Y resolution
static constexpr uint16_t ST7123_REG_MAX_TOUCHES = 0x0009;
static constexpr uint16_t ST7123_REG_ADV_TOUCH_INFO = 0x0010;  // start of the reporting table
static constexpr uint16_t ST7123_REG_TOUCH_DATA = 0x0014;      // first touch point

// Device status field of the status register.
static constexpr uint8_t ST7123_STATUS_INIT = 0x1;

// Each touch point occupies 7 bytes: X high, X low, Y high, Y low, area, intensity, reserved.
static constexpr uint8_t ST7123_TOUCH_STRIDE = 7;
// Bit 7 of the X high byte indicates a valid touch point.
static constexpr uint8_t ST7123_TOUCH_VALID = 0x80;
// The X and Y high bytes only use the low 6 bits.
static constexpr uint8_t ST7123_COORD_HIGH_MASK = 0x3F;
// The ST7123 can report at most 10 touch points.
static constexpr uint8_t ST7123_MAX_TOUCHES = 10;

class ST7123Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint8_t max_touches_{ST7123_MAX_TOUCHES};
  uint32_t setup_time_{1};
};

}  // namespace esphome::st7123
