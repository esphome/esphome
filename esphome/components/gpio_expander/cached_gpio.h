#pragma once

#include <array>
#include <cstdint>
#include "esphome/core/hal.h"

namespace esphome {
namespace gpio_expander {

/// @brief A class to cache the read state of a GPIO expander.
///        This class caches reads between GPIO Pins which are on the same bank.
///        This means that for reading whole Port (ex. 8 pins) component needs only one
///        I2C/SPI read per main loop call. It assumes, that one bit in byte identifies one GPIO pin
///        Template parameters:
///           T - Type which represents internal register. Could be uint8_t or uint16_t. Adjust to
///               match size of your internal GPIO bank register.
///           N - Number of pins
template<typename T, T N> class CachedGpioExpander {
 public:
  bool digital_read(T pin) {
    uint8_t bank = pin / (sizeof(T) * BITS_PER_BYTE);
    if (this->read_cache_invalidated_[bank]) {
      this->read_cache_invalidated_[bank] = false;
      if (!this->digital_read_hw(pin))
        return false;
    }
    return this->digital_read_cache(pin);
  }

  void digital_write(T pin, bool value) { this->digital_write_hw(pin, value); }

 protected:
  /// @brief Call component low level function to read GPIO state from device
  virtual bool digital_read_hw(T pin) = 0;
  /// @brief Call component read function from internal cache.
  virtual bool digital_read_cache(T pin) = 0;
  /// @brief Call component low level function to write GPIO state to device
  virtual void digital_write_hw(T pin, bool value) = 0;
  const uint8_t cache_byte_size_ = N / (sizeof(T) * BITS_PER_BYTE);

  /// @brief Invalidate cache. This function should be called in component loop().
  void reset_pin_cache_() {
    for (T i = 0; i < this->cache_byte_size_; i++) {
      this->read_cache_invalidated_[i] = true;
    }
  }

  static const uint8_t BITS_PER_BYTE = 8;
  std::array<bool, N / (sizeof(T) * BITS_PER_BYTE)> read_cache_invalidated_{};
};

}  // namespace gpio_expander
}  // namespace esphome
