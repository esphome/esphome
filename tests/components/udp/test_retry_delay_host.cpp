#include <gtest/gtest.h>

#include "esphome/components/udp/udp_component.h"

#ifdef USE_HOST

namespace esphome::udp::testing {

TEST(NextRetryDelay, DoublesUntilCapped) {
  uint32_t delay = 0;
  for (uint32_t want : {1000u, 2000u, 4000u, 8000u, 16000u, 32000u, 60000u, 60000u}) {
    delay = next_retry_delay_ms(delay);
    EXPECT_EQ(delay, want);
  }
}

}  // namespace esphome::udp::testing

#endif  // USE_HOST
