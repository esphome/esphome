#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst328 {

static const uint8_t CST328_REG_STATUS = 0x00;
static const uint8_t CST328_TOUCH_MAX_POINTS = 5;
static const uint8_t CST328_TOUCH_DATA_SIZE = 27;

static const uint16_t CST_REG_TOUCH_INFORMATION = 0xD000;
static const uint16_t CST_REG_TOUCH_FINGER_NUMBER = 0xD005;
static const uint16_t CST_REG_KEY_TX_RX_NUMBERS = 0xD1F4;
static const uint16_t CST_REG_X_Y_RESOLUTION = 0xD1F8;
static const uint16_t CST_REG_FW_CRC_AND_BOOT_TIME = 0xD1FC;
static const uint16_t CST_REG_CHIP_TYPE_AND_PROJECT_ID = 0xD204;
static const uint16_t CST_REG_FW_REVISION = 0xD208;

// Work Modes
static const uint16_t CST_WM_DEBUG_INFO = 0xD101;
static const uint16_t CST_WM_RESET = 0xD102;
static const uint16_t CST_WM_RECALIBRATION = 0xD104;
static const uint16_t CST_WM_DEEP_SLEEP = 0xD105;
static const uint16_t CST_WM_DEBUG_POINT = 0xD108;
static const uint16_t CST_WM_NORMAL = 0xD109;
static const uint16_t CST_WM_DEBUG_RAWDATA = 0xD10A;
static const uint16_t CST_WM_DEBUG_DIFF = 0xD10D;
static const uint16_t CST_WM_DEBUG_FACTORY = 0xD119;
static const uint16_t CST_WM_DEBUG_FACTORY_2 = 0xD120;

class CST328ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST328Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void register_button_listener(CST328ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }

 protected:
  bool read16_(uint16_t addr, uint8_t *data, size_t len, bool ok_to_fail = false);
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

}  // namespace cst328
}  // namespace esphome
