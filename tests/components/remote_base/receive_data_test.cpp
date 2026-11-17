#include <gtest/gtest.h>
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_base::testing {

TEST(RemoteReceiveDataTest, KeepsTheVectorItWasGiven) {
  const RawTimings timings{100, -200, 300};
  RemoteReceiveData data(timings, 25, TOLERANCE_MODE_PERCENTAGE);
  EXPECT_EQ(data.get_raw_vector(), &timings);
  EXPECT_EQ(data.get_raw_data().data(), timings.data());
  EXPECT_EQ(data.size(), 3);
}

TEST(RemoteReceiveDataTest, ViewsABufferWithoutAVector) {
  static const int32_t BUFFER[] = {100, -200, 300, -400};
  RemoteReceiveData data(RawTimingsView(BUFFER, 3), 25, TOLERANCE_MODE_PERCENTAGE);
  EXPECT_EQ(data.get_raw_vector(), nullptr);
  EXPECT_EQ(data.get_raw_data().data(), BUFFER);
  EXPECT_EQ(data.size(), 3);
  EXPECT_TRUE(data.expect_mark(100));
}

// The AOK, Cardin S438 and Drayton sync scans rely on the range accepting exactly what the peeks do
TEST(RemoteReceiveDataTest, TimingRangeAcceptsWhatThePeeksAccept) {
  RawTimings timings{0};
  for (const auto mode : {TOLERANCE_MODE_PERCENTAGE, TOLERANCE_MODE_TIME}) {
    // A time tolerance longer than the length puts the lower bound below zero
    for (const uint32_t tolerance : {0U, 25U, 100U, 700U}) {
      if (mode == TOLERANCE_MODE_PERCENTAGE && tolerance > 100)
        continue;
      for (const uint32_t length : {1U, 285U, 600U, 5000U}) {
        for (int32_t value = -7000; value <= 7000; value++) {
          timings[0] = value;
          const RemoteReceiveData src(timings, tolerance, mode);
          const TimingRange range = src.timing_range(length);
          SCOPED_TRACE(::testing::Message() << mode << " " << tolerance << " " << length << " " << value);
          ASSERT_EQ(range.matches_mark(value), src.peek_mark(length));
          ASSERT_EQ(range.matches_mark(-value), src.peek_space(length));
          ASSERT_EQ(value >= range.min, src.peek_mark_at_least(length));
          ASSERT_EQ(-value >= range.min, src.peek_space_at_least(length));
        }
      }
    }
  }
}

}  // namespace esphome::remote_base::testing
