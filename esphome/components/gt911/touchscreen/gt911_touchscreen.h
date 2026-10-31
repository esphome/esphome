#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#ifdef GT911_BUTTON_COUNT
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

#include <utility>
#endif

namespace esphome::gt911 {

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
#ifdef GT911_BUTTON_COUNT
  /// Kept sorted by index (config order within an index) so a report publishes its keys in index order.
  void add_button_binary_sensor(binary_sensor::BinarySensor *sensor, uint8_t index) {
    this->buttons_.push_back({sensor, index});
    for (size_t i = this->buttons_.size() - 1; i > 0 && this->buttons_[i - 1].index > index; i--)
      std::swap(this->buttons_[i - 1], this->buttons_[i]);
  }
#endif

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
#ifdef GT911_BUTTON_COUNT
  struct ButtonSensor {
    binary_sensor::BinarySensor *sensor;
    uint8_t index;
  };
  StaticVector<ButtonSensor, GT911_BUTTON_COUNT> buttons_;
  uint8_t button_state_{0xFF};  // last button state. Initial FF guarantees first update.
#endif
};

}  // namespace esphome::gt911
