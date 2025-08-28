#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include "esphome/core/hal.h"

namespace esphome::gpio_expander {

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
  /// @brief Read the state of the given pin. This will invalidate the cache for the given pin number.
  /// @param pin Pin number to read
  /// @return Pin state
  bool digital_read(T pin) {
    const uint8_t bank = pin / BANK_SIZE;
    const T pin_mask = (1 << (pin % BANK_SIZE));
    // Check if specific pin cache is valid
    if (this->read_cache_valid_[bank] & pin_mask) {
      // Invalidate pin
      this->read_cache_valid_[bank] &= ~pin_mask;
    } else {
      // Read whole bank from hardware
      if (!this->digital_read_hw(pin))
        return false;
      // Mark bank cache as valid except the pin that is being returned now
      this->read_cache_valid_[bank] = std::numeric_limits<T>::max() & ~pin_mask;
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

  /// @brief Invalidate cache. This function should be called in component loop().
  void reset_pin_cache_() { memset(this->read_cache_valid_, 0x00, CACHE_SIZE_BYTES); }

  static constexpr uint8_t BITS_PER_BYTE = 8;
  static constexpr uint8_t BANK_SIZE = sizeof(T) * BITS_PER_BYTE;
  static constexpr size_t BANKS = N / BANK_SIZE;
  static constexpr size_t CACHE_SIZE_BYTES = BANKS * sizeof(T);

  T read_cache_valid_[BANKS]{0};
};

}  // namespace esphome::gpio_expander
