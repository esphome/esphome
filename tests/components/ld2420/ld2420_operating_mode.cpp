#include <cstring>
#include <gtest/gtest.h>
#include "esphome/components/ld2420/ld2420.h"
#include "esphome/components/select/select.h"

namespace esphome::ld2420::testing {

class TestSelect : public select::Select {
 public:
  TestSelect() { this->traits.set_options({"Normal", "Calibrate", "Simple"}); }

 protected:
  void control(size_t index) override {}
};

class LD2420OperatingModeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 1.5.4 is the first firmware with calibration
    this->set_firmware_("v1.5.4");
    this->ld2420_.set_operating_mode_select(&this->select_);
  }
  // The fixture is a friend of the hub, so it reaches the version and calibration flag for the tests
  void set_firmware_(const char *version) {
    std::strncpy(this->ld2420_.firmware_ver_, version, sizeof(this->ld2420_.firmware_ver_));
  }
  void expect_mode_(OpMode mode, size_t index, bool calibrating) {
    EXPECT_EQ(this->ld2420_.current_operating_mode, static_cast<uint8_t>(mode));
    EXPECT_EQ(this->select_.active_index().value_or(SIZE_MAX), index);
    EXPECT_EQ(this->ld2420_.calibration_, calibrating);
  }

  LD2420Component ld2420_;
  TestSelect select_;
};

TEST_F(LD2420OperatingModeTest, NamesMapToModesAndSelectIndex) {
  this->ld2420_.set_operating_mode("Calibrate");
  this->expect_mode_(OP_CALIBRATE_MODE, 1, true);
  this->ld2420_.set_operating_mode("Simple");
  this->expect_mode_(OP_SIMPLE_MODE, 2, false);
  this->ld2420_.set_operating_mode("Normal");
  this->expect_mode_(OP_NORMAL_MODE, 0, false);
}

TEST_F(LD2420OperatingModeTest, EnumEntryPointMatchesTheSelectIndex) {
  this->ld2420_.set_operating_mode(OP_CALIBRATE_MODE);
  this->expect_mode_(OP_CALIBRATE_MODE, 1, true);
  this->ld2420_.set_operating_mode(OP_NORMAL_MODE);
  this->expect_mode_(OP_NORMAL_MODE, 0, false);
}

TEST_F(LD2420OperatingModeTest, UnknownNameLeavesTheModeUnchanged) {
  this->ld2420_.set_operating_mode(OP_CALIBRATE_MODE);
  this->ld2420_.set_operating_mode("Bogus");
  this->expect_mode_(OP_CALIBRATE_MODE, 1, true);
}

TEST_F(LD2420OperatingModeTest, OlderFirmwareOnlySupportsSimpleMode) {
  this->set_firmware_("v1.5.3");
  this->ld2420_.set_operating_mode("Calibrate");
  this->expect_mode_(OP_SIMPLE_MODE, 2, false);
  this->ld2420_.set_operating_mode(OP_NORMAL_MODE);
  this->expect_mode_(OP_SIMPLE_MODE, 2, false);
}

}  // namespace esphome::ld2420::testing
