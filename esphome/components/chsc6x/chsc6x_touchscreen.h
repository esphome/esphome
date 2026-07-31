#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::chsc6x {

static const char *const TAG = "chsc6x.touchscreen";

static const uint8_t CHSC6X_REG_STATUS = 0x00;
static const uint8_t CHSC6X_REG_STATUS_TOUCH = 0x00;
static const uint8_t CHSC6X_REG_STATUS_X_COR = 0x02;
static const uint8_t CHSC6X_REG_STATUS_Y_COR = 0x04;
static const uint8_t CHSC6X_REG_STATUS_LEN = 0x05;
static const uint8_t CHSC6X_CHIP_ID = 0x2e;

// CHSC6540 report layout, confirmed over 1651 captured samples on a 320x480
// panel (VIEWE UEDX32480035E-WB-A):
//
//   [0] [1]   always 0x00
//   [2]       number of active touch points (0, 1 or 2)
//   [3]       event flags in bits 7-6, X high nibble in bits 3-0
//   [4]       X low byte
//   [5]       event flags in bits 7-6, Y high nibble in bits 3-0
//   [6]       Y low byte
//   [7] [8]   fixed 0x0D 0x10 (pressure / area, never varied)
//   [9]..     second touch point, 0xFF 0xFF when absent
//
// Reading 7 bytes covers the header plus the first point, which is all the
// touchscreen component consumes.
//
// Bytes 3 and 5 carry the coordinate high nibble in bits 3-0, so a decoded
// coordinate is up to 12 bits (0..4095). The 320x480 panel these samples came
// from only ever used 9 of them, but the field itself is wider and nothing here
// assumes otherwise.
static const uint8_t CHSC6540_REG_STATUS_LEN = 0x07;
static const uint8_t CHSC6540_REG_STATUS_TOUCH = 0x02;
static const uint8_t CHSC6540_REG_STATUS_X_HI = 0x03;
static const uint8_t CHSC6540_REG_STATUS_X_LO = 0x04;
static const uint8_t CHSC6540_REG_STATUS_Y_HI = 0x05;
static const uint8_t CHSC6540_REG_STATUS_Y_LO = 0x06;

// The coordinate high nibble shares a byte with the event flags in bits 7-6.
static const uint8_t CHSC6540_COORD_HI_MASK = 0x0F;

// Only 1 and 2 are valid; anything higher is a corrupt frame.
static const uint8_t CHSC6540_MAX_TOUCHES = 2;

enum CHSC6XModel : uint8_t {
  // Original layout: touch count at byte 0, single-byte coordinates. Matches
  // the 240x240 Seeed Studio Round Display for XIAO this component targets.
  CHSC6X_MODEL_CHSC6X = 0,
  // CHSC6540: three-byte header and 9-bit coordinates. Required on panels wider
  // or taller than 255px, which single-byte coordinates cannot address.
  CHSC6X_MODEL_CHSC6540 = 1,
};

class CHSC6XTouchscreen final : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_model(CHSC6XModel model) { this->model_ = model; }

 protected:
  void update_touches_chsc6540_();

  InternalGPIOPin *interrupt_pin_{};
  CHSC6XModel model_{CHSC6X_MODEL_CHSC6X};
};

}  // namespace esphome::chsc6x
