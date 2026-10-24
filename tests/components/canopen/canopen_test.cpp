#include <gtest/gtest.h>
#include "test_helpers.h"
#include "esphome/components/canopen/automation.h"
#include "esphome/components/canopen/canopen_node.h"

namespace esphome::canopen::testing {

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

TEST(CANopenSDO, IdleLoopSleepsAndNewWorkWakesIt) {
  MockCANbus bus;
  ProbeClient client(&bus);
  client.loop();
  EXPECT_FALSE(client.is_in_loop_state());
  ASSERT_TRUE(client.write_sdo(32, 0x2000, 0, 1, 1));
  EXPECT_TRUE(client.is_in_loop_state());
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x60, 0, 0x20, 0, 0, 0, 0, 0});
  EXPECT_FALSE(client.is_in_loop_state());
  EXPECT_FALSE(client.status_has_warning());
}

TEST(CANopenSDO, WriteActionReportsAcknowledgementAbortTimeoutAndTransportError) {
  MockCANbus bus;
  ProbeClient client(&bus);
  SDOWriteAction<> action(&client, 32, 0x2000, 0, 1);
  action.set_value([]() -> uint32_t { return 42; });
  unsigned successes = 0;
  std::vector<SDOResult> errors;
  uint32_t abort_code = 0;
  action.add_on_success_callback([&]() { successes++; });
  action.add_on_error_callback([&](SDOResult result, uint32_t code) {
    errors.push_back(result);
    abort_code = code;
  });
  action.play();
  action.play();
  EXPECT_EQ(successes, 0);
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x60, 0, 0x20, 0, 0, 0, 0, 0});
  EXPECT_EQ(successes, 1);
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x80, 0, 0x20, 0, 0, 0, 2, 6});
  ASSERT_EQ(errors.size(), 1);
  EXPECT_EQ(errors.back(), SDOResult::SDO_RESULT_ABORT);
  EXPECT_EQ(abort_code, 0x06020000);
  action.play();
  client.loop();
  client.expire();
  client.loop();
  ASSERT_EQ(errors.size(), 2);
  EXPECT_EQ(errors.back(), SDOResult::SDO_RESULT_TIMEOUT);
  EXPECT_EQ(abort_code, 0);
  bus.result = canbus::ERROR_FAILTX;
  action.play();
  client.loop();
  ASSERT_EQ(errors.size(), 3);
  EXPECT_EQ(errors.back(), SDOResult::SDO_RESULT_TRANSPORT_ERROR);
  EXPECT_EQ(successes, 1);
}

TEST(CANopenSDO, WriteActionReportsImmediateRejectionsExactlyOnce) {
  MockCANbus bus;
  ProbeClient client(&bus);
  SDOWriteAction<> action(&client, 32, 0x2000, 0, 1);
  unsigned errors = 0;
  action.add_on_error_callback([&](SDOResult result, uint32_t code) {
    EXPECT_EQ(result, SDOResult::SDO_RESULT_REJECTED);
    EXPECT_EQ(code, 0);
    errors++;
  });
  action.set_value([]() -> uint32_t { return 300; });
  action.play();
  EXPECT_EQ(errors, 1);
  EXPECT_TRUE(bus.sent.empty());
  action.set_value([]() -> uint32_t { return 42; });
  for (unsigned i = 0; i < 16; i++)
    ASSERT_TRUE(client.write_sdo(33, 0x2000, 0, i, 1));
  action.play();
  EXPECT_EQ(errors, 2);
  client.loop();
  client.expire();
  client.loop();
  EXPECT_EQ(errors, 2);
}

TEST(CANopenSDO, CompletionCallbackCanEnqueueNextWrite) {
  MockCANbus bus;
  ProbeClient client(&bus);
  SDOWriteAction<> action(&client, 32, 0x2000, 0, 1);
  action.set_value([]() -> uint32_t { return 1; });
  action.add_on_success_callback([&]() { EXPECT_TRUE(client.write_sdo(33, 0x2001, 0, 2, 1)); });
  action.play();
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x60, 0, 0x20, 0, 0, 0, 0, 0});
  EXPECT_TRUE(client.is_in_loop_state());
  client.loop();
  ASSERT_EQ(bus.sent.size(), 2);
  EXPECT_EQ(bus.sent.back().can_id, 0x621);
}

TEST(CANopenNMT, CommandsAreStandardFramesAndValidateNodeAndCommand) {
  MockCANbus bus;
  ProbeClient client(&bus);
  for (const auto command : {NMTCommand::NMT_COMMAND_START, NMTCommand::NMT_COMMAND_STOP,
                            NMTCommand::NMT_COMMAND_PRE_OPERATIONAL, NMTCommand::NMT_COMMAND_RESET_NODE,
                            NMTCommand::NMT_COMMAND_RESET_COMMUNICATION}) {
    ASSERT_TRUE(client.send_nmt(32, command));
    EXPECT_EQ(bus.sent.back().can_id, 0);
    EXPECT_EQ(bus.sent.back().can_data_length_code, 2);
    EXPECT_EQ(bus.sent.back().data[0], static_cast<uint8_t>(command));
    EXPECT_EQ(bus.sent.back().data[1], 32);
  }
  EXPECT_TRUE(client.send_nmt(0, NMTCommand::NMT_COMMAND_START));
  EXPECT_FALSE(client.send_nmt(128, NMTCommand::NMT_COMMAND_START));
  EXPECT_FALSE(client.send_nmt(32, static_cast<NMTCommand>(3)));
  EXPECT_EQ(bus.sent.size(), 6);
  bus.result = canbus::ERROR_FAILTX;
  EXPECT_FALSE(client.send_nmt(32, NMTCommand::NMT_COMMAND_START));
}

TEST(CANopenEvents, HeartbeatAndEmergencyWorkWhileIdleAndRejectMalformedFrames) {
  MockCANbus bus;
  ProbeClient client(&bus);
  unsigned heartbeats = 0;
  unsigned emergencies = 0;
  client.add_on_heartbeat_callback([&](uint8_t node, uint8_t state) {
    EXPECT_EQ(node, 32);
    EXPECT_EQ(state, 5);
    heartbeats++;
  });
  client.add_on_emcy_callback([&](uint8_t node, const Emergency &emergency) {
    EXPECT_EQ(node, 33);
    EXPECT_EQ(emergency.error_code, 0x1234);
    EXPECT_EQ(emergency.error_register, 8);
    EXPECT_EQ(emergency.manufacturer_data, (std::array<uint8_t, 5>{1, 2, 3, 4, 5}));
    emergencies++;
  });
  client.loop();
  client.on_frame_(0x720, false, false, {5});
  client.on_frame_(0xA1, false, false, {0x34, 0x12, 8, 1, 2, 3, 4, 5});
  client.on_frame_(0x720, true, false, {5});
  client.on_frame_(0x720, false, true, {5});
  client.on_frame_(0x720, false, false, {});
  client.on_frame_(0x720, false, false, {5, 0});
  client.on_frame_(0x720, false, false, {0x85});
  client.on_frame_(0x700, false, false, {5});
  client.on_frame_(0x780, false, false, {5});
  client.on_frame_(0xA1, false, false, {0x34, 0x12});
  client.on_frame_(0x80, false, false, {0x34, 0x12, 8, 1, 2, 3, 4, 5});
  EXPECT_EQ(heartbeats, 1);
  EXPECT_EQ(emergencies, 1);
  EXPECT_FALSE(client.is_in_loop_state());
}

class ProbeNode : public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
  uint32_t heartbeat_time() const { return this->last_heartbeat_ms_; }
  void set_heartbeat_time(uint32_t value) { this->last_heartbeat_ms_ = value; }
};

TEST(CANopenNode, NodesShareHubAndTrackTheirOwnHeartbeatAndEmergency) {
  MockCANbus bus;
  ProbeClient client(&bus);
  ProbeNode node_a(&client, 32);
  ProbeNode node_b(&client, 33);
  node_a.setup_canopen();
  node_b.setup_canopen();
  unsigned emergencies = 0;
  node_a.add_on_emcy_callback([&](const Emergency &emergency) {
    EXPECT_EQ(emergency.error_code, 0);
    emergencies++;
  });
  EXPECT_FALSE(node_a.is_online(0, 1500));
  client.on_frame_(0x720, false, false, {127});
  EXPECT_TRUE(node_a.is_online(node_a.heartbeat_time() + 1499, 1500));
  EXPECT_FALSE(node_a.is_online(node_a.heartbeat_time() + 1500, 1500));
  EXPECT_EQ(node_a.get_nmt_state(), 127);
  EXPECT_FALSE(node_b.heartbeat_seen());
  client.on_frame_(0x720, false, false, {0});
  EXPECT_EQ(node_a.get_nmt_state(), 0);
  client.on_frame_(0x720, false, false, {4});
  EXPECT_EQ(node_a.get_nmt_state(), 4);
  node_a.set_heartbeat_time(UINT32_MAX - 5);
  EXPECT_EQ(node_a.heartbeat_age(4), 10);
  EXPECT_TRUE(node_a.is_online(4, 11));
  client.on_frame_(0xA1, false, false, {0, 0, 0, 0, 0, 0, 0, 0});
  client.on_frame_(0xA0, false, false, {0, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_EQ(emergencies, 1);
  Reply reply;
  ASSERT_TRUE(node_a.read_sdo(0x2000, 0, Reply::receive, &reply));
  ASSERT_TRUE(node_b.write_sdo(0x2000, 0, 42, 1));
  client.loop();
  EXPECT_EQ(bus.sent.back().can_id, 0x620);
  client.on_frame_(0x5A0, false, false, {0x4F, 0, 0x20, 0, 1, 0, 0, 0});
  client.loop();
  EXPECT_EQ(bus.sent.back().can_id, 0x621);
  EXPECT_TRUE(node_a.send_nmt(NMTCommand::NMT_COMMAND_START));
  EXPECT_EQ(bus.sent.back().data[1], 32);
}

}  // namespace esphome::canopen::testing
