#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst226 {

static const uint8_t CST226_REG_STATUS = 0x00;

class CST226ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST226Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }
  void register_button_listener(CST226ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  bool read16_(uint16_t addr, uint8_t *data, size_t len);
  void continue_setup_();
  void update_button_state_(bool state);

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint8_t chip_id_{};
  bool setup_complete_{};
  std::vector<CST226ButtonListener *> button_listeners_;
  bool button_touched_{};
};

}  // namespace cst226
}  // namespace esphome
