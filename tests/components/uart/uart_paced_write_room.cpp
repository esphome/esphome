#include <cstdint>

#include "common.h"
#include "esphome/core/application.h"

namespace esphome::uart::testing {

class RoomUART : public MockUARTComponent {
 public:
  size_t available_for_write() override { return this->room; }
  size_t room{SIZE_MAX};
};

class PacedWriteRoom : public ::testing::Test {
 protected:
  void SetUp() override {
    App.set_loop_interval(16);
    this->uart_.set_baud_rate(9600);
  }
  void TearDown() override {
    App.set_loop_interval(16);
    this->at_(0);
  }
  // Publishes now_ms as the loop start time, as Application::loop() does.
  void at_(uint32_t now_ms) { LoopBlockingGuard dispatch{nullptr, nullptr, now_ms}; }

  RoomUART uart_;
};

TEST_F(PacedWriteRoom, KnownRoomIsReturned) {
  this->uart_.room = 7;
  this->at_(1000);
  EXPECT_EQ(this->uart_.paced_write_room(0), 7u);
  this->uart_.room = 0;
  EXPECT_EQ(this->uart_.paced_write_room(0), 0u);
}

TEST_F(PacedWriteRoom, UnknownRoomTakesOneLoopInterval) {
  this->at_(1000);
  // 9600 baud at 10 bits per byte for 16 ms.
  EXPECT_EQ(this->uart_.paced_write_room(0), 15u);
}

TEST_F(PacedWriteRoom, EarlyPassTakesTheTimeSinceTheLastWrite) {
  this->at_(1005);
  EXPECT_EQ(this->uart_.paced_write_room(1000), 4u);
}

TEST_F(PacedWriteRoom, AtLeastOneByte) {
  this->at_(1000);
  EXPECT_EQ(this->uart_.paced_write_room(1000), 1u);
}

TEST_F(PacedWriteRoom, SpanCapsAtFourSeconds) {
  App.set_loop_interval(10000);
  this->uart_.set_baud_rate(300);
  this->at_(6000);
  // 30 bytes/s for 4 s.
  EXPECT_EQ(this->uart_.paced_write_room(0), 120u);
}

}  // namespace esphome::uart::testing
