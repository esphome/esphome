#include <gtest/gtest.h>

#include "esphome/components/power_supply/power_supply.h"
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"

namespace esphome::power_supply::testing {

// Minimal dummy internal GPIO pin implementation for testing
class DummyInternalPin : public InternalGPIOPin {
 public:
  DummyInternalPin() = default;
  void setup() override {}
  void pin_mode(esphome::gpio::Flags) override {}
  esphome::gpio::Flags get_flags() const override { return esphome::gpio::FLAG_NONE; }
  bool digital_read() override { return false; }
  void digital_write(bool) override {}
  void detach_interrupt() const override {}
  ISRInternalGPIOPin to_isr() const override { return ISRInternalGPIOPin(); }
  uint8_t get_pin() const override { return 0; }
  bool is_inverted() const override { return false; }

 protected:
  // Implement protected attach_interrupt required by InternalGPIOPin
  void attach_interrupt(void (*func)(void *), void *arg, esphome::gpio::InterruptType type) const override {}
};

TEST(PowerSupply, HasHigherPriorityThanBusWhenInternalAndEnableOnBoot) {
  power_supply::PowerSupply ps;
  DummyInternalPin pin;
  ps.set_pin(&pin);
  ps.set_enable_on_boot(true);

  // POWER priority should be greater than BUS priority
  EXPECT_GT(ps.get_setup_priority(), setup_priority::BUS);
}

TEST(PowerSupply, FallsBackToIOWhenNotEnableOnBoot) {
  power_supply::PowerSupply ps;
  DummyInternalPin pin;
  ps.set_pin(&pin);
  ps.set_enable_on_boot(false);

  EXPECT_EQ(ps.get_setup_priority(), setup_priority::IO);
}

}  // namespace esphome::power_supply::testing
