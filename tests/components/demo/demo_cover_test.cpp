#include <gtest/gtest.h>

#include "esphome/components/demo/demo_cover.h"
#include "esphome/core/application.h"

namespace esphome::demo::testing {

using cover::COVER_CLOSED;
using cover::COVER_OPERATION_IDLE;
using cover::COVER_OPERATION_OPENING;

// Type 4 is the demo cover with a stop button. It starts closed.
TEST(DemoCoverTest, StopEndsMovement) {
  DemoCover cover;
  cover.set_type(DemoCoverType::TYPE_4);
  cover.setup();

  cover.make_call().set_command_open().perform();
  EXPECT_EQ(cover.current_operation, COVER_OPERATION_OPENING);

  cover.make_call().set_command_stop().perform();
  EXPECT_EQ(cover.current_operation, COVER_OPERATION_IDLE);
  EXPECT_EQ(cover.position, COVER_CLOSED);
  // MOVE_TIMEOUT_ID (0) is gone, so the move cannot finish 2 s later
  EXPECT_FALSE(App.scheduler.cancel_timeout(&cover, uint32_t{0}));
}

}  // namespace esphome::demo::testing
