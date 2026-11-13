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

TEST_F(PacedWriteRoom, OneMillisecondPassesHandOutWhatTheLineSent) {
  // At 19200 baud the line sends 1.92 bytes per ms, 19.2 in 10 ms. Ten 1-ms passes hand out 19, because each keeps
  // the part of a byte for the next. Whole milliseconds rounded down per pass let out 1 byte each, 10 in all.
  this->uart_.set_baud_rate(19200);
  EXPECT_EQ(this->passes_(1000, 10000), 19u);
}

TEST_F(PacedWriteRoom, LoopIntervalPassesKeepThePartOfAByte) {
  // At 9600 baud the line sends 15.36 bytes per 16-ms loop interval, 153.6 in ten. Ten passes one loop interval apart
  // hand out 153: the cap does not cut the part of a byte. Rounded down per pass they would get 15 each, 150 in all.
  EXPECT_EQ(this->passes_(16000, 160000), 153u);
}

TEST_F(PacedWriteRoom, EarlyPassTakesTheWholeBytesSinceTheLastWrite) {
  // 9600 baud is 960 bytes/s: 4.8 bytes in 5 ms, rounded down.
  EXPECT_EQ(this->uart_.paced_room_(5000), 4u);
}

TEST_F(PacedWriteRoom, LongGapTakesOneLoopIntervalAndOneByte) {
  // 1 s counts as one 16-ms loop interval and one byte: 15.36 + 1 bytes at 9600 baud, 1474.56 + 1 at 921600.
  EXPECT_EQ(this->uart_.paced_room_(1000000), 16u);
  this->uart_.set_baud_rate(921600);
  EXPECT_EQ(this->uart_.paced_room_(1000000), 1475u);
}

TEST_F(PacedWriteRoom, AtLeastOneByte) { EXPECT_EQ(this->uart_.paced_room_(0), 1u); }

TEST_F(PacedWriteRoom, StampKeepsThePartOfAByte) {
  // A byte takes 1041.7 us at 9600 baud. 5 ms after the last stamp the line sent 4.8 bytes, so the stamp moves on by
  // 4 bytes, to 4167 us, and the 0.8 byte left counts for the next write.
  EXPECT_EQ(this->uart_.paced_stamp_(5000, 0), 4167u);
}

TEST_F(PacedWriteRoom, StreamNeitherFillsNorDrainsTheBuffer) {
  // usb_uart and weikai never wait, so what does not fit their buffer is lost. 100 passes of 1.0, 1.3 and 1.7 ms at
  // 19200 baud: the first write gets one loop interval and one byte, 31 bytes, and each later one the whole bytes the
  // line sent since. The buffer then holds 30 or 31 bytes: it neither fills up nor runs dry.
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

TEST_F(PacedWriteRoom, NoBaudRateWritesOneByte) {
  // ble_nus and usb_cdc_acm set no baud rate: one byte per pass, and the stamp is the time the write returned.
  this->uart_.set_baud_rate(0);
  EXPECT_EQ(this->uart_.paced_room_(1000000), 1u);
  EXPECT_EQ(this->uart_.paced_stamp_(5000, 0), 5000u);
}

TEST_F(PacedWriteRoom, DoneStoresWhenAPacedWriteReturned) {
  uint32_t last_write_us = 0;
  this->uart_.paced_write_room(last_write_us);
  this->uart_.paced_write_done(last_write_us);
  const uint32_t after = micros();
  // Stored less than one byte time (1.04 ms) before now. A second leaves room for a slow runner, and the unsigned
  // difference also holds across a counter wrap.
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
