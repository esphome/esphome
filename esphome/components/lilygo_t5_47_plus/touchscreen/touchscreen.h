#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "../board_pins.h"

namespace esphome {
namespace lilygo_t5_47_plus {

using namespace touchscreen;

// GT911 register addresses (16-bit)
static const uint8_t GET_TOUCH_STATE[] = {0x81, 0x4E};
static const uint8_t CLEAR_TOUCH_STATE[] = {0x81, 0x4E, 0x00};
static const uint8_t GET_TOUCHES[] = {0x81, 0x4F};
static const uint8_t GET_SWITCHES[] = {0x80, 0x4D};
static const uint8_t GET_MAX_VALUES[] = {0x80, 0x48};

// Use the board-level TOUCH_INT constant from board_pins.h (GPIO47 on ESP32-S3).
static constexpr size_t MAX_TOUCHES = 5;

/// Touchscreen driver for the LilyGo T5 4.7" Plus board.
///
/// Uses GT911 (Goodix) touch controller with 16-bit I2C register protocol.
/// Implements the wakeup sequence required by this specific board (GPIO47 pulse)
/// and uses polling mode (no interrupt pin) since GPIO47 stays permanently LOW
/// on this hardware.
///
/// Based on the working reference implementation:
///   LilyGo-EPD47 / SensorLib v0.19 (TouchDrvGT911)
class LilygoT547PlusTouchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void update_touches() override;
};

}  // namespace lilygo_t5_47_plus
}  // namespace esphome
