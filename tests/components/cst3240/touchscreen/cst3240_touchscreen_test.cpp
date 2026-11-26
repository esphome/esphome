#include <gtest/gtest.h>

#include "esphome/components/cst3240/touchscreen/cst3240_touchscreen.h"
#include "esphome/core/gpio.h"
#include "../common.h"

namespace esphome::cst3240::testing {

class TestableCST3240 : public CST3240Touchscreen {
 public:
  using CST3240Touchscreen::chip_id_;
  using CST3240Touchscreen::is_touched_;
  using CST3240Touchscreen::setup_complete_;
  using CST3240Touchscreen::skip_update_;
  using CST3240Touchscreen::update_touches;
  using CST3240Touchscreen::x_raw_max_;
  using CST3240Touchscreen::y_raw_max_;
};

class CST3240Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.registers[cstxxx::REG_CHIP_INFO] = {0x01, 0x00, 0x40, 0x32};
    this->bus_.registers[cstxxx::REG_RESOLUTION] = {0x40, 0x01, 0xF0, 0x00};
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.setup();
    this->bus_.writes.clear();
  }

  void update_() {
    this->hub_.skip_update_ = false;
    this->hub_.is_touched_ = false;
    this->hub_.update_touches();
  }

  bool cleared_() const {
    return this->bus_.writes.size() == 1 && this->bus_.writes[0].reg == cstxxx::REG_TOUCH_DATA &&
           this->bus_.writes[0].size == 2u + CST3240_REPORT_SIZE && this->bus_.writes[0].zero_data;
  }

  RegisterBus bus_;
  TestableCST3240 hub_;
};

TEST_F(CST3240Test, SetupReadsTheChipAndResolutionWithoutAFirmwareCheck) {
  EXPECT_TRUE(this->hub_.setup_complete_);
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_EQ(this->hub_.chip_id_, CST3240_CHIP_ID);
  EXPECT_EQ(this->hub_.x_raw_max_, 320);
  EXPECT_EQ(this->hub_.y_raw_max_, 240);
}

TEST_F(CST3240Test, ReportWithoutTheSyncByteIsSkippedAndCleared) {
  this->bus_.set_report(1, 0x55);
  this->update_();
  EXPECT_TRUE(this->hub_.skip_update_);
  EXPECT_FALSE(this->hub_.is_touched_);
  EXPECT_TRUE(this->cleared_());
}

TEST_F(CST3240Test, IdleBlockIsNotRewritten) {
  this->bus_.set_report(0, 0);
  this->update_();
  EXPECT_TRUE(this->hub_.skip_update_);
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(CST3240Test, ReleasedRecordIsNotATouch) {
  this->bus_.set_report(1, cstxxx::SYNC_BYTE, 0);
  this->update_();
  EXPECT_FALSE(this->hub_.skip_update_);
  EXPECT_FALSE(this->hub_.is_touched_);
  EXPECT_TRUE(this->hub_.get_touches().empty());
}

TEST_F(CST3240Test, PollingClearsTheReport) {
  this->bus_.set_report(1);
  this->update_();
  EXPECT_TRUE(this->hub_.is_touched_);
  EXPECT_TRUE(this->cleared_());
}

TEST_F(CST3240Test, InterruptAcknowledgesTheReport) {
  InternalGPIOPin pin;
  this->hub_.set_interrupt_pin(&pin);
  this->bus_.set_report(1);
  this->update_();
  ASSERT_EQ(this->bus_.writes.size(), 2u);
  EXPECT_EQ(this->bus_.writes[0].reg, cstxxx::REG_FINGER_COUNT);
  EXPECT_EQ(this->bus_.writes[1].reg, cstxxx::REG_TOUCH_DATA);
  EXPECT_EQ(this->bus_.writes[1].size, 3u);
}

TEST_F(CST3240Test, ReportsUpToFiveFingers) {
  this->bus_.set_report(5);
  this->update_();
  EXPECT_EQ(this->hub_.get_touches().size(), 5u);
  this->bus_.set_report(6);
  this->update_();
  EXPECT_FALSE(this->hub_.is_touched_);
}

}  // namespace esphome::cst3240::testing
