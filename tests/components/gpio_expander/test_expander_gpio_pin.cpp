#include <gtest/gtest.h>

#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::gpio_expander::testing {

class FakeExpander {
 public:
  void pin_mode(uint8_t pin, gpio::Flags flags) {
    this->mode_pin = pin;
    this->mode_flags = flags;
  }
  bool digital_read(uint8_t pin) { return this->level; }
  void digital_write(uint8_t pin, bool value) {
    this->write_pin = pin;
    this->written = value;
  }

  uint8_t mode_pin{0xFF};
  gpio::Flags mode_flags{gpio::FLAG_NONE};
  bool level{false};
  uint8_t write_pin{0xFF};
  bool written{false};
};

using FakeExpanderPin = ExpanderGPIOPin<FakeExpander>;

}  // namespace esphome::gpio_expander::testing

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::gpio_expander::testing::FakeExpander);
ESPHOME_EXPANDER_PIN_SUMMARY(esphome::gpio_expander::testing::FakeExpander, "%u via FAKE")

namespace esphome::gpio_expander::testing {

static FakeExpanderPin make_pin(FakeExpander *parent, uint8_t pin, bool inverted, gpio::Flags flags) {
  FakeExpanderPin gpio_pin;
  gpio_pin.set_parent(parent);
  gpio_pin.set_pin(pin);
  gpio_pin.set_inverted(inverted);
  gpio_pin.set_flags(flags);
  return gpio_pin;
}

TEST(ExpanderGPIOPin, SetupAppliesConfiguredFlags) {
  FakeExpander parent;
  auto pin = make_pin(&parent, 3, false, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  pin.setup();
  EXPECT_EQ(parent.mode_pin, 3);
  EXPECT_EQ(parent.mode_flags, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  EXPECT_EQ(pin.get_flags(), gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
}

TEST(ExpanderGPIOPin, ReadAndWriteHonourInversion) {
  FakeExpander parent;
  auto plain = make_pin(&parent, 1, false, gpio::FLAG_OUTPUT);
  auto inverted = make_pin(&parent, 2, true, gpio::FLAG_OUTPUT);

  parent.level = true;
  EXPECT_TRUE(plain.digital_read());
  EXPECT_FALSE(inverted.digital_read());

  inverted.digital_write(true);
  EXPECT_EQ(parent.write_pin, 2);
  EXPECT_FALSE(parent.written);
  plain.digital_write(true);
  EXPECT_EQ(parent.write_pin, 1);
  EXPECT_TRUE(parent.written);
}

TEST(ExpanderGPIOPin, SummaryUsesTheExpanderFormat) {
  FakeExpander parent;
  auto pin = make_pin(&parent, 7, false, gpio::FLAG_INPUT);
  char buffer[32];
  pin.dump_summary(buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "7 via FAKE");
}

}  // namespace esphome::gpio_expander::testing
