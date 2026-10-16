#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::gt911 {

class GT911ButtonListener {
 public:
  virtual void update_button(uint8_t index, bool state) = 0;
};

class GT911Touchscreen final : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  /// With a reset pin the chip is reset with the interrupt pin held low, which selects address 0x5D, and the
  /// rest of the setup runs from timeouts once the chip is ready. Without one setup_internal_() runs at once.
  void setup() override;
  void dump_config() override;
  bool can_proceed() override { return this->setup_done_; }

  /// Set a interrupt pin (supports hardware interrupts or expander connected).
  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }

  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void register_button_listener(GT911ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void update_touches() override;

  /// @brief Perform the internal setup routine for the GT911 touchscreen.
  ///
  /// This function checks the I2C address, configures the interrupt pin (if available),
  /// reads the touchscreen mode from the controller, and attempts to read calibration
  /// data (maximum X and Y values) if not already set.
  ///
  /// On success, sets @ref setup_done_ to true.
  /// On failure, calls @ref mark_failed() with an appropriate error message.
  void setup_internal_();
  /// @brief True if the touchscreen setup has completed successfully.
  bool setup_done_{false};

  GPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  std::vector<GT911ButtonListener *> button_listeners_;
  uint8_t button_state_{0xFF};  // last button state. Initial FF guarantees first update.
};

}  // namespace esphome::gt911
