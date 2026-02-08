#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::ns2009 {

class NS2009Component : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  /// Set the threshold for the touch detection.
  void set_threshold(uint8_t threshold) { this->threshold_ = threshold; }

  /// @brief Initialize the NS2009 touchscreen.
  ///
  /// This function checks the primary/secondary I2C addresses 0x48/0x49.
  ///
  /// On success, sets @ref setup_done_ to true.
  /// On failure, calls @ref mark_failed() with an appropriate error message.
  void setup() override;
  void dump_config() override;

 protected:
  void update_touches() override;

  /// @brief True if the touchscreen setup has completed successfully.
  bool setup_done_{false};

  /// @brief The threshold at which a touch is registered.
  uint8_t threshold_{40};
};

}  // namespace esphome::ns2009
