#include <gtest/gtest.h>

#include <string>

#include "esphome/core/gpio.h"

namespace esphome::core::testing {
namespace {

class SetupRecordingPin : public GPIOPin {
 public:
  void setup() override { this->events += 'S'; }
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override { this->events += value ? 'H' : 'L'; }

  std::string events;
};

}  // namespace

TEST(SetupOutputPin, NullPinIsIgnored) { setup_output_pin(nullptr, true); }

TEST(SetupOutputPin, SetsUpBeforeWritingTheLevel) {
  SetupRecordingPin low;
  setup_output_pin(&low, false);
  EXPECT_EQ(low.events, "SL");
  SetupRecordingPin high;
  setup_output_pin(&high, true);
  EXPECT_EQ(high.events, "SH");
}

}  // namespace esphome::core::testing
