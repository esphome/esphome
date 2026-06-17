#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {

/// Maximum buffer size for dump_summary output
inline constexpr size_t GPIO_SUMMARY_MAX_LEN = 48;

#ifdef USE_ESP8266
#define LOG_PIN(prefix, pin) log_pin(TAG, F(prefix), pin)
#else
#define LOG_PIN(prefix, pin) log_pin(TAG, prefix, pin)
#endif

// put GPIO flags in a namespace to not pollute esphome namespace
namespace gpio {

enum Flags : uint8_t {
  // Can't name these just INPUT because of Arduino defines :(
  FLAG_NONE = 0x00,
  FLAG_INPUT = 0x01,
  FLAG_OUTPUT = 0x02,
  FLAG_OPEN_DRAIN = 0x04,
  FLAG_PULLUP = 0x08,
  FLAG_PULLDOWN = 0x10,
};

class FlagsHelper {
 public:
  constexpr FlagsHelper(Flags val) : val_(val) {}
  constexpr operator Flags() const { return val_; }

 protected:
  Flags val_;
};
constexpr FlagsHelper operator&(Flags lhs, Flags rhs) {
  return static_cast<Flags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr FlagsHelper operator|(Flags lhs, Flags rhs) {
  return static_cast<Flags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

enum InterruptType : uint8_t {
  INTERRUPT_RISING_EDGE = 1,
  INTERRUPT_FALLING_EDGE = 2,
  INTERRUPT_ANY_EDGE = 3,
  INTERRUPT_LOW_LEVEL = 4,
  INTERRUPT_HIGH_LEVEL = 5,
};

}  // namespace gpio

class GPIOPin {
 public:
  virtual void setup() = 0;

  virtual void pin_mode(gpio::Flags flags) = 0;

  /**
   * @brief Retrieve GPIO pin flags.
   *
   * @return The GPIO flags describing the pin mode and properties.
   */
  virtual gpio::Flags get_flags() const = 0;

  virtual bool digital_read() = 0;

  virtual void digital_write(bool value) = 0;

  /// Write a summary of this pin to the provided buffer.
  /// @param buffer The buffer to write to
  /// @param len The size of the buffer (must be > 0)
  /// @return The number of characters that would be written (excluding null terminator),
  ///         which may exceed len-1 if truncation occurred (snprintf semantics)
  virtual size_t dump_summary(char *buffer, size_t len) const;

  /// Get a summary of this pin as a string.
  /// @deprecated Use dump_summary(char*, size_t) instead. Will be removed in 2026.7.0.
  ESPDEPRECATED("Override dump_summary(char*, size_t) instead. Will be removed in 2026.7.0.", "2026.1.0")
  virtual std::string dump_summary() const;

  virtual bool is_internal() { return false; }
};

/// Copy of GPIOPin that is safe to use from ISRs (with no virtual functions)
class ISRInternalGPIOPin {
 public:
  ISRInternalGPIOPin() = default;
  ISRInternalGPIOPin(void *arg) : arg_(arg) {}
  bool digital_read();
  void digital_write(bool value);
  void clear_interrupt();
  void pin_mode(gpio::Flags flags);

 protected:
  void *arg_{nullptr};
};

class InternalGPIOPin : public GPIOPin {
 public:
  template<typename T> void attach_interrupt(void (*func)(T *), T *arg, gpio::InterruptType type) const {
    this->attach_interrupt(reinterpret_cast<void (*)(void *)>(func), arg, type);
  }

  virtual void detach_interrupt() const = 0;

  virtual ISRInternalGPIOPin to_isr() const = 0;

  virtual uint8_t get_pin() const = 0;

  bool is_internal() override { return true; }

  virtual bool is_inverted() const = 0;

 protected:
  virtual void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const = 0;
};

// Inline default implementations for GPIOPin virtual methods.
// These provide bridge functionality for backwards compatibility with external components.

// Default implementation bridges to old std::string method for backwards compatibility.
inline size_t GPIOPin::dump_summary(char *buffer, size_t len) const {
  if (len == 0)
    return 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::string s = this->dump_summary();
#pragma GCC diagnostic pop
  size_t copy_len = std::min(s.size(), len - 1);
  memcpy(buffer, s.c_str(), copy_len);
  buffer[copy_len] = '\0';
  return s.size();  // Return would-be length (snprintf semantics)
}

// Default implementation returns empty string.
// External components should override this if they haven't migrated to buffer-based version.
// Remove before 2026.7.0
inline std::string GPIOPin::dump_summary() const { return {}; }

// Inline helper for log_pin - allows compiler to inline into log_pin in gpio.cpp
inline void log_pin_with_prefix(const char *tag, const char *prefix, GPIOPin *pin) {
  char buffer[GPIO_SUMMARY_MAX_LEN];
  size_t len = pin->dump_summary(buffer, sizeof(buffer));
  len = std::min(len, sizeof(buffer) - 1);
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s%.*s", prefix, (int) len, buffer);
}

// log_pin function declarations - implementation in gpio.cpp
#ifdef USE_ESP8266
void log_pin(const char *tag, const __FlashStringHelper *prefix, GPIOPin *pin);
#else
void log_pin(const char *tag, const char *prefix, GPIOPin *pin);
#endif

}  // namespace esphome
