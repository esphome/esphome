#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"

namespace esphome::ns2009 {

class NS2009Component : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  /// Set the threshold for the touch detection.
  void set_threshold(uint8_t threshold) { this->threshold_ = threshold; }

  /// @brief Initialize the NS2009 touchscreen.
  ///
  /// This function checks the primary/secondary I2C addresses 0x48/0x49.
  void setup() override;
  void dump_config() override;

 protected:
  void update_touches() override;

  /// @brief The threshold at which a touch is registered.
  uint8_t threshold_{40};
};

}  // namespace esphome::ns2009
