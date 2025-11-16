#pragma once

#ifdef USE_ZEPHYR
#include "esphome/core/hal.h"
struct device;
namespace esphome {
namespace zephyr {

class ZephyrGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return this->pin_; }
  bool is_inverted() const override { return this->inverted_; }
  gpio::Flags get_flags() const override { return flags_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;
  uint8_t pin_;
  bool inverted_{};
  gpio::Flags flags_{};
  const device *gpio_{nullptr};
  bool value_{false};
};

}  // namespace zephyr
}  // namespace esphome

#endif  // USE_ZEPHYR
