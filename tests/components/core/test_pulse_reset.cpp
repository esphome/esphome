#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome::core::testing {

class RecordingPin : public GPIOPin {
 public:
  void setup() override { this->events += 'S'; }
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override {
    this->events += value ? 'H' : 'L';
    this->write_ms.push_back(millis());
  }

  std::string events;
  std::vector<uint32_t> write_ms;
};

TEST(PulseReset, NullPinIsIgnored) { pulse_reset(nullptr, {.low_ms = 1}); }

TEST(PulseReset, LowThenHigh) {
  RecordingPin pin;
  pulse_reset(&pin, {.low_ms = 1});
  EXPECT_EQ(pin.events, "SLH");
}

TEST(PulseReset, PreHighDrivesHighFirst) {
  RecordingPin pin;
  pulse_reset(&pin, {.high_before_ms = 1, .low_ms = 1, .high_after_ms = 1});
  EXPECT_EQ(pin.events, "SHLH");
}

// Distinct waits, so dropping or swapping one shows up; only lower bounds, a busy host can run long
TEST(PulseReset, HoldsEachLevelForItsTime) {
  RecordingPin pin;
  pulse_reset(&pin, {.high_before_ms = 10, .low_ms = 20, .high_after_ms = 30});
  const uint32_t end = millis();
  ASSERT_EQ(pin.events, "SHLH");
  EXPECT_GE(pin.write_ms[1] - pin.write_ms[0], 10u);
  EXPECT_GE(pin.write_ms[2] - pin.write_ms[1], 20u);
  EXPECT_GE(end - pin.write_ms[2], 30u);
}

}  // namespace esphome::core::testing
