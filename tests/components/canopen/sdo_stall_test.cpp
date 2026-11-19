#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include "fake_canbus.h"

// The SDO client when the bus refuses frames, and requests it must not send as they are.
namespace esphome::canopen::testing {

class SdoStall : public SdoTest {};

TEST_F(SdoStall, UnsentWriteEndsAndIsNeverSent) {
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
TEST_F(SdoStall, FrameUnsentAtTheDeadlineIsNotSentLate) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x1234, on_reply, this));
  this->hub_.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
  EXPECT_EQ(this->bus_.count(0x621), 0u);
}

// The bus takes the frame before the deadline: sent, and the response timeout starts again from there.
TEST_F(SdoStall, RetryBeforeTheDeadlineRestartsTheTimeout) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->hub_.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<std::vector<uint8_t>>{{0x40, 0x41, 0x60, 0, 0, 0, 0, 0}}));
  // 25 ms since the request, 10 ms since it was sent (timeout 20 ms)
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  this->hub_.loop();
  EXPECT_TRUE(this->replies_.empty());
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 0x0237u);
}

// A reply can't be for a request that hasn't gone out: e.g. a late one to an earlier write of the same object.
TEST_F(SdoStall, ReplyWhileTheRequestIsUnsentIsIgnored) {
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

TEST_F(SdoStall, UnsentReadEnds) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
}

TEST_F(SdoStall, QueueBehindAStalledRequestMovesOn) {
  this->bus_.busy = true;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6042, 0, on_reply, this));
  this->wait_for_timeout_();
  this->wait_for_timeout_();
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_EQ(this->replies_[1].result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
}

// The request went out, then the bus stalled: the abort after the timeout can't be sent.
TEST_F(SdoStall, UnsentAbortIsDroppedAndNextFrameGoesOutDirectly) {
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

TEST_F(SdoStall, ValueThatDoesNotFitIsRejected) {
  EXPECT_FALSE(this->hub_.write_sdo(0x21, 0x6040, 0, 300u, 1, on_reply, this));
  EXPECT_FALSE(this->hub_.write_sdo(0x21, 0x6040, 0, static_cast<uint32_t>(-129), 1, on_reply, this));
  EXPECT_FALSE(this->hub_.write_sdo(0x21, 0x6040, 0, 0x10000u, 2, on_reply, this));
  ASSERT_EQ(this->replies_.size(), 3u);
  for (const auto &reply : this->replies_)
    EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_REJECTED);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 0u);
}

TEST_F(SdoStall, ValueAtTheLimitsIsSent) {
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 255u, 1));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, static_cast<uint32_t>(-128), 1));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 0xFFFFu, 2));
  EXPECT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, 0xFFFFFFFFu, 4));
  EXPECT_TRUE(this->hub_.write_sdo<int16_t>(0x21, 0x6040, 0, -2));
}

#ifdef USE_CANBUS_FD
// CANopen is classic CAN: an FD frame on the reply id isn't a reply, even with 8 bytes
TEST_F(SdoStall, FdFrameOnTheReplyIdIsIgnored) {
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

TEST_F(SdoStall, ReadFillsTheCallersBuffer) {
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

TEST_F(SdoStall, ReadOverTheCapacityIsAbortedAndStaysInTheBuffer) {
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
TEST_F(SdoStall, ReadOverTheCapacityInTheLastSegment) {
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

TEST_F(SdoStall, ExpeditedReadOverTheCapacity) {
  std::array<uint8_t, 2> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1000, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x43, 0x00, 0x10, 0, 1, 2, 3, 4});
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, static_cast<uint32_t>(CO_SDO_AB_OUT_OF_MEM));
}

TEST_F(SdoStall, ReadWithoutABufferIsRejected) {
  std::array<uint8_t, 4> buffer{};
  EXPECT_FALSE(this->hub_.read_sdo_data(0x21, 0x1008, 0, nullptr, 4, on_reply, this));
  EXPECT_FALSE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), 0, on_reply, this));
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_REJECTED);
  EXPECT_EQ(this->replies_[1].result, SDOResult::SDO_RESULT_REJECTED);
}

}  // namespace esphome::canopen::testing
