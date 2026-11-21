#include <gtest/gtest.h>

#include <string>

#include "esphome/core/gpio.h"

namespace esphome::core::testing {
namespace {

class PulseRecordingPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override { this->events += value ? 'H' : 'L'; }

  std::string events;
};

}  // namespace

TEST(PulsePin, HighPulseReturnsLow) {
  PulseRecordingPin pin;
  pulse_pin(&pin, true, 1);
  EXPECT_EQ(pin.events, "HL");
}

TEST(PulsePin, LowPulseReturnsHigh) {
  PulseRecordingPin pin;
  pulse_pin(&pin, false, 1);
  EXPECT_EQ(pin.events, "LH");
}

TEST(PulsePin, ZeroWidthWritesBackToBack) {
  PulseRecordingPin pin;
  pulse_pin(&pin, true);
  EXPECT_EQ(pin.events, "HL");
}

}  // namespace esphome::core::testing
