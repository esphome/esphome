#include <gtest/gtest.h>

#include "esphome/components/udp/udp_component.h"

#ifdef USE_HOST

namespace esphome::udp::testing {

TEST(NextRetryDelay, FirstRetryUsesInitialDelay) { EXPECT_EQ(next_retry_delay_ms(0), RETRY_INITIAL_MS); }

TEST(NextRetryDelay, DoublesUntilCapped) {
  uint32_t delay = 0;
  const uint32_t expected[] = {1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000};
  for (uint32_t want : expected) {
    delay = next_retry_delay_ms(delay);
    EXPECT_EQ(delay, want);
  }
}

TEST(NextRetryDelay, NeverExceedsMax) { EXPECT_EQ(next_retry_delay_ms(RETRY_MAX_MS - 1), RETRY_MAX_MS); }

}  // namespace esphome::udp::testing

#endif  // USE_HOST
