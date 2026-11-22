#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include "esphome/core/hal.h"

namespace esphome::gpio_expander {

/// Pin number type for an expander with N pins.
template<uint16_t N> using expander_pin_t = std::conditional_t<(N > 256), uint16_t, uint8_t>;

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
///
///        CachedGpioExpanderBase holds the cache; CachedGpioExpander adds the invalidate-on-read flag, and
///        CachedGpioExpanderComponent takes it from the interrupt pin instead.
template<typename T, uint16_t N, typename P = expander_pin_t<N>> class CachedGpioExpanderBase {
 public:
  void digital_write(P pin, bool value) { this->digital_write_hw(pin, value); }

 protected:
  using bank_t = T;
  using pin_t = P;

  /// Read pin through the bank cache. With invalidate_on_read the pin is read from hardware again next
  /// time; otherwise the bank stays cached until reset_pin_cache_().
  bool cached_read_(P pin, bool invalidate_on_read) {
    const P bank = pin / BANK_SIZE;
    const T pin_mask = T(1) << (pin % BANK_SIZE);
    // Check if specific pin cache is valid
    if (this->read_cache_valid_[bank] & pin_mask) {
      if (invalidate_on_read) {
        // Invalidate pin so next read triggers hardware read
        this->read_cache_valid_[bank] &= ~pin_mask;
      }
    } else {
      // Read whole bank from hardware
      if (!this->digital_read_hw(pin))
        return false;
      // Mark bank cache as valid except the pin that is being returned now
      // (when not invalidating on read, mark all pins including this one as valid)
      this->read_cache_valid_[bank] = std::numeric_limits<T>::max() & ~(invalidate_on_read ? pin_mask : 0);
    }
    return this->digital_read_cache(pin);
  }

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

/// A CachedGpioExpanderBase whose digital_read() invalidates each pin it returns unless told otherwise.
template<typename T, uint16_t N, typename P = expander_pin_t<N>>
class CachedGpioExpander : public CachedGpioExpanderBase<T, N, P> {
 public:
  bool digital_read(P pin) { return this->cached_read_(pin, this->invalidate_on_read_); }

 protected:
  /// @brief Control whether digital_read() invalidates the pin's cache entry after reading.
  /// When enabled (default), each read self-invalidates so the next read triggers a hardware read.
  /// When disabled, cache stays valid until reset_pin_cache_() is explicitly called.
  void set_invalidate_on_read_(bool invalidate) { this->invalidate_on_read_ = invalidate; }

  bool invalidate_on_read_{true};
};

/// Adds a store of the last value read from each bank to a read cache Base. digital_read_hw() stores the
/// bank it reads in input_[pin / BANK_SIZE] (a chip read in one transfer is one bank, input_[0]), and
/// digital_read_cache() is provided.
template<typename Base> class GpioExpanderInputs : public Base {
 protected:
  using typename Base::bank_t;
  using typename Base::pin_t;
  using Base::BANK_SIZE;
  using Base::BANKS;

  bool digital_read_cache(pin_t pin) override {
    return this->input_[pin / BANK_SIZE] & (bank_t(1) << (pin % BANK_SIZE));
  }

  /// The input state per bank - 1 means HIGH, 0 means LOW
  bank_t input_[BANKS]{};
};

}  // namespace esphome::gpio_expander
