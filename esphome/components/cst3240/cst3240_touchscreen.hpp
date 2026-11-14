#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::cst3240 {

// Each finger data will charge the I2C bus, more without interrupt_pin set
// Note the polling mode (without int) will also prevent the automatic chip standby
// and query the bus for these every 100ms... forever...
static constexpr uint8_t MAX_TOUCH_POINTS = 3;

class CST3240Touchscreen final : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  bool check_init_();
  void continue_setup_();

  void update_touches() override;

  bool write_reg16_(uint16_t reg);
  bool write_reg16_(uint16_t reg, uint8_t byte);
  bool clear_reg16_(uint16_t reg);
  bool read_reg16_(uint16_t reg, uint8_t *buffer, size_t len);

  bool inited_{false};
  bool polling_{true};

  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint8_t fw_ver_major_{};
  uint8_t fw_ver_minor_{};
  uint16_t fw_build_{};

  InternalGPIOPin *interrupt_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
};

}  // namespace esphome::cst3240
