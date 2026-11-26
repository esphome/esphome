#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include "esphome/components/lvgl/lvgl_esphome.h"

namespace esphome::lvgl::testing {

// The largest float below 2^31, as in chart_point_value()
static constexpr int32_t POINT_LIMIT = 2147483520;

TEST(ChartPointValueTest, ScalesAndRoundsHalfAwayFromZero) {
  EXPECT_EQ(chart_point_value(1.25f, 10.0f), 13);
  EXPECT_EQ(chart_point_value(-2.5f, 1.0f), -3);
  EXPECT_EQ(chart_point_value(2.4f, 1.0f), 2);
  EXPECT_EQ(chart_point_value(0.5f, 100.0f), 50);
  // The same values as test_scale_value in test_chart.py, which round differently in 64-bit floats
  EXPECT_EQ(chart_point_value(311.835f, 100.0f), 31184);
  EXPECT_EQ(chart_point_value(-303.025f, 100.0f), -30303);
}

TEST(ChartPointValueTest, NanLeavesAGap) { EXPECT_EQ(chart_point_value(NAN, 10.0f), LV_CHART_POINT_NONE); }

TEST(ChartPointValueTest, LimitsToTheInt32RangeWithoutMakingAGap) {
  EXPECT_EQ(chart_point_value(1e12f, 1.0f), POINT_LIMIT);
  EXPECT_EQ(chart_point_value(-1e12f, 1.0f), -POINT_LIMIT);
  EXPECT_EQ(chart_point_value(std::numeric_limits<float>::infinity(), 1.0f), POINT_LIMIT);
  EXPECT_NE(chart_point_value(3e9f, 1.0f), LV_CHART_POINT_NONE);
}

TEST(ChartStackedPointValueTest, StoresValuesOfZeroOrLessAsGaps) {
  EXPECT_EQ(chart_stacked_point_value(0.0f, 1.0f), LV_CHART_POINT_NONE);
  EXPECT_EQ(chart_stacked_point_value(0.4f, 1.0f), LV_CHART_POINT_NONE);
  EXPECT_EQ(chart_stacked_point_value(-3.0f, 1.0f), LV_CHART_POINT_NONE);
  EXPECT_EQ(chart_stacked_point_value(NAN, 1.0f), LV_CHART_POINT_NONE);
  EXPECT_EQ(chart_stacked_point_value(0.4f, 10.0f), 4);
}

class ChartPointSlotTest : public ::testing::Test {
 protected:
  static constexpr uint32_t POINT_COUNT = 5;

  static void SetUpTestSuite() {
    if (!lv_is_initialized())
      lv_init();
    // Objects need a display to belong to; nothing is drawn
    if (lv_display_get_default() == nullptr)
      lv_display_create(100, 100);
  }

  void SetUp() override {
    this->chart_ = lv_chart_create(lv_screen_active());
    lv_chart_set_point_count(this->chart_, POINT_COUNT);
    this->series_ = lv_chart_add_series(this->chart_, lv_color_black(), LV_CHART_AXIS_PRIMARY_Y);
  }

  void TearDown() override { lv_obj_delete(this->chart_); }

  // Appends 0, 1, ... count - 1
  void append_(int32_t count) {
    for (int32_t value = 0; value < count; value++)
      lv_chart_set_next_value(this->chart_, this->series_, value);
  }

  // Appends the points (0, 0), (1, 1), ... (count - 1, count - 1)
  void append_points_(int32_t count) {
    for (int32_t value = 0; value < count; value++)
      lv_chart_set_next_value2(this->chart_, this->series_, value, value);
  }

  // The value drawn at `index` from the left
  int32_t value_at_(uint32_t index) {
    return lv_chart_get_series_y_array(this->chart_,
                                       this->series_)[chart_point_slot(this->chart_, this->series_, index)];
  }

  lv_obj_t *chart_{nullptr};
  lv_chart_series_t *series_{nullptr};
};

TEST_F(ChartPointSlotTest, ShiftCountsFromTheOldestValue) {
  lv_chart_set_update_mode(this->chart_, LV_CHART_UPDATE_MODE_SHIFT);
  this->append_(7);  // 0 and 1 have been shifted out
  EXPECT_EQ(this->value_at_(0), 2);
  EXPECT_EQ(this->value_at_(4), 6);
}

TEST_F(ChartPointSlotTest, CircularCountsFromTheFirstSlot) {
  lv_chart_set_update_mode(this->chart_, LV_CHART_UPDATE_MODE_CIRCULAR);
  this->append_(7);  // 5 and 6 have replaced 0 and 1
  EXPECT_EQ(this->value_at_(0), 5);
  EXPECT_EQ(this->value_at_(2), 2);
}

TEST_F(ChartPointSlotTest, OutOfRangeIndexIsReturnedUnchanged) {
  lv_chart_set_update_mode(this->chart_, LV_CHART_UPDATE_MODE_SHIFT);
  this->append_(3);
  EXPECT_EQ(chart_point_slot(this->chart_, this->series_, POINT_COUNT), POINT_COUNT);
  EXPECT_EQ(chart_point_slot(this->chart_, this->series_, UINT32_MAX), UINT32_MAX);
}

class ChartScatterPointTest : public ChartPointSlotTest {
 protected:
  void SetUp() override {
    this->chart_ = lv_chart_create(lv_screen_active());
    // X values are only kept for a series added to a scatter chart
    lv_chart_set_type(this->chart_, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(this->chart_, POINT_COUNT);
    lv_chart_set_update_mode(this->chart_, LV_CHART_UPDATE_MODE_CIRCULAR);
    this->series_ = lv_chart_add_series(this->chart_, lv_color_black(), LV_CHART_AXIS_PRIMARY_Y);
  }

  int32_t x_at_(uint32_t slot) { return lv_chart_get_series_x_array(this->chart_, this->series_)[slot]; }
  int32_t y_at_(uint32_t slot) { return lv_chart_get_series_y_array(this->chart_, this->series_)[slot]; }
};

TEST_F(ChartScatterPointTest, AppendsScaledPoints) {
  chart_set_next_point(this->chart_, this->series_, 1.5f, 2.25f, 10.0f, 100.0f);
  EXPECT_EQ(this->x_at_(0), 15);
  EXPECT_EQ(this->y_at_(0), 225);
}

TEST_F(ChartScatterPointTest, NanInEitherCoordinateLeavesAGap) {
  chart_set_next_point(this->chart_, this->series_, NAN, 2.0f, 1.0f, 1.0f);
  chart_set_next_point(this->chart_, this->series_, 1.0f, NAN, 1.0f, 1.0f);
  // LVGL only checks Y for a gap
  EXPECT_EQ(this->y_at_(0), LV_CHART_POINT_NONE);
  EXPECT_EQ(this->y_at_(1), LV_CHART_POINT_NONE);
  EXPECT_EQ(this->x_at_(1), 1);
}

TEST_F(ChartScatterPointTest, SetsAPointByIndex) {
  this->append_points_(3);
  chart_set_point(this->chart_, this->series_, 1, NAN, 7.0f, 1.0f, 1.0f);
  chart_set_point(this->chart_, this->series_, 2, 8.0f, 9.0f, 1.0f, 1.0f);
  EXPECT_EQ(this->y_at_(1), LV_CHART_POINT_NONE);
  EXPECT_EQ(this->x_at_(2), 8);
  EXPECT_EQ(this->y_at_(2), 9);
}

}  // namespace esphome::lvgl::testing
