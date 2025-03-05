#pragma once

#ifdef USE_ESP32
#include "esphome/core/hal.h"
#include <driver/gpio.h>

namespace esphome {
namespace esp32 {

class ESP32InternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(gpio_num_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_drive_strength(gpio_drive_cap_t drive_strength) { drive_strength_ = drive_strength; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return (uint8_t) pin_; }
  gpio::Flags get_flags() const override { return flags_; }
  bool is_inverted() const override { return inverted_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  gpio_num_t pin_;
  bool inverted_;
  gpio_drive_cap_t drive_strength_;
  gpio::Flags flags_;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static bool isr_service_installed;
};

}  // namespace esp32
}  // namespace esphome

#endif  // USE_ESP32
