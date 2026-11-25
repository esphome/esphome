#include "sx127x_test_helpers.h"
#include <string>

namespace esphome::sx127x::testing {

// A configure() from the warning's log callback recovers the radio, and no retry is left
// behind to reset it again later
TEST(SX127xReentry, ConfigureFromTheFailureWarningLeavesNoRetry) {
  RadioHardware hw;
  LogCallback log;
  hw.configure(true, true);
  bool reentered = false;
  log.set_callback([&](const std::string &message) {
    if (!reentered && message.find("set Warning flag") != std::string::npos) {
      reentered = true;
      hw.stuck = false;
      hw.radio.configure();
    }
  });
  hw.stuck = true;
  hw.radio.set_mode_standby();
  ASSERT_TRUE(reentered);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving());
  hw.radio.set_mode_sleep();
  run_scheduler(1100);
  EXPECT_EQ(hw.mode(), MODE_SLEEP);
}

}  // namespace esphome::sx127x::testing
