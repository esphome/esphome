#include <string>
#include "common.h"
#include "esphome/components/text_sensor/filter.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::ld2450::testing {

namespace {
// Marks the values that went through the filters
class MarkFilter : public text_sensor::Filter {
 public:
  bool new_value(std::string &value) override {
    value += '!';
    return true;
  }
};
}  // namespace

// The xy_unfiltered button window on a target X/Y text sensor with a filter, for a target that does not move
class LD2450XYUnfilteredTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->xy_.add_filter(&this->mark_);
    this->xy_.add_on_raw_state_callback([this](const std::string &) { this->raw_calls_++; });
    this->ld2450_.set_target_xy_text_sensor(0, &this->xy_);
  }
  void TearDown() override { this->ld2450_.cancel_timeout(XY_UNFILTERED_TIMEOUT_ID); }

  /// Runs the deferred start of the window
  void run_deferred_() {
    App.scheduler.process_to_add();
    App.scheduler.call(millis());
  }
  /// What the window's timeout runs when it ends
  void end_window_() { this->ld2450_.set_xy_unfiltered_(false); }

  MarkFilter mark_;
  text_sensor::TextSensor xy_;
  TestableLD2450 ld2450_;
  int raw_calls_{0};
};

TEST_F(LD2450XYUnfilteredTest, ShowsTheCurrentValueThenFiltersItAgain) {
  this->xy_.publish_state("1,2");
  ASSERT_EQ(this->xy_.state, "1,2!");
  this->ld2450_.start_xy_unfiltered();
  EXPECT_FALSE(this->ld2450_.xy_unfiltered_);  // deferred, as a press from on_raw_value comes mid-publish
  this->run_deferred_();
  EXPECT_TRUE(this->ld2450_.xy_unfiltered_);
  EXPECT_EQ(this->xy_.state, "1,2");  // shown without waiting for a new frame
  EXPECT_EQ(this->raw_calls_, 2);

  this->ld2450_.start_xy_unfiltered();  // a second press only extends the window
  this->run_deferred_();
  EXPECT_EQ(this->raw_calls_, 2);

  this->end_window_();
  EXPECT_FALSE(this->ld2450_.xy_unfiltered_);
  EXPECT_EQ(this->xy_.state, "1,2!");  // filtered again
  EXPECT_EQ(this->raw_calls_, 2);      // without calling on_raw_value again
}

// After a link loss the sensor keeps the value it has (unknown), it is not rebuilt from the target data
TEST_F(LD2450XYUnfilteredTest, KeepsTheValueItHas) {
  this->xy_.publish_state("unknown");
  this->ld2450_.start_xy_unfiltered();
  this->run_deferred_();
  EXPECT_EQ(this->xy_.state, "unknown");
  this->end_window_();
  EXPECT_EQ(this->xy_.state, "unknown!");
}

TEST_F(LD2450XYUnfilteredTest, LeavesASensorWithoutValueAlone) {
  this->ld2450_.start_xy_unfiltered();
  this->run_deferred_();
  this->end_window_();
  EXPECT_FALSE(this->xy_.has_state());
  EXPECT_EQ(this->raw_calls_, 0);
}

}  // namespace esphome::ld2450::testing
