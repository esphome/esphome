#include <gtest/gtest.h>
#include "esphome/components/canopen/canopen.h"

namespace esphome::canopen::testing {

class MockCANbus : public canbus::Canbus {
 public:
  std::vector<canbus::CanFrame> sent;
  canbus::Error result{canbus::ERROR_OK};

 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override {
    this->sent.push_back(*frame);
    return this->result;
  }
  canbus::Error read_message(canbus::CanFrame *) override { return canbus::ERROR_NOMSG; }
};

class ProbeClient : public CANopenComponent {
 public:
  using CANopenComponent::CANopenComponent;
  using CANopenComponent::on_frame_;
  void expire() { this->sent_at_ms_ -= this->response_timeout_ms_; }
};

struct Reply {
  unsigned count{0};
  SDOResult result{};
  uint32_t value{0};
  uint8_t size{0};
  static void receive(void *context, SDOResult result, uint32_t value, uint8_t size) {
    auto *reply = static_cast<Reply *>(context);
    reply->count++;
    reply->result = result;
    reply->value = value;
    reply->size = size;
  }
};

TEST(CANopenSDO, ReadMatchesNodeIndexAndSubindex) {
  MockCANbus bus;
  ProbeClient client(&bus);
  Reply reply;
  ASSERT_TRUE(client.read_sdo(32, 0x6041, 1, Reply::receive, &reply));
  client.loop();
  ASSERT_EQ(bus.sent.size(), 1);
  EXPECT_EQ(bus.sent[0].can_id, 0x620);
  EXPECT_EQ(bus.sent[0].data[0], 0x40);
  EXPECT_EQ(bus.sent[0].data[1], 0x41);
  EXPECT_EQ(bus.sent[0].data[2], 0x60);
  client.on_frame_(0x5A1, false, false, {0x4B, 0x41, 0x60, 1, 0x18, 0x06, 0, 0});
  client.on_frame_(0x5A0, false, false, {0x4B, 0x42, 0x60, 1, 0x18, 0x06, 0, 0});
  client.on_frame_(0x5A0, false, false, {0x4B, 0x41, 0x60, 0, 0x18, 0x06, 0, 0});
  client.on_frame_(0x5A0, true, false, {0x4B, 0x41, 0x60, 1, 0x18, 0x06, 0, 0});
  client.on_frame_(0x5A0, false, true, {0x4B, 0x41, 0x60, 1, 0x18, 0x06, 0, 0});
  client.on_frame_(0x5A0, false, false, {0x4B, 0x41});
  EXPECT_EQ(reply.count, 0);
  client.on_frame_(0x5A0, false, false, {0x4B, 0x41, 0x60, 1, 0x18, 0x06, 0xFF, 0xFF});
  EXPECT_EQ(reply.count, 1);
  EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_SUCCESS);
  EXPECT_EQ(reply.value, 0x0618);
  EXPECT_EQ(reply.size, 2);
}

TEST(CANopenSDO, WritesUseExpeditedWidthsAndAwaitAcknowledgement) {
  for (const uint8_t size : {1, 2, 4}) {
    MockCANbus bus;
    ProbeClient client(&bus);
    ASSERT_TRUE(client.write_sdo(33, 0x2000, 0, 42, size));
    ASSERT_TRUE(client.write_sdo(32, 0x2001, 0, 17, 1));
    client.loop();
    client.loop();
    ASSERT_EQ(bus.sent.size(), 1);
    EXPECT_EQ(bus.sent[0].data[0], 0x23 | ((4 - size) << 2));
    EXPECT_EQ(bus.sent[0].data[4], 42);
    client.on_frame_(0x5A1, false, false, {0x60, 0, 0x20, 0, 0, 0, 0, 0});
    client.loop();
    ASSERT_EQ(bus.sent.size(), 2);
    EXPECT_EQ(bus.sent[1].can_id, 0x620);
  }
}

TEST(CANopenSDO, TimeoutReleasesQueueWithoutRetryingWrites) {
  MockCANbus bus;
  ProbeClient client(&bus);
  Reply reply;
  client.read_sdo(32, 0x6041, 0, Reply::receive, &reply);
  client.write_sdo(33, 0x2000, 0, 3, 1);
  client.loop();
  client.expire();
  client.loop();
  EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_TIMEOUT);
  EXPECT_EQ(reply.count, 1);
  client.loop();
  ASSERT_EQ(bus.sent.size(), 2);
  EXPECT_EQ(bus.sent[1].can_id, 0x621);
}

TEST(CANopenSDO, AbortAndUnsupportedTransferAreReported) {
  MockCANbus bus;
  ProbeClient client(&bus);
  Reply reply;
  client.read_sdo(32, 0x6041, 0, Reply::receive, &reply);
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x80, 0x41, 0x60, 0, 0, 0, 2, 6});
  EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(reply.value, 0x06020000);
  client.read_sdo(32, 0x6041, 0, Reply::receive, &reply);
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x41, 0x41, 0x60, 0, 8, 0, 0, 0});
  EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_UNSUPPORTED);
}

TEST(CANopenSDO, QueueIsBoundedAndInvalidWritesAreRejected) {
  MockCANbus bus;
  ProbeClient client(&bus);
  EXPECT_FALSE(client.write_sdo(0, 0x2000, 0, 0, 1));
  EXPECT_FALSE(client.write_sdo(32, 0x2000, 0, 256, 1));
  EXPECT_FALSE(client.write_sdo(32, 0x2000, 0, 0, 3));
  for (unsigned i = 0; i < 16; i++)
    ASSERT_TRUE(client.write_sdo(32, 0x2000, 0, i, 1));
  EXPECT_FALSE(client.write_sdo(32, 0x2000, 0, 0, 1));
}

TEST(CANopenSDO, SendFailureCallsBackAndAllowsFollowingRequest) {
  MockCANbus bus;
  ProbeClient client(&bus);
  Reply reply;
  bus.result = canbus::ERROR_FAILTX;
  client.read_sdo(32, 0x6041, 0, Reply::receive, &reply);
  client.loop();
  EXPECT_EQ(reply.result, SDOResult::SDO_RESULT_TRANSPORT_ERROR);
  EXPECT_EQ(reply.count, 1);
  bus.result = canbus::ERROR_OK;
  EXPECT_TRUE(client.write_sdo(33, 0x2000, 0, 1, 1));
  client.loop();
  EXPECT_EQ(bus.sent.size(), 2);
}

TEST(CANopenSDO, UploadWidthsAndLittleEndianDownloads) {
  MockCANbus bus;
  ProbeClient client(&bus);
  Reply reply;
  for (const uint8_t size : {1, 2, 4}) {
    client.read_sdo(32, 0x2000, 0, Reply::receive, &reply);
    client.loop();
    const uint8_t command = 0x43 | ((4 - size) << 2);
    client.on_frame_(0x5A0, false, false, {command, 0, 0x20, 0, 0x73, 0x61, 0x76, 0x65});
    EXPECT_EQ(reply.size, size);
    EXPECT_EQ(reply.value, size == 1 ? 0x73 : size == 2 ? 0x6173 : 0x65766173);
  }
  client.write_sdo(32, 0x1010, 1, 0x65766173, 4);
  client.loop();
  const auto &frame = bus.sent.back();
  EXPECT_EQ(frame.data[4], 0x73);
  EXPECT_EQ(frame.data[5], 0x61);
  EXPECT_EQ(frame.data[6], 0x76);
  EXPECT_EQ(frame.data[7], 0x65);
}

TEST(CANopenSDO, InstancesHaveIndependentQueues) {
  MockCANbus bus_a;
  MockCANbus bus_b;
  ProbeClient client_a(&bus_a);
  ProbeClient client_b(&bus_b);
  Reply reply_a;
  Reply reply_b;
  client_a.read_sdo(32, 0x2000, 0, Reply::receive, &reply_a);
  client_b.read_sdo(33, 0x2000, 0, Reply::receive, &reply_b);
  client_a.loop();
  client_b.loop();
  client_a.on_frame_(0x5A0, false, false, {0x4F, 0, 0x20, 0, 1, 0, 0, 0});
  EXPECT_EQ(reply_a.count, 1);
  EXPECT_EQ(reply_b.count, 0);
  client_b.on_frame_(0x5A1, false, false, {0x4F, 0, 0x20, 0, 2, 0, 0, 0});
  EXPECT_EQ(reply_b.value, 2);
}

}  // namespace esphome::canopen::testing
