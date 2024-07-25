#pragma once

#ifdef USE_ZEPHYR
#include "esphome/core/hal.h"
struct device;
namespace esphome {
namespace zephyr {

class ZephyrGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return pin_; }
  bool is_inverted() const override { return inverted_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
  const device *gpio_ = nullptr;
  bool value_ = false;
};

}  // namespace zephyr
}  // namespace esphome

#endif  // USE_ZEPHYR
