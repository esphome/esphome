#include <algorithm>
#include <cstdint>

#include "common.h"
#include "esphome/core/application.h"

namespace esphome::uart::testing {

class RoomUART : public MockUARTComponent {
 public:
  size_t available_for_write() override { return this->room; }
  using UARTComponent::paced_room_;
  using UARTComponent::paced_stamp_;
  size_t room{SIZE_MAX};
};

class PacedWriteRoom : public ::testing::Test {
 protected:
  void SetUp() override {
    App.set_loop_interval(16);
    this->uart_.set_baud_rate(9600);
  }
  void TearDown() override { App.set_loop_interval(16); }

  // The bytes handed out by a pass every pass_us up to end_us, each write returning at once.
  size_t passes_(uint32_t pass_us, uint32_t end_us) {
    uint32_t last_write_us = 0;
    size_t total = 0;
    for (uint32_t now_us = pass_us; now_us <= end_us; now_us += pass_us) {
      total += this->uart_.paced_room_(now_us - last_write_us);
      last_write_us = this->uart_.paced_stamp_(now_us, last_write_us);
    }
    return total;
  }

  RoomUART uart_;
};

TEST_F(PacedWriteRoom, KnownRoomIsReturned) {
  this->uart_.room = 7;
  EXPECT_EQ(this->uart_.paced_write_room(0), 7u);
  this->uart_.room = 0;
  EXPECT_EQ(this->uart_.paced_write_room(0), 0u);
}

struct PassesCase {
  uint32_t baud;
  uint32_t pass_us;
  uint32_t end_us;
  size_t expected;
};

class PacedPasses : public PacedWriteRoom, public ::testing::WithParamInterface<PassesCase> {};

// Each pass keeps the part of a byte for the next, so the total is what the line sent, rounded down once
TEST_P(PacedPasses, HandOutWhatTheLineSent) {
  this->uart_.set_baud_rate(GetParam().baud);
  EXPECT_EQ(this->passes_(GetParam().pass_us, GetParam().end_us), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(PacedWriteRoom, PacedPasses,
                         ::testing::Values(PassesCase{19200, 1000, 10000, 19},      // 19.2 bytes
                                           PassesCase{9600, 16000, 160000, 153}));  // 153.6 bytes

struct RoomCase {
  uint32_t baud;
  uint32_t span_us;
  size_t expected;
};

class PacedRoom : public PacedWriteRoom, public ::testing::WithParamInterface<RoomCase> {};

TEST_P(PacedRoom, IsTheWholeBytesSentCappedAtALoopIntervalAndAByte) {
  this->uart_.set_baud_rate(GetParam().baud);
  EXPECT_EQ(this->uart_.paced_room_(GetParam().span_us), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(PacedWriteRoom, PacedRoom,
                         ::testing::Values(RoomCase{9600, 5000, 4},          // 4.8 bytes
                                           RoomCase{9600, 1000000, 16},      // 15.36 + 1
                                           RoomCase{921600, 1000000, 1475},  // 1474.56 + 1
                                           RoomCase{9600, 0, 1},             // at least one byte
                                           RoomCase{0, 1000000, 1}));        // ble_nus, usb_cdc_acm: no baud rate

TEST_F(PacedWriteRoom, StampKeepsThePartOfAByte) {
  // 4.8 bytes in 5 ms at 9600 baud: the stamp moves on 4 bytes of 1041.7 us
  EXPECT_EQ(this->uart_.paced_stamp_(5000, 0), 4167u);
}

TEST_F(PacedWriteRoom, StreamNeitherFillsNorDrainsTheBuffer) {
  // usb_uart and weikai drop what does not fit their buffer: after the first 31 bytes it stays at 30 or 31
  this->uart_.set_baud_rate(19200);
  static constexpr uint32_t PASS_US[] = {1000, 1300, 1700};
  const uint32_t start_us = 10000000;
  uint32_t now_us = start_us;
  uint32_t last_write_us = 0;
  uint32_t handed_out = 0;
  uint32_t lowest = UINT32_MAX;
  uint32_t highest = 0;
  for (int pass = 0; pass < 100; pass++) {
    handed_out += this->uart_.paced_room_(now_us - last_write_us);
    last_write_us = this->uart_.paced_stamp_(now_us, last_write_us);
    // Whole bytes the line sent since the first write, 1.92 per ms.
    uint32_t sent = (now_us - start_us) * 192 / 100000;
    lowest = std::min(lowest, handed_out - sent);
    highest = std::max(highest, handed_out - sent);
    now_us += PASS_US[pass % 3];
  }
  EXPECT_EQ(lowest, 30u);
  EXPECT_EQ(highest, 31u);
}

TEST_F(PacedWriteRoom, NoBaudRateStampsWhenTheWriteReturned) {
  this->uart_.set_baud_rate(0);
  EXPECT_EQ(this->uart_.paced_stamp_(5000, 0), 5000u);
}

TEST_F(PacedWriteRoom, DoneStoresWhenAPacedWriteReturned) {
  uint32_t last_write_us = 0;
  this->uart_.paced_write_room(last_write_us);
  this->uart_.paced_write_done(last_write_us);
  const uint32_t after = micros();
  // Less than a byte time before now; a second of slack for a slow runner
  EXPECT_LT(after - last_write_us, 1000000u);
}

TEST_F(PacedWriteRoom, DoneKeepsTheTimeWhereTheUartReportsItsRoom) {
  this->uart_.room = 7;
  uint32_t last_write_us = 1234;
  this->uart_.paced_write_room(last_write_us);
  this->uart_.paced_write_done(last_write_us);
  EXPECT_EQ(last_write_us, 1234u);
}

}  // namespace esphome::uart::testing
