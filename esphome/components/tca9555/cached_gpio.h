#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tca9555 {

/// @brief A proxy class for forcing one GPIO read/write call to HW per main loop iteration
class CachedGpio : virtual Component {
 public:
  CachedGpio();

  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);

  void loop() override;

 protected:
  virtual bool read_gpio_from_cache(uint8_t pin) = 0;
  virtual bool write_gpio_from_cache(uint8_t pin, bool value) = 0;
  virtual bool read_gpio_hw() = 0;
  virtual bool write_gpio_hw() = 0;

 private:
  bool read_cache_invalidated_;
  bool write_cache_invalidated_;
};

}  // namespace tca9555
}  // namespace esphome
