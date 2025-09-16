#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include "esphome/core/hal.h"

namespace esphome::gpio_expander {

/// @brief A class to cache the read state of a GPIO expander.
///        This class caches reads between GPIO Pins which are on the same bank.
///        This means that for reading whole Port (ex. 8 pins) component needs only one
///        I2C/SPI read per main loop call. It assumes that one bit in byte identifies one GPIO pin.
///
///        Template parameters:
///           T - Type which represents internal bank register. Could be uint8_t or uint16_t.
///               Choose based on how your I/O expander reads pins:
///               * uint8_t:  For chips that read banks separately (8 pins at a time)
///                          Examples: MCP23017 (2x8-bit banks), TCA9555 (2x8-bit banks)
///               * uint16_t: For chips that read all pins at once (up to 16 pins)
///                          Examples: PCF8574/8575 (8/16 pins), PCA9554/9555 (8/16 pins)
///           N - Total number of pins (maximum 65535)
///           P - Type for pin number parameters (automatically selected based on N:
///               uint8_t for N<=256, uint16_t for N>256). Can be explicitly specified
///               if needed (e.g., for components like SN74HC165 with >256 pins)
template<typename T, uint16_t N, typename P = typename std::conditional<(N > 256), uint16_t, uint8_t>::type>
class CachedGpioExpander {
 public:
  /// @brief Read the state of the given pin. This will invalidate the cache for the given pin number.
  /// @param pin Pin number to read
  /// @return Pin state
  bool digital_read(P pin) {
    const P bank = pin / BANK_SIZE;
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

  void digital_write(P pin, bool value) { this->digital_write_hw(pin, value); }

 protected:
  /// @brief Read GPIO bank from hardware into internal state
  /// @param pin Pin number (used to determine which bank to read)
  /// @return true if read succeeded, false on communication error
  /// @note This does NOT return the pin state. It returns whether the read operation succeeded.
  ///       The actual pin state should be returned by digital_read_cache().
  virtual bool digital_read_hw(P pin) = 0;

  /// @brief Get cached pin value from internal state
  /// @param pin Pin number to read
  /// @return Pin state (true = HIGH, false = LOW)
  virtual bool digital_read_cache(P pin) = 0;

  /// @brief Write GPIO state to hardware
  /// @param pin Pin number to write
  /// @param value Pin state to write (true = HIGH, false = LOW)
  virtual void digital_write_hw(P pin, bool value) = 0;

  /// @brief Invalidate cache. This function should be called in component loop().
  void reset_pin_cache_() { memset(this->read_cache_valid_, 0x00, CACHE_SIZE_BYTES); }

  static constexpr uint16_t BITS_PER_BYTE = 8;
  static constexpr uint16_t BANK_SIZE = sizeof(T) * BITS_PER_BYTE;
  static constexpr size_t BANKS = N / BANK_SIZE;
  static constexpr size_t CACHE_SIZE_BYTES = BANKS * sizeof(T);

  T read_cache_valid_[BANKS]{0};
};

}  // namespace esphome::gpio_expander
