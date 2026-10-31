#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#ifdef CST328_BUTTON_COUNT
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"
#endif

namespace esphome::cst328 {

static const uint8_t CST328_TOUCH_MAX_POINTS = 5;
static const uint8_t CST328_TOUCH_DATA_SIZE = CST328_TOUCH_MAX_POINTS * 5 + 2;

static const uint16_t CST_REG_TOUCH_INFORMATION = 0xD000;
static const uint16_t CST_REG_TOUCH_FINGER_NUMBER = 0xD005;

static const uint16_t CST_REG_FINGER_COUNT_IDX = CST_REG_TOUCH_FINGER_NUMBER - CST_REG_TOUCH_INFORMATION;

static const uint16_t CST_REG_X_Y_RESOLUTION = 0xD1F8;
static const uint16_t CST_REG_FW_CRC_AND_BOOT_TIME = 0xD1FC;
static const uint16_t CST_REG_CHIP_TYPE_AND_PROJECT_ID = 0xD204;
static const uint16_t CST_REG_FW_REVISION = 0xD208;

static const uint16_t CST_WM_DEBUG_INFO = 0xD101;
static const uint16_t CST_WM_NORMAL = 0xD109;

class CST328Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
#ifdef CST328_BUTTON_COUNT
  void add_button_binary_sensor(binary_sensor::BinarySensor *sensor) { this->buttons_.push_back(sensor); }
#endif
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void reset_device_();
  void continue_setup_();
  void update_button_state_(bool state);

  // Small fields first: they fill the padding at the end of the base classes
  uint8_t fw_ver_major_{};
  uint8_t fw_ver_minor_{};
#ifdef CST328_BUTTON_COUNT
  bool button_touched_{};
#endif
  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};

#ifdef CST328_BUTTON_COUNT
  StaticVector<binary_sensor::BinarySensor *, CST328_BUTTON_COUNT> buttons_;
#endif

  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint16_t fw_build_{};

  bool setup_complete_{};
};

}  // namespace esphome::cst328
