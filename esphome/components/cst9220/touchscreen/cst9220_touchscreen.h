#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::cst9220 {

static const char *const TAG = "cst9220.touchscreen";

// The CST92xx family uses 16-bit (big-endian) register addresses.
static const uint16_t REG_TOUCH_DATA = 0xD000;  // touch report
static const uint16_t REG_CMD_MODE = 0xD101;    // enter command mode
static const uint16_t REG_CHECKCODE = 0xD1FC;   // firmware check code
static const uint16_t REG_RESOLUTION = 0xD1F8;  // panel resolution
static const uint16_t REG_CHIP_INFO = 0xD204;   // chip type + project id

static const uint8_t TOUCH_ACK = 0xAB;
static const uint8_t TOUCH_EVENT_DOWN = 0x06;

static const uint16_t CST9220_CHIP_ID = 0x9220;
static const uint16_t CST9217_CHIP_ID = 0x9217;

// Maximum simultaneous touch points reported by the family.
static const uint8_t CST9220_MAX_TOUCHES = 5;
// Report layout: 5 bytes per touch point plus 5 bytes of status/ack overhead.
static const size_t CST9220_DATA_LENGTH = CST9220_MAX_TOUCHES * 5 + 5;

class CST9220Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void continue_setup_();

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint16_t chip_id_{};
  uint16_t project_id_{};
  bool setup_complete_{};
};

}  // namespace esphome::cst9220
