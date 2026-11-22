#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome::core::testing {
namespace {

class PulseRecordingPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override {
    this->events += value ? 'H' : 'L';
    this->write_us.push_back(micros());
  }

  std::string events;
  std::vector<uint32_t> write_us;
};

}  // namespace

TEST(PulsePin, WritesLevelThenOpposite) {
  struct Case {
    bool level;
    uint32_t width_us;
    const char *events;
  };
  for (const Case &c : {Case{true, 1, "HL"}, Case{false, 1, "LH"}, Case{true, 0, "HL"}, Case{false, 0, "LH"}}) {
    PulseRecordingPin pin;
    pulse_pin(&pin, c.level, c.width_us);
    EXPECT_EQ(pin.events, c.events) << "level " << c.level << ", width " << c.width_us;
  }
}

TEST(PulsePin, DefaultWidthWritesBackToBack) {
  PulseRecordingPin pin;
  pulse_pin(&pin, true);
  EXPECT_EQ(pin.events, "HL");
}

// Only a lower bound: a busy host can run long
TEST(PulsePin, HoldsTheLevelForTheWidth) {
  PulseRecordingPin pin;
  pulse_pin(&pin, true, 5000);
  ASSERT_EQ(pin.write_us.size(), 2u);
  EXPECT_GE(pin.write_us[1] - pin.write_us[0], 5000u);
}

}  // namespace esphome::core::testing
