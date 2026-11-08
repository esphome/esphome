#include <gtest/gtest.h>

#include "esphome/components/gpio_expander/cached_gpio_component.h"

namespace esphome::gpio_expander::testing {

class FakeComponent : public CachedGpioExpanderComponent<uint8_t, 16> {
 public:
  void setup() override { this->setup_interrupt_pin_(); }
  void pin_mode(uint8_t pin, gpio::Flags flags) {
    if (flags & gpio::FLAG_INPUT)
      this->enable_polling_loop_();
  }

  uint8_t chip[2]{};
  unsigned reads{0};

 protected:
  bool digital_read_hw(uint8_t pin) override {
    this->reads++;
    this->input_[pin / 8] = this->chip[pin / 8];
    return true;
  }
  void digital_write_hw(uint8_t pin, bool value) override {}
};

class CachedGpioComponentTest : public ::testing::Test {
 protected:
  void SetUp() override { this->int_pin_.set_inverted(true); }  // INT released

  FakeComponent component_;
  InternalGPIOPin int_pin_;  // digital_read() returns the inverted flag on the host, so it doubles as the level
};

TEST_F(CachedGpioComponentTest, InputEnablesPollingOnlyWithoutAnInterruptPin) {
  this->component_.setup();
  ASSERT_TRUE(this->component_.is_idle());
  this->component_.pin_mode(0, gpio::FLAG_OUTPUT);
  EXPECT_TRUE(this->component_.is_idle());
  this->component_.pin_mode(0, gpio::FLAG_INPUT);
  EXPECT_TRUE(this->component_.is_in_loop_state());

  FakeComponent with_interrupt;
  with_interrupt.set_interrupt_pin(&this->int_pin_);
  with_interrupt.setup();
  ASSERT_TRUE(with_interrupt.is_idle());
  with_interrupt.pin_mode(0, gpio::FLAG_INPUT);
  EXPECT_TRUE(with_interrupt.is_idle());
}

TEST_F(CachedGpioComponentTest, WithoutAnInterruptPinEveryReadHitsTheChip) {
  this->component_.setup();
  this->component_.digital_read(0);
  this->component_.digital_read(0);
  EXPECT_EQ(this->component_.reads, 2u);
}

TEST_F(CachedGpioComponentTest, InterruptPinKeepsTheCacheUntilTheLoopRuns) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  this->component_.chip[0] = 0x01;
  this->component_.chip[1] = 0x80;
  EXPECT_TRUE(this->component_.digital_read(0));
  EXPECT_TRUE(this->component_.digital_read(15));
  this->component_.chip[0] = 0x00;
  this->component_.chip[1] = 0x00;
  EXPECT_TRUE(this->component_.digital_read(0));
  EXPECT_TRUE(this->component_.digital_read(15));
  EXPECT_EQ(this->component_.reads, 2u);

  this->component_.loop();
  EXPECT_FALSE(this->component_.digital_read(0));
  EXPECT_FALSE(this->component_.digital_read(15));
  EXPECT_EQ(this->component_.reads, 4u);
}

TEST_F(CachedGpioComponentTest, LoopStaysOnUntilIntIsReleased) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  this->component_.enable_loop();
  ASSERT_TRUE(this->component_.is_in_loop_state());

  this->int_pin_.set_inverted(false);  // INT still low
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_in_loop_state());

  this->int_pin_.set_inverted(true);  // INT released
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_idle());
}

TEST_F(CachedGpioComponentTest, IntAlreadyLowAtSetupRunsTheLoop) {
  this->int_pin_.set_inverted(false);  // INT asserted before the ISR is attached
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_in_loop_state());

  this->int_pin_.set_inverted(true);
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_idle());
}

TEST_F(CachedGpioComponentTest, PollingLoopNeverDisablesItself) {
  this->component_.setup();
  this->component_.pin_mode(0, gpio::FLAG_INPUT);
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_in_loop_state());
}

}  // namespace esphome::gpio_expander::testing
