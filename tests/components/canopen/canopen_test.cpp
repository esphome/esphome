#include <gtest/gtest.h>
#include "test_helpers.h"
#include "esphome/components/canopen/canopen_node.h"

namespace esphome::canopen::testing {

struct Reply {
  unsigned count{0};
  SDOResult result{};
  uint32_t value{0};
  uint8_t size{0};
  static void receive(void *context, const SDOReply &response) {
    auto *reply = static_cast<Reply *>(context);
    reply->count++;
    reply->result = response.result;
    reply->value = response.value;
    reply->size = response.size;
  }
};

TEST(CANopenNMT, CommandsAreStandardFramesAndValidateNodeAndCommand) {
  MockCANbus bus;
  ProbeClient client(&bus);
  for (const auto command :
       {NMTCommand::NMT_COMMAND_START, NMTCommand::NMT_COMMAND_STOP, NMTCommand::NMT_COMMAND_PRE_OPERATIONAL,
        NMTCommand::NMT_COMMAND_RESET_NODE, NMTCommand::NMT_COMMAND_RESET_COMMUNICATION}) {
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
  EXPECT_EQ(reply.count, 1);
  EXPECT_EQ(reply.value, 1);
  EXPECT_EQ(bus.sent.back().can_id, 0x621);
  EXPECT_TRUE(node_a.send_nmt(NMTCommand::NMT_COMMAND_START));
  EXPECT_EQ(bus.sent.back().data[1], 32);
}

}  // namespace esphome::canopen::testing
