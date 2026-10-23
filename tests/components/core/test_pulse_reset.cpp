#include <gtest/gtest.h>

#include <string>

#include "esphome/core/gpio.h"

namespace esphome::core::testing {

class RecordingPin : public GPIOPin {
 public:
  void setup() override { this->events += 'S'; }
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override { this->events += value ? 'H' : 'L'; }

  std::string events;
};

TEST(PulseReset, NullPinIsIgnored) { pulse_reset(nullptr, 1, 1, 1); }

TEST(PulseReset, LowThenHigh) {
  RecordingPin pin;
  pulse_reset(&pin, 1);
  EXPECT_EQ(pin.events, "SLH");
}

TEST(PulseReset, PreHighDrivesHighFirst) {
  RecordingPin pin;
  pulse_reset(&pin, 1, 1, 1);
  EXPECT_EQ(pin.events, "SHLH");
}

}  // namespace esphome::core::testing
