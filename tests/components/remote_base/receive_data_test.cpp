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

}  // namespace esphome::remote_base::testing
