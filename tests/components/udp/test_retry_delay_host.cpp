#include <gtest/gtest.h>

#include "esphome/components/udp/udp_component.h"

#ifdef USE_HOST

namespace esphome::udp::testing {

TEST(NextRetryDelay, DoublesUntilCapped) {
  uint16_t delay = 0;
  for (uint16_t want : {1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000}) {
    delay = next_retry_delay_ms(delay);
    EXPECT_EQ(delay, want);
  }
}

}  // namespace esphome::udp::testing

#endif  // USE_HOST
