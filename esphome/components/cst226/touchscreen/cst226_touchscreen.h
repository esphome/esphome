#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst226 {

static const char *const TAG = "cst226.touchscreen";

static const uint8_t CST226_REG_STATUS = 0x00;

class CST226Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }

 protected:
  bool read16_(uint16_t addr, uint8_t *data, size_t len) {
    if (this->read_register16(addr, data, len) != i2c::ERROR_OK) {
      esph_log_e(TAG, "Read data from 0x%04X failed", addr);
      this->mark_failed();
      return false;
    }
    return true;
  }
  void continue_setup_();

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{NULL_PIN};
  uint8_t chip_id_{};
  bool setup_complete_{};
};

}  // namespace cst226
}  // namespace esphome
