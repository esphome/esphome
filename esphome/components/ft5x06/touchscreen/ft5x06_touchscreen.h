#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ft5x06 {

enum VendorId {
  FT5X06_ID_UNKNOWN = 0,
  FT5X06_ID_1 = 0x51,
  FT5X06_ID_2 = 0x11,
  FT5X06_ID_3 = 0xCD,
};

enum FTCmd : uint8_t {
  FT5X06_MODE_REG = 0x00,
  FT5X06_ORIGIN_REG = 0x08,
  FT5X06_RESOLUTION_REG = 0x0C,
  FT5X06_VENDOR_ID_REG = 0xA8,
  FT5X06_TD_STATUS = 0x02,
  FT5X06_TOUCH_DATA = 0x03,
  FT5X06_I_MODE = 0xA4,
  FT5X06_TOUCH_MAX = 0x4C,
};

enum FTMode : uint8_t {
  FT5X06_OP_MODE = 0,
  FT5X06_SYSINFO_MODE = 0x10,
  FT5X06_TEST_MODE = 0x40,
};

static const size_t MAX_TOUCHES = 5;  // max number of possible touches reported

class FT5x06Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update_touches() override;

  void set_interrupt_pin(InternalGPIOPin *interrupt_pin) { this->interrupt_pin_ = interrupt_pin; }

 protected:
  void continue_setup_();
  bool err_check_(i2c::ErrorCode err, const char *msg);
  bool set_mode_(FTMode mode);
  VendorId vendor_id_{FT5X06_ID_UNKNOWN};

  InternalGPIOPin *interrupt_pin_{nullptr};
};

}  // namespace ft5x06
}  // namespace esphome
