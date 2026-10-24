#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/switch/switch.h"

namespace esphome::switch_::testing {

class RecordingSwitch : public Switch {
 public:
  std::vector<bool> writes;

 protected:
  void write_state(bool state) override {
    this->writes.push_back(state);
    this->publish_state(state);
  }
};

TEST(SwitchRestore, DrivesTheRestoreModeStateThroughInversion) {
  RecordingSwitch on, off, inverted;
  on.set_restore_mode(SWITCH_ALWAYS_ON);
  off.set_restore_mode(SWITCH_ALWAYS_OFF);
  inverted.set_restore_mode(SWITCH_ALWAYS_ON);
  inverted.set_inverted(true);
  on.restore();
  off.restore();
  inverted.restore();
  EXPECT_EQ(on.writes, std::vector<bool>{true});
  EXPECT_EQ(off.writes, std::vector<bool>{false});
  EXPECT_EQ(inverted.writes, std::vector<bool>{false});  // the hardware level
  EXPECT_TRUE(on.state);
  EXPECT_FALSE(off.state);
  EXPECT_TRUE(inverted.state);
}

TEST(SwitchRestore, DefaultAppliesOnlyWhenTheModeIsDisabled) {
  RecordingSwitch disabled, on;
  disabled.set_restore_mode(SWITCH_RESTORE_DISABLED);
  on.set_restore_mode(SWITCH_ALWAYS_ON);
  disabled.restore();
  EXPECT_TRUE(disabled.writes.empty());
  disabled.restore(true);
  on.restore(false);
  EXPECT_EQ(disabled.writes, std::vector<bool>{true});
  EXPECT_EQ(on.writes, std::vector<bool>{true});
}

}  // namespace esphome::switch_::testing
