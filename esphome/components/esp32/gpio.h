#pragma once

#ifdef USE_ESP32
#include "esphome/core/hal.h"
#include <driver/gpio.h>

namespace esphome {
namespace esp32 {

// Static assertions to ensure our bit-packed fields can hold the enum values
static_assert(GPIO_NUM_MAX <= 256, "gpio_num_t has too many values for uint8_t");
static_assert(GPIO_DRIVE_CAP_MAX <= 4, "gpio_drive_cap_t has too many values for 2-bit field");

class ESP32InternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(gpio_num_t pin) { this->pin_ = static_cast<uint8_t>(pin); }
  void set_inverted(bool inverted) { this->pin_flags_.inverted = inverted; }
  void set_drive_strength(gpio_drive_cap_t drive_strength) {
    this->pin_flags_.drive_strength = static_cast<uint8_t>(drive_strength);
  }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return this->pin_; }
  gpio::Flags get_flags() const override { return this->flags_; }
  bool is_inverted() const override { return this->pin_flags_.inverted; }
  gpio_num_t get_pin_num() const { return static_cast<gpio_num_t>(this->pin_); }
  gpio_drive_cap_t get_drive_strength() const { return static_cast<gpio_drive_cap_t>(this->pin_flags_.drive_strength); }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  // Memory layout: 8 bytes total on 32-bit systems
  // - 3 bytes for members below
  // - 1 byte padding for alignment
  // - 4 bytes for vtable pointer
  uint8_t pin_;          // GPIO pin number (0-255, actual max ~54 on ESP32)
  gpio::Flags flags_{};  // GPIO flags (1 byte)
  struct PinFlags {
    uint8_t inverted : 1;        // Invert pin logic (1 bit)
    uint8_t drive_strength : 2;  // Drive strength 0-3 (2 bits)
    uint8_t reserved : 5;        // Reserved for future use (5 bits)
  } pin_flags_{};                // Total: 1 byte
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static bool isr_service_installed;
};

}  // namespace esp32
}  // namespace esphome

#endif  // USE_ESP32
