#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>

#include "fake_canbus.h"

// The SDO client against canned replies of a server (node 0x21: requests on 0x621, replies on 0x5A1).
namespace esphome::canopen::testing {

using Bytes = std::vector<uint8_t>;

class SdoClient : public ::testing::Test {
 protected:
  FakeCanbus bus_;
  TestHub hub_;
  std::vector<SDOReply> replies_;
  std::string data_;  // READ_DATA: the buffer, copied during the callback

  void SetUp() override {
    this->hub_.set_canbus(&this->bus_);
    this->hub_.set_response_timeout(20);
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
  }

  static void on_reply(void *context, const SDOReply &reply) {
    auto *self = static_cast<SdoClient *>(context);
    self->replies_.push_back(reply);
    if (reply.data != nullptr)
      self->data_.assign(reinterpret_cast<const char *>(reply.data), reply.len);
  }

  // the requests sent to node 0x21, in order
  std::vector<Bytes> requests_(uint32_t can_id = 0x621) const {
    std::vector<Bytes> out;
    for (const auto &f : this->bus_.sent) {
      if (f.can_id == can_id)
        out.push_back(f.data);
    }
    return out;
  }
};

TEST_F(SdoClient, ExpeditedRead) {
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<Bytes>{{0x40, 0x41, 0x60, 0, 0, 0, 0, 0}}));
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_SUCCESS);
  EXPECT_EQ(this->replies_[0].value, 0x0237u);
  EXPECT_EQ(this->replies_[0].size, 2);
}

TEST_F(SdoClient, ReadAnyWidthAndSizeMismatch) {
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

TEST_F(SdoClient, ExpeditedWrite) {
  ASSERT_TRUE(this->hub_.write_sdo<int16_t>(0x21, 0x6040, 0, -2, on_reply, this));
  this->hub_.loop();
  ASSERT_EQ(this->requests_(), (std::vector<Bytes>{{0x2B, 0x40, 0x60, 0, 0xFE, 0xFF, 0, 0}}));
  this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_EQ(this->replies_[0].value, 0xFFFEu);
  EXPECT_EQ(this->replies_[0].size, 2);
}

TEST_F(SdoClient, SegmentedRead) {
  ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, on_reply, this));
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

TEST_F(SdoClient, SegmentedWrite) {
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

TEST_F(SdoClient, ServerAbort) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1234, 5, on_reply, this));
  this->hub_.loop();
  // object does not exist
  this->hub_.frame(0x5A1, {0x80, 0x34, 0x12, 5, 0x00, 0x00, 0x02, 0x06});
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(this->replies_[0].abort_code, 0x06020000u);
}

TEST_F(SdoClient, TimeoutSendsAbort) {
  ASSERT_TRUE(this->hub_.read_sdo<uint32_t>(0x21, 0x1000, 0, on_reply, this));
  this->hub_.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  this->hub_.loop();
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_TIMEOUT);
  EXPECT_EQ(this->replies_[0].abort_code, 0x05040000u);
  auto req = this->requests_();
  ASSERT_EQ(req.size(), 2u);
  EXPECT_EQ(req[1], (Bytes{0x80, 0x00, 0x10, 0, 0x00, 0x00, 0x04, 0x05}));
}

TEST_F(SdoClient, QueueRunsInOrder) {
  ASSERT_TRUE(this->hub_.read_sdo<uint8_t>(0x21, 0x1001, 0, on_reply, this));
  ASSERT_TRUE(this->hub_.write_sdo<uint8_t>(0x22, 0x6060, 0, 1, on_reply, this));
  this->hub_.loop();
  // one transfer at a time: the second waits for the first
  EXPECT_EQ(this->requests_(0x622).size(), 0u);
  this->hub_.frame(0x5A1, {0x4F, 0x01, 0x10, 0, 0, 0, 0, 0});
  this->hub_.loop();
  ASSERT_EQ(this->requests_(0x622), (std::vector<Bytes>{{0x2F, 0x60, 0x60, 0, 1, 0, 0, 0}}));
  this->hub_.frame(0x5A2, {0x60, 0x60, 0x60, 0, 0, 0, 0, 0});
  ASSERT_EQ(this->replies_.size(), 2u);
  EXPECT_TRUE(this->replies_[0].ok());
  EXPECT_TRUE(this->replies_[1].ok());
}

TEST_F(SdoClient, Rejected) {
  // invalid node ids and sizes: the callback runs before the call returns false
  EXPECT_FALSE(this->hub_.read_sdo<uint8_t>(0, 0x1000, 0, on_reply, this));
  EXPECT_FALSE(this->hub_.read_sdo<uint8_t>(128, 0x1000, 0, on_reply, this));
  EXPECT_FALSE(this->hub_.read_sdo(0x21, 0x1000, 0, 3, on_reply, this));
  EXPECT_FALSE(this->hub_.write_sdo(0x21, 0x1000, 0, 0, 3, on_reply, this));
  EXPECT_FALSE(this->hub_.write_sdo_data(0x21, 0x1000, 0, nullptr, 0, on_reply, this));
  ASSERT_EQ(this->replies_.size(), 5u);
  for (const auto &r : this->replies_)
    EXPECT_EQ(r.result, SDOResult::SDO_RESULT_REJECTED);
  EXPECT_TRUE(this->bus_.sent.empty());
}

TEST_F(SdoClient, QueueFull) {
  int accepted = 0;
  while (this->hub_.write_sdo<uint8_t>(0x21, 0x2000, 0, 0, on_reply, this))
    accepted++;
  EXPECT_EQ(accepted, 16);
  ASSERT_EQ(this->replies_.size(), 1u);
  EXPECT_EQ(this->replies_[0].result, SDOResult::SDO_RESULT_REJECTED);
}

}  // namespace esphome::canopen::testing
