#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace st7123 {

class ST7123ButtonListener {
 public:
  virtual void update_button(uint8_t index, bool state) = 0;
};

class ST7123Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  /// @brief Initialize the ST7123 touchscreen.
  ///
  /// If @ref reset_pin_ is set, the touchscreen will be hardware reset,
  /// and the rest of the setup will be scheduled to run 25ms later using @ref set_timeout()
  /// to allow the device to stabilize after reset.
  ///
  /// After the timeout, or immediately if no reset is performed, @ref setup_internal_()
  /// is called to complete the initialization.
  void setup() override;
  void dump_config() override;
  bool can_proceed() override { return this->setup_done_; }

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void register_button_listener(ST7123ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void update_touches() override;

  /// @brief Perform the internal setup routine for the ST7123 touchscreen.
  ///
  /// On success, sets @ref setup_done_ to true.
  /// On failure, calls @ref mark_failed() with an appropriate error message.
  void setup_internal_();
  /// @brief Perform the late setup routine for the ST7123 touchscreen.
  ///
  /// This function reads the calibration data (maximum X and Y values) if not already set.
  void setup_lazy_();
  /// @brief True if the touchscreen setup has completed successfully.
  bool setup_done_{false};

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  std::vector<ST7123ButtonListener *> button_listeners_;
  uint8_t button_state_{0xFF};  // last button state. Initial FF guarantees first update.
};

}  // namespace st7123
}  // namespace esphome
