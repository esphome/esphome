#include <gtest/gtest.h>
#include <array>
#include <vector>

#include "fake_canbus.h"

// The SDO client when the bus refuses frames, and requests it must not send as they are.
namespace esphome::canopen::testing {

TEST_F(SdoTest, UnsentWriteEndsAndIsNeverSent) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x1234, on_reply, this));
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);

  this->bus_.busy = false;
  this->wait_for_timeout_();
  EXPECT_EQ(this->bus_.count(0x621), 0u);
  EXPECT_EQ(this->replies_.size(), 1u);
}

// The bus takes frames again right at the deadline: the request ends unsent instead of reaching the server just
// before the caller is told it failed.
TEST_F(SdoTest, FrameUnsentAtTheDeadlineIsNotSentLate) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x1234, on_reply, this));
  this->hub_.loop();
  advance_(25);
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
  EXPECT_EQ(this->bus_.count(0x621), 0u);
}

// No reply can wait behind a capped receive pass for a frame that never went out: its deadline still applies.
TEST_F(SdoTest, UnsentFrameEndsAtTheDeadlineDuringACappedPass) {
  this->bus_.set_max_frames_per_loop(1);
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x1234, on_reply, this));
  this->hub_.loop();
  this->bus_.queue(0x181, {0});
  this->bus_.queue(0x181, {1});
  advance_(25);
  this->bus_.loop();
  ASSERT_TRUE(this->bus_.rx_capped());
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
  EXPECT_EQ(this->bus_.count(0x621), 0u);
}

// The bus takes the frame before the deadline: sent, and the response timeout starts again from there.
TEST_F(SdoTest, RetryBeforeTheDeadlineRestartsTheTimeout) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->hub_.loop();
  advance_(15);
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<std::vector<uint8_t>>{{0x40, 0x41, 0x60, 0, 0, 0, 0, 0}}));
  // 25 ms since the request, 10 ms since it was sent (timeout 20 ms)
  advance_(10);
  this->hub_.loop();
  EXPECT_TRUE(this->replies_.empty());
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 0x0237u);
}

// Queued frames go out from loop(), not for every received frame: on ESP32 each refused send can block.
TEST_F(SdoTest, ReceivedFrameDoesNotSendQueuedFrames) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x21, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.frame(0x701, {0x05});
  EXPECT_EQ(this->bus_.count(0x621), 0u);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
}

// A reply can't be for a request that hasn't gone out: e.g. a late one to an earlier write of the same object.
TEST_F(SdoTest, ReplyWhileTheRequestIsUnsentIsIgnored) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x21, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  EXPECT_TRUE(this->replies_.empty());
  this->bus_.busy = false;
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
}

// A late segment response of a timed-out transfer would make CANopenNode abort the next request to that node.
TEST_F(SdoTest, SegmentResponseWhileWaitingForTheInitiateOneIsIgnored) {
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x21, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x20, 0, 0, 0, 0, 0, 0, 0});  // download segment response
  this->hub_.frame(0x5A1, {0x00, 0, 0, 0, 0, 0, 0, 0});  // upload segment response
  this->hub_.loop();
  EXPECT_TRUE(this->replies_.empty());
  EXPECT_EQ(this->bus_.count(0x621), 1u);  // no abort
  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
}

TEST_F(SdoTest, UnsentReadEnds) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
}

TEST_F(SdoTest, QueueBehindAStalledRequestMovesOn) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6042, 0, on_reply, this));
  this->wait_for_timeout_();
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_EQ(this->replies_[1].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
}

// The request went out, then the bus stalled: the abort after the timeout can't be sent.
TEST_F(SdoTest, UnsentAbortIsDroppedAndNextFrameGoesOutDirectly) {
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);

  this->bus_.busy = true;
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  this->bus_.busy = false;
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);

  // a frame left counted in the driver would delay this one to the next pass
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6042, 0, on_reply, this));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

TEST_F(SdoTest, ValueAtTheLimitsIsSent) {
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 255u, 1));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, static_cast<uint32_t>(-128), 1));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 0xFFFFu, 2));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 0xFFFFFFFFu, 4));
  EXPECT_TRUE(this->hub_.write_sdo<int16_t>(0x21, 0x6040, 0, -2));
}

#ifdef USE_CANBUS_FD
// CANopen is classic CAN: an FD frame on the reply id isn't a reply, even with 8 bytes
TEST_F(SdoTest, FdFrameOnTheReplyIdIsIgnored) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1000, 0, on_reply, this));
  this->hub_.loop();
  canbus::CanFrame frame{};
  frame.can_id = 0x5A1;
  frame.fd = true;
  frame.can_data_length_code = 8;
  const uint8_t data[8] = {0x43, 0x00, 0x10, 0, 0x92, 0x01, 0, 0};
  std::copy(data, data + 8, frame.data);
  this->hub_.frame(frame);
  EXPECT_TRUE(this->replies_.empty());
}
#endif

TEST_F(SdoTest, ReadFillsTheCallersBuffer) {
  std::array<uint8_t, 4> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x43, 0x08, 0x10, 0, 'a', 'b', 'c', 'd'});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].data, buffer.data());
  EXPECT_EQ(this->replies_[0].len, 4u);
  EXPECT_EQ(buffer, (std::array<uint8_t, 4>{'a', 'b', 'c', 'd'}));
}

TEST_F(SdoTest, ReadOverTheCapacityIsAbortedAndStaysInTheBuffer) {
  // the guard bytes must survive: the object doesn't fit in the first 8
  std::array<uint8_t, 12> buffer{};
  buffer.fill(0xEE);
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), 8, on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 0, 0, 0, 0});
  for (uint8_t toggle = 0; this->replies_.empty() && this->bus_.count(0x621) < 100; toggle ^= 0x10) {
    this->hub_.loop();
    this->hub_.frame(0x5A1, {toggle, 'a', 'b', 'c', 'd', 'e', 'f', 'g'});
  }
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, static_cast<uint32_t>(CO_SDO_AB_OUT_OF_MEM));
  for (size_t i = 8; i < buffer.size(); i++)
    EXPECT_EQ(buffer[i], 0xEE);
}

// The whole object arrives at once: the transfer is over, so no abort frame goes out, but the caller gets the reason.
TEST_F(SdoTest, ReadOverTheCapacityInTheLastSegment) {
  std::array<uint8_t, 4> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 7, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x01, 'd', 'i', 'm', 'm', 'e', 'r', '4'});
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, static_cast<uint32_t>(CO_SDO_AB_OUT_OF_MEM));
  for (const auto &frame : this->bus_.sent)
    EXPECT_NE(frame.data[0], 0x80);
}

TEST_F(SdoTest, ExpeditedReadOverTheCapacity) {
  std::array<uint8_t, 2> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1000, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x43, 0x00, 0x10, 0, 1, 2, 3, 4});
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, static_cast<uint32_t>(CO_SDO_AB_OUT_OF_MEM));
}

// A reply still queued behind a burst the canbus loop stopped at its cap must not time the request out.
TEST_F(SdoTest, ReplyBehindACappedBurstDoesNotTimeOut) {
  this->bus_.set_max_frames_per_loop(4);
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x1017, 0, on_reply, this));
  this->hub_.loop();
  for (uint8_t i = 0; i < 6; i++)
    this->bus_.queue(0x181, {i});  // PDOs
  this->bus_.queue(0x5A1, {0x4B, 0x17, 0x10, 0, 0xE8, 0x03, 0, 0});
  advance_(25);  // past the 20 ms response timeout
  this->bus_.loop();
  ASSERT_TRUE(this->bus_.rx_capped());
  this->hub_.loop();
  EXPECT_TRUE(this->replies_.empty());
  this->bus_.loop();
  EXPECT_FALSE(this->bus_.rx_capped());
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 1000u);
}

// On a bus that never drains, the time is held at most one response timeout past the deadline.
TEST_F(SdoTest, SaturatedBusStillTimesOut) {
  this->bus_.set_max_frames_per_loop(1);
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x1017, 0, on_reply, this));
  this->hub_.loop();
  advance_(15);
  this->hub_.loop();  // nothing received: 15 ms charged
  uint32_t waited = 15;
  while (this->replies_.empty() && waited < 100) {
    this->bus_.queue(0x181, {0});
    this->bus_.queue(0x181, {1});
    advance_(1);
    waited++;
    this->bus_.loop();
    this->hub_.loop();
  }
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  EXPECT_EQ(waited, 40u);
}

}  // namespace esphome::canopen::testing
