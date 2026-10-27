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

TEST(SwitchRestore, InitialWriteStateAppliesInversion) {
  RecordingSwitch on, inverted, disabled;
  on.set_restore_mode(SWITCH_ALWAYS_ON);
  inverted.set_restore_mode(SWITCH_ALWAYS_ON);
  inverted.set_inverted(true);
  disabled.set_restore_mode(SWITCH_RESTORE_DISABLED);
  disabled.set_inverted(true);
  EXPECT_EQ(on.get_initial_write_state(), optional<bool>(true));
  EXPECT_EQ(inverted.get_initial_write_state(), optional<bool>(false));
  EXPECT_FALSE(disabled.get_initial_write_state().has_value());
}

}  // namespace esphome::switch_::testing
