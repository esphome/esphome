#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

TEST(HighFrequencyLoopRequester, SetFollowsTheValue) {
  HighFrequencyLoopRequester requester;
  requester.set(true);
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  requester.set(false);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

TEST(HighFrequencyLoopRequester, RepeatedValueCountsOnce) {
  HighFrequencyLoopRequester first;
  HighFrequencyLoopRequester second;
  first.set(true);
  first.set(true);
  second.start();
  first.set(false);
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  second.stop();
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  second.set(false);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

}  // namespace esphome::core::testing
