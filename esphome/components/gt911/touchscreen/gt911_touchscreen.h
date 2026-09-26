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
  /// @brief Initialize the GT911 touchscreen.
  ///
  /// If @ref reset_pin_ is set, the touchscreen will be hardware reset,
  /// and the rest of the setup will be scheduled to run 50ms later using @ref set_timeout()
  /// to allow the device to stabilize after reset.
  ///
  /// If @ref interrupt_pin_ is set, it will be temporarily configured during reset
  /// to control I2C address selection.
  ///
  /// After the timeout, or immediately if no reset is performed, @ref setup_internal_()
  /// is called to complete the initialization.
  void setup() override;
  void dump_config() override;
  bool can_proceed() override { return this->setup_done_; }

  /// Set a interrupt pin (supports hardware interrupts or expander connected).
  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_use_primary_i2c_addr(bool flag) { this->use_primary_i2c_addr_ = flag; }
  void register_button_listener(GT911ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void update_touches() override;

  /// @brief Execute the GT911 initialization sequence for address 0x5D.
  ///
  /// Performs the reset and startup procedure required by the GT911 when operating
  /// at its 7‑bit I2C address 0x5D (0xBA 8-bit notation from datasheet).
  /// The function drives the reset and interrupt pins through the documented Goodix
  /// timing sequence and restores the interrupt pin to input mode once initialization is complete.
  bool init_sequence_(bool use_primary_i2c_address);
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
  /// @brief read device information registers from gt911 (product-id, firmware-version, config-version and resolution)
  void read_device_info_();

  GPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  std::vector<GT911ButtonListener *> button_listeners_;
  uint8_t button_state_{0xFF};       // last button state. Initial FF guarantees first update.
  bool use_primary_i2c_addr_{true};  // use 0x5d by default
};

}  // namespace esphome::gt911
