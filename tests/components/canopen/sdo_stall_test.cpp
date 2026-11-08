#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include "fake_canbus.h"

// The SDO client when the bus refuses frames, and requests it must not send as they are.
// Requests to node 0x21 on 0x621, replies on 0x5A1.
namespace esphome::canopen::testing {

class SdoStall : public ::testing::Test {
 protected:
  FakeCanbus bus_;
  TestHub hub_;
  std::vector<SDOReply> replies_;

  void SetUp() override {
    this->hub_.set_canbus(&this->bus_);
    this->hub_.set_response_timeout(20);
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
  }

  static void on_reply(void *context, const SDOReply &reply) {
    static_cast<SdoStall *>(context)->replies_.push_back(reply);
  }

  // long enough for the response timeout to end a transfer
  void wait_for_timeout_() {
    for (int i = 0; i < 60; i++) {
      this->hub_.loop();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
};

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
