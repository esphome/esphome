#include <gtest/gtest.h>
#include <array>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

using Bytes = std::vector<uint8_t>;

TEST_F(SdoTest, ExpeditedRead) {
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<Bytes>{{0x40, 0x41, 0x60, 0, 0, 0, 0, 0}}));
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_SUCCESS);
  EXPECT_EQ(this->replies_[0].value, 0x0237u);
  EXPECT_EQ(this->replies_[0].size, 2);
}

TEST_F(SdoTest, ReadAnyWidthAndSizeMismatch) {
  // expected_size 0 accepts the 1-byte value; 4 rejects the 2-byte one
  ASSERT_TRUE(this->hub_.read_sdo(0x21, 0x1001, 0, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.read_sdo(0x21, 0x6041, 0, 4, on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x4F, 0x01, 0x10, 0, 0xFE, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_SUCCESS);
  EXPECT_EQ(this->replies_[0].as<int8_t>(), -2);
  EXPECT_EQ(this->replies_[1].result, SDOResult::SDO_RESULT_SIZE_MISMATCH);
}

TEST_F(SdoTest, ExpeditedWrite) {
  ASSERT_TRUE(this->hub_.write_sdo<int16_t>(0x21, 0x6040, 0, -2, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<Bytes>{{0x2B, 0x40, 0x60, 0, 0xFE, 0xFF, 0, 0}}));
  this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 0xFFFEu);
  EXPECT_EQ(this->replies_[0].size, 2);
}

TEST_F(SdoTest, SegmentedRead) {
  std::array<uint8_t, 32> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  // initiate response: segmented, 10 bytes
  this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 10, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x00, 'c', 'a', 'n', 'o', 'p', 'e', 'n'});
  this->hub_.loop();
  // last segment, toggle bit set, 3 bytes (4 unused)
  this->hub_.frame(0x5A1, {0x10 | (4 << 1) | 1, ' ', 'i', 'o', 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->data_, "canopen io");
  auto req = this->requests_();
  ASSERT_EQ(req.size(), 3u);
  EXPECT_EQ(req[1][0], 0x60);  // upload segment, toggle 0
  EXPECT_EQ(req[2][0], 0x70);  // toggle 1
}

TEST_F(SdoTest, SegmentedWrite) {
  static const uint8_t DATA[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_TRUE(this->hub_.write_sdo_data(0x21, 0x2000, 1, DATA, sizeof(DATA), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x60, 0x00, 0x20, 1, 0, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x20, 0, 0, 0, 0, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x30, 0, 0, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->requests_(), (std::vector<Bytes>{
                                   {0x21, 0x00, 0x20, 1, 9, 0, 0, 0},           // initiate, size indicated
                                   {0x00, 1, 2, 3, 4, 5, 6, 7},                 // toggle 0
                                   {0x10 | (5 << 1) | 1, 8, 9, 0, 0, 0, 0, 0},  // toggle 1, 2 bytes, last
                               }));
}

TEST_F(SdoTest, ServerAbort) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1234, 5, on_reply, this));
  this->hub_.loop();
  // object does not exist
  this->hub_.frame(0x5A1, {0x80, 0x34, 0x12, 5, 0x00, 0x00, 0x02, 0x06});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, 0x06020000u);
}

TEST_F(SdoTest, TimeoutSendsAbort) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1000, 0, on_reply, this));
  this->hub_.loop();
  advance_(40);
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  EXPECT_EQ(this->replies_[0].abort_code, 0x05040000u);
  auto req = this->requests_();
  ASSERT_EQ(req.size(), 2u);
  EXPECT_EQ(req[1], (Bytes{0x80, 0x00, 0x10, 0, 0x00, 0x00, 0x04, 0x05}));
}

TEST_F(SdoTest, QueueRunsInOrder) {
  ASSERT_TRUE(this->hub_.read_sdo<uint8_t>(0x21, 0x1001, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x22, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  EXPECT_EQ(this->requests_(0x622).size(), 0u);
  this->hub_.frame(0x5A1, {0x4F, 0x01, 0x10, 0, 0, 0, 0, 0});
  this->hub_.loop();
  ASSERT_EQ(this->requests_(0x622), (std::vector<Bytes>{{0x2F, 0x60, 0x60, 0, 1, 0, 0, 0}}));
  this->hub_.frame(0x5A2, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_TRUE(this->replies_[1].ok());
}

TEST_F(SdoTest, InvalidRequestsAreRejected) {
  static uint8_t buffer[4];
  static constexpr struct {
    const char *name;
    bool (*call)(TestHub &hub, void *context);
  } CASES[] = {
      {"node id 0", [](TestHub &hub, void *ctx) { return hub.read_sdo<uint8_t>(0, 0x1000, 0, on_reply, ctx); }},
      {"node id 128", [](TestHub &hub, void *ctx) { return hub.read_sdo<uint8_t>(128, 0x1000, 0, on_reply, ctx); }},
      {"read size 3", [](TestHub &hub, void *ctx) { return hub.read_sdo(0x21, 0x1000, 0, 3, on_reply, ctx); }},
      {"write size 3", [](TestHub &hub, void *ctx) { return hub.write_sdo(0x21, 0x1000, 0, 0, 3, on_reply, ctx); }},
      {"300 in 1 byte", [](TestHub &hub, void *ctx) { return hub.write_sdo(0x21, 0x6040, 0, 300u, 1, on_reply, ctx); }},
      {"-129 in 1 byte",
       [](TestHub &hub, void *ctx) {
         return hub.write_sdo(0x21, 0x6040, 0, static_cast<uint32_t>(-129), 1, on_reply, ctx);
       }},
      {"0x10000 in 2 bytes",
       [](TestHub &hub, void *ctx) { return hub.write_sdo(0x21, 0x6040, 0, 0x10000u, 2, on_reply, ctx); }},
      {"write without data",
       [](TestHub &hub, void *ctx) { return hub.write_sdo_data(0x21, 0x1000, 0, nullptr, 0, on_reply, ctx); }},
      {"write over 64 KiB",
       [](TestHub &hub, void *ctx) { return hub.write_sdo_data(0x21, 0x1000, 0, buffer, 65536, on_reply, ctx); }},
      {"read without a buffer",
       [](TestHub &hub, void *ctx) { return hub.read_sdo_data(0x21, 0x1008, 0, nullptr, 4, on_reply, ctx); }},
      {"read with capacity 0",
       [](TestHub &hub, void *ctx) { return hub.read_sdo_data(0x21, 0x1008, 0, buffer, 0, on_reply, ctx); }},
  };
  for (const auto &c : CASES) {
    SCOPED_TRACE(c.name);
    this->replies_.clear();
    EXPECT_FALSE(c.call(this->hub_, this));
    ASSERT_EQ(this->replies_.size(), 1u);
    EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_REJECTED);
  }
  this->hub_.loop();
  EXPECT_TRUE(this->bus_.sent.empty());
}

TEST_F(SdoTest, QueueFull) {
  int accepted = 0;
  while (this->hub_.write_sdo<uint8_t>(0x21, 0x2000, 0, 0, on_reply, this))
    accepted++;
  EXPECT_EQ(accepted, 16);
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_REJECTED);
}

TEST_F(SdoTest, SignedRawWriteRepliesZeroExtended) {
  ASSERT_TRUE(this->hub_.write_sdo(0x21, 0x6040, 0, static_cast<uint32_t>(-1), 1, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<Bytes>{{0x2F, 0x40, 0x60, 0, 0xFF, 0, 0, 0}}));
  this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].value, 0xFFu);
  EXPECT_EQ(this->replies_[0].as<int32_t>(), -1);
}

TEST_F(SdoTest, LongObjectIsASizeMismatchForAValueRead) {
  ASSERT_TRUE(this->hub_.read_sdo(0x21, 0x1008, 0, 0, on_reply, this));
  this->hub_.loop();
  // initiate response: segmented, 200 bytes
  this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 200, 0, 0, 0});
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x00, 'd', 'i', 'm', 'm', 'e', 'r', '4'});
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_SIZE_MISMATCH);
  // aborted without requesting the next segment
  EXPECT_EQ(this->requests_(), (std::vector<Bytes>{
                                   {0x40, 0x08, 0x10, 0, 0, 0, 0, 0},
                                   {0x60, 0, 0, 0, 0, 0, 0, 0},
                                   {0x80, 0x08, 0x10, 0, 0x12, 0x00, 0x07, 0x06},
                               }));
}

TEST_F(SdoTest, SegmentedWriteLargerThanTheFifo) {
  static const uint8_t DATA[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  ASSERT_TRUE(this->hub_.write_sdo_data(0x21, 0x2000, 1, DATA, sizeof(DATA), on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x60, 0x00, 0x20, 1, 0, 0, 0, 0});
  this->hub_.frame(0x5A1, {0x20, 0, 0, 0, 0, 0, 0, 0});
  this->hub_.frame(0x5A1, {0x30, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_TRUE(this->replies_.empty());
  this->hub_.frame(0x5A1, {0x20, 0, 0, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->requests_(), (std::vector<Bytes>{
                                   {0x21, 0x00, 0x20, 1, 20, 0, 0, 0},
                                   {0x00, 1, 2, 3, 4, 5, 6, 7},
                                   {0x10, 8, 9, 10, 11, 12, 13, 14},
                                   {0x00 | (1 << 1) | 1, 15, 16, 17, 18, 19, 20, 0},  // toggle 0, 6 bytes, last
                               }));
}

TEST_F(SdoTest, ServerAbortWithTheTimeoutCode) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1000, 0, on_reply, this));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x80, 0x00, 0x10, 0, 0x00, 0x00, 0x04, 0x05});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, 0x05040000u);
}

// A frame handled after the deadline, before the queued reply, must not time the request out.
TEST_F(SdoTest, OtherFrameAfterTheTimeoutBeforeTheReply) {
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x1017, 0, on_reply, this));
  this->hub_.loop();
  advance_(40);
  this->hub_.frame(0x701, {0x05});  // a heartbeat
  EXPECT_TRUE(this->replies_.empty());
  this->hub_.frame(0x5A1, {0x4B, 0x17, 0x10, 0, 0xE8, 0x03, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 1000u);
}

TEST_F(SdoTest, DelayedSegmentDoesNotShortenTheNextOne) {
  std::array<uint8_t, 16> buffer{};
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), buffer.size(), on_reply, this));
  this->hub_.loop();
  advance_(15);
  this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 10, 0, 0, 0});
  // 25 ms since the last loop, 10 ms since the segment request (timeout 20 ms)
  advance_(10);
  this->hub_.loop();
  EXPECT_TRUE(this->replies_.empty());
  advance_(5);
  this->hub_.frame(0x5A1, {0x00, 'c', 'a', 'n', 'o', 'p', 'e', 'n'});
  advance_(15);
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x10 | (4 << 1) | 1, ' ', 'i', 'o', 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->data_, "canopen io");
}

TEST_F(SdoTest, LateRepliesForAnotherObjectAreIgnored) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1000, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x21, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  advance_(40);
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  this->hub_.loop();
  ASSERT_EQ(this->requests_().back(), (Bytes{0x2F, 0x60, 0x60, 0, 1, 0, 0, 0}));

  this->hub_.frame(0x5A1, {0x43, 0x00, 0x10, 0, 0x92, 0x01, 0, 0});        // upload response, 0x1000
  this->hub_.frame(0x5A1, {0x80, 0x00, 0x10, 0, 0x00, 0x00, 0x02, 0x06});  // abort, 0x1000
  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 1, 0, 0, 0, 0});              // download response, 0x6060:1
  this->hub_.loop();
  EXPECT_EQ(this->replies_.size(), 1u);
  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_TRUE(this->replies_[1].ok());
}

TEST_F(SdoTest, LateWriteAckForTheSameObjectIsIgnoredByARead) {
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x21, 0x6060, 0, 1, on_reply, this));
  ASSERT_TRUE(this->hub_.read_sdo<uint8_t>(0x21, 0x6060, 0, on_reply, this));
  this->hub_.loop();
  advance_(40);
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  this->hub_.loop();
  ASSERT_EQ(this->requests_().back(), (Bytes{0x40, 0x60, 0x60, 0, 0, 0, 0, 0}));

  this->hub_.frame(0x5A1, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});  // the write's late download response
  this->hub_.loop();
  EXPECT_EQ(this->replies_.size(), 1u);
  this->hub_.frame(0x5A1, {0x4F, 0x60, 0x60, 0, 0x07, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_TRUE(this->replies_[1].ok());
  EXPECT_EQ(this->replies_[1].value, 7u);
}

// e.g. from on_boot, which runs before the hub's setup()
TEST(CanopenSdoClient, RequestBeforeSetupWaitsForIt) {
  FakeCanbus bus;
  TestHub hub(&bus, CANopenComponent::NO_NODE_ID);
  std::vector<SDOReply> replies;
  auto on_reply = [](void *context, const SDOReply &reply) {
    static_cast<std::vector<SDOReply> *>(context)->push_back(reply);
  };
  ASSERT_TRUE(hub.read_sdo<uint8_t>(0x21, 0x1001, 0, on_reply, &replies));
  EXPECT_TRUE(replies.empty());
  hub.setup();
  hub.loop();
  EXPECT_EQ(bus.count(0x621), 1u);
  hub.frame(0x5A1, {0x4F, 0x01, 0x10, 0, 0, 0, 0, 0});
  ASSERT_EQ(replies.size(), 1u);
  EXPECT_TRUE(replies[0].ok());
}

// its loop never runs, so requests would only time out
TEST(CanopenSdoClient, FailedBusFailsTheHub) {
  FakeCanbus bus;
  TestHub hub(&bus, CANopenComponent::NO_NODE_ID);
  std::vector<SDOReply> replies;
  auto on_reply = [](void *context, const SDOReply &reply) {
    static_cast<std::vector<SDOReply> *>(context)->push_back(reply);
  };
  ASSERT_TRUE(hub.read_sdo<uint8_t>(0x21, 0x1001, 0, on_reply, &replies));
  bus.mark_failed();
  hub.setup();
  EXPECT_TRUE(hub.is_failed());
  ASSERT_EQ(replies.size(), 1u);  // the request made before setup()
  EXPECT_EQ(replies[0].result, SDOResult::SDO_RESULT_REJECTED);
  EXPECT_FALSE(hub.read_sdo<uint8_t>(0x21, 0x1001, 0, on_reply, &replies));
  EXPECT_TRUE(bus.sent.empty());
}

}  // namespace esphome::canopen::testing
