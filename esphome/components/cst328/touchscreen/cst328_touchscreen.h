#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

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

class CST328ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST328Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void register_button_listener(CST328ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void reset_device_();
  void continue_setup_();
  void update_button_state_(bool state);

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};

  std::vector<CST328ButtonListener *> button_listeners_;
  bool button_touched_{};

  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint8_t fw_ver_major_{};
  uint8_t fw_ver_minor_{};
  uint16_t fw_build_{};

  bool setup_complete_{};
};

}  // namespace esphome::cst328
