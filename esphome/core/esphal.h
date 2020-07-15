#pragma once

#include "Arduino.h"
#ifdef ARDUINO_ARCH_ESP32
#include <esp32-hal.h>
#endif
// Fix some arduino defs
#ifdef round
#undef round
#endif
#ifdef bool
#undef bool
#endif
#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef abs
#undef abs
#endif

namespace esphome {

#define LOG_PIN(prefix, pin) \
  if ((pin) != nullptr) { \
    ESP_LOGCONFIG(TAG, prefix LOG_PIN_PATTERN, LOG_PIN_ARGS(pin)); \
  }
#define LOG_PIN_PATTERN "GPIO%u (Mode: %s%s)"
#define LOG_PIN_ARGS(pin) (pin)->get_pin(), (pin)->get_pin_mode_name(), ((pin)->is_inverted() ? ", INVERTED" : "")

/// Copy of GPIOPin that is safe to use from ISRs (with no virtual functions)
class ISRInternalGPIOPin {
 public:
  ISRInternalGPIOPin(uint8_t pin,
#ifdef ARDUINO_ARCH_ESP32
                     volatile uint32_t *gpio_clear, volatile uint32_t *gpio_set,
#endif
                     volatile uint32_t *gpio_read, uint32_t gpio_mask, bool inverted);
  bool digital_read();
  void digital_write(bool value);
  void clear_interrupt();

 protected:
  const uint8_t pin_;
  const bool inverted_;
  volatile uint32_t *const gpio_read_;
  const uint32_t gpio_mask_;
#ifdef ARDUINO_ARCH_ESP32
  volatile uint32_t *const gpio_clear_;
  volatile uint32_t *const gpio_set_;
#endif
};

/** A high-level abstraction class that can expose a pin together with useful options like pinMode.
 *
 * Set the parameters for this at construction time and use setup() to apply them. The inverted parameter will
 * automatically invert the input/output for you.
 *
 * Use read_value() and write_value() to use digitalRead() and digitalWrite(), respectively.
 */
class GPIOPin {
 public:
  /** Construct the GPIOPin instance.
   *
   * @param pin The GPIO pin number of this instance.
   * @param mode The Arduino pinMode that this pin should be put into at setup().
   * @param inverted Whether all digitalRead/digitalWrite calls should be inverted.
   */
  GPIOPin(uint8_t pin, uint8_t mode, bool inverted = false);

  /// Setup the pin mode.
  virtual void setup();
  /// Read the binary value from this pin using digitalRead (and inverts automatically).
  virtual bool digital_read();
  /// Write the binary value to this pin using digitalWrite (and inverts automatically).
  virtual void digital_write(bool value);
  /// Set the pin mode
  virtual void pin_mode(uint8_t mode);

  /// Get the GPIO pin number.
  uint8_t get_pin() const;
  const char *get_pin_mode_name() const;
  /// Get the pinMode of this pin.
  uint8_t get_mode() const;
  /// Return whether this pin shall be treated as inverted. (for example active-low)
  bool is_inverted() const;

  template<typename T> void attach_interrupt(void (*func)(T *), T *arg, int mode) const;
  void detach_interrupt() const;

  ISRInternalGPIOPin *to_isr() const;

 protected:
  void attach_interrupt_(void (*func)(void *), void *arg, int mode) const;
  void detach_interrupt_() const;

  const uint8_t pin_;
  const uint8_t mode_;
  const bool inverted_;
#ifdef ARDUINO_ARCH_ESP32
  volatile uint32_t *const gpio_set_;
  volatile uint32_t *const gpio_clear_;
#endif
  volatile uint32_t *const gpio_read_;
  const uint32_t gpio_mask_;
};

template<typename T> void GPIOPin::attach_interrupt(void (*func)(T *), T *arg, int mode) const {
  this->attach_interrupt_(reinterpret_cast<void (*)(void *)>(func), arg, mode);
}
/** This function can be used by the HAL to force-link specific symbols
 * into the generated binary without modifying the linker script.
 *
 * It is called by the application very early on startup and should not be used for anything
 * other than forcing symbols to be linked.
 */
void force_link_symbols();

}  // namespace esphome
