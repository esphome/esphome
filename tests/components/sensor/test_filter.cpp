#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"
#include <gtest/gtest.h>
#include <cmath>

namespace esphome::sensor {
namespace {

class FilterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default accuracy is 0, which means rounding to integers for comparisons in some filters.
    // Setting it to 2 to allow finer grained comparisons if needed.
    sensor_.set_accuracy_decimals(2);
  }

  Sensor sensor_;
};

TEST_F(FilterTest, OffsetFilter) {
  OffsetFilter filter(10.0f);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 15.0f);
}

TEST_F(FilterTest, MultiplyFilter) {
  MultiplyFilter filter(2.0f);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 10.0f);
}

TEST_F(FilterTest, FilterOutValueFilter) {
  FilterOutValueFilter filter({10.0f, 20.0f});
  sensor_.add_filter(&filter);

  float received_value = NAN;
  int callback_count = 0;
  sensor_.add_on_state_callback([&](float value) {
    received_value = value;
    callback_count++;
  });

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 5.0f);
  EXPECT_EQ(callback_count, 1);

  sensor_.publish_state(10.0f);
  EXPECT_EQ(callback_count, 1);  // Should not update

  sensor_.publish_state(20.0f);
  EXPECT_EQ(callback_count, 1);  // Should not update

  sensor_.publish_state(21.0f);
  EXPECT_FLOAT_EQ(received_value, 21.0f);
  EXPECT_EQ(callback_count, 2);
}

TEST_F(FilterTest, MinFilter) {
  MinFilter filter(/*window_size=*/3, /*send_every=*/1, /*send_first_at=*/1);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(10.0f);
  EXPECT_FLOAT_EQ(received_value, 10.0f);

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 5.0f);

  sensor_.publish_state(8.0f);
  EXPECT_FLOAT_EQ(received_value, 5.0f);

  sensor_.publish_state(12.0f);
  // Window logic:
  // 10 -> [10], min 10
  // 5 -> [10, 5], min 5
  // 8 -> [10, 5, 8], min 5
  // 12 -> [5, 8, 12] (overwrite 10), min 5
  EXPECT_FLOAT_EQ(received_value, 5.0f);

  sensor_.publish_state(20.0f);
  // Window: [8, 12, 20] (overwrite 5), min 8
  EXPECT_FLOAT_EQ(received_value, 8.0f);
}

TEST_F(FilterTest, MaxFilter) {
  MaxFilter filter(/*window_size=*/3, /*send_every=*/1, /*send_first_at=*/1);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(10.0f);
  EXPECT_FLOAT_EQ(received_value, 10.0f);

  sensor_.publish_state(15.0f);
  EXPECT_FLOAT_EQ(received_value, 15.0f);

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 15.0f);

  sensor_.publish_state(20.0f);  // [10, 15, 5] -> [15, 5, 20] (overwrite 10)
  EXPECT_FLOAT_EQ(received_value, 20.0f);
}

TEST_F(FilterTest, SlidingWindowMovingAverageFilter) {
  SlidingWindowMovingAverageFilter filter(/*window_size=*/3, /*send_every=*/1, /*send_first_at=*/1);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(10.0f);
  EXPECT_FLOAT_EQ(received_value, 10.0f);  // [10] / 1

  sensor_.publish_state(20.0f);
  EXPECT_FLOAT_EQ(received_value, 15.0f);  // [10, 20] / 2

  sensor_.publish_state(30.0f);
  EXPECT_FLOAT_EQ(received_value, 20.0f);  // [10, 20, 30] / 3

  sensor_.publish_state(40.0f);  // [20, 30, 40] / 3 = 90 / 3 = 30
  EXPECT_FLOAT_EQ(received_value, 30.0f);
}

TEST_F(FilterTest, LambdaFilter) {
  auto lambda = [](float value) -> optional<float> {
    if (value < 0)
      return {};
    return value * 2;
  };
  LambdaFilter filter(lambda);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  int callback_count = 0;
  sensor_.add_on_state_callback([&](float value) {
    received_value = value;
    callback_count++;
  });

  sensor_.publish_state(5.0f);
  EXPECT_FLOAT_EQ(received_value, 10.0f);
  EXPECT_EQ(callback_count, 1);

  sensor_.publish_state(-5.0f);
  EXPECT_EQ(callback_count, 1);  // Should be filtered out
}

TEST_F(FilterTest, ClampFilter) {
  ClampFilter filter(/*min=*/0.0f, /*max=*/100.0f, /*ignore_out_of_range=*/false);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  sensor_.add_on_state_callback([&](float value) { received_value = value; });

  sensor_.publish_state(50.0f);
  EXPECT_FLOAT_EQ(received_value, 50.0f);

  sensor_.publish_state(-10.0f);
  EXPECT_FLOAT_EQ(received_value, 0.0f);

  sensor_.publish_state(150.0f);
  EXPECT_FLOAT_EQ(received_value, 100.0f);
}

TEST_F(FilterTest, ClampFilterIgnore) {
  ClampFilter filter(/*min=*/0.0f, /*max=*/100.0f, /*ignore_out_of_range=*/true);
  sensor_.add_filter(&filter);

  float received_value = NAN;
  int callback_count = 0;
  sensor_.add_on_state_callback([&](float value) {
    received_value = value;
    callback_count++;
  });

  sensor_.publish_state(50.0f);
  EXPECT_FLOAT_EQ(received_value, 50.0f);
  EXPECT_EQ(callback_count, 1);

  sensor_.publish_state(-10.0f);
  EXPECT_EQ(callback_count, 1);  // Ignored

  sensor_.publish_state(150.0f);
  EXPECT_EQ(callback_count, 1);  // Ignored
}

}  // namespace
}  // namespace esphome::sensor
