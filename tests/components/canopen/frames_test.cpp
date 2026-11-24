#include <gtest/gtest.h>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

TEST(CanopenDecodeHeartbeat, NodeAndState) {
  uint8_t node = 0;
  NMTState state = NMTState::NMT_STATE_UNKNOWN;
  const uint8_t op[] = {0x05}, preop[] = {0x7F}, boot[] = {0x00}, stopped[] = {0x04};
  ASSERT_TRUE(decode_heartbeat(0x701, op, 1, node, state));
  EXPECT_EQ(node, 1);
  EXPECT_EQ(state, NMTState::NMT_STATE_OPERATIONAL);
  ASSERT_TRUE(decode_heartbeat(0x77F, preop, 1, node, state));
  EXPECT_EQ(node, 0x7F);
  EXPECT_EQ(state, NMTState::NMT_STATE_PRE_OPERATIONAL);
  ASSERT_TRUE(decode_heartbeat(0x710, boot, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_BOOTUP);
  ASSERT_TRUE(decode_heartbeat(0x710, stopped, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_STOPPED);
}

TEST(CanopenDecodeHeartbeat, UnknownStateAndToggleBit) {
  uint8_t node = 0;
  NMTState state = NMTState::NMT_STATE_BOOTUP;
  const uint8_t odd[] = {0x42}, toggled[] = {0x85};
  ASSERT_TRUE(decode_heartbeat(0x710, odd, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_UNKNOWN);
  // bit 7 is the node guarding toggle bit
  ASSERT_TRUE(decode_heartbeat(0x710, toggled, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_OPERATIONAL);
}

TEST(CanopenDecodeHeartbeat, OtherFrames) {
  uint8_t node = 0;
  NMTState state;
  const uint8_t op[] = {0x05};
  EXPECT_FALSE(decode_heartbeat(0x700, op, 1, node, state));  // node 0 doesn't exist
  EXPECT_FALSE(decode_heartbeat(0x780, op, 1, node, state));
  EXPECT_FALSE(decode_heartbeat(0x701, op, 0, node, state));
  EXPECT_FALSE(decode_heartbeat(0x581, op, 1, node, state));
}

TEST(CanopenDecodeEmcy, Fields) {
  uint8_t node = 0;
  Emergency e{};
  const uint8_t emcy[] = {0x10, 0x81, 0x11, 1, 2, 3, 4, 5};
  ASSERT_TRUE(decode_emcy(0x8A, emcy, 8, node, e));
  EXPECT_EQ(node, 0x0A);
  EXPECT_EQ(e.error_code, 0x8110);
  EXPECT_EQ(e.error_register, 0x11);
  EXPECT_EQ(e.manufacturer_data, (std::array<uint8_t, 5>{1, 2, 3, 4, 5}));
  ASSERT_TRUE(decode_emcy(0xFF, emcy, 8, node, e));
  EXPECT_EQ(node, 0x7F);
}

TEST(CanopenDecodeEmcy, OtherFrames) {
  uint8_t node = 0;
  Emergency e{};
  const uint8_t emcy[8] = {};
  EXPECT_FALSE(decode_emcy(0x80, emcy, 8, node, e));  // SYNC
  EXPECT_FALSE(decode_emcy(0x100, emcy, 8, node, e));
  EXPECT_FALSE(decode_emcy(0x81, emcy, 7, node, e));
  EXPECT_FALSE(decode_emcy(0x181, emcy, 8, node, e));  // TPDO1
}

class FramesTest : public ::testing::Test {
 protected:
  struct Received {
    uint32_t can_id;
    std::vector<uint8_t> data;
  };
  FakeCanbus bus_;
  TestHub hub_{&this->bus_};
  std::vector<Received> received_;

  static void on_frame(void *context, uint32_t can_id, const uint8_t *data, uint8_t len) {
    static_cast<FramesTest *>(context)->received_.push_back({can_id, {data, data + len}});
  }
};

TEST_F(FramesTest, ListenerGetsMatchingFrames) {
  // before setup(): kept for it
  this->hub_.add_frame_listener(0x180, 0x780, on_frame, this);
  this->hub_.setup();
  ASSERT_FALSE(this->hub_.is_failed());
  this->hub_.frame(0x18A, {1, 2, 3});
  this->hub_.frame(0x20A, {4});
  this->hub_.frame(0x1FF, {});
  ASSERT_EQ(this->received_.size(), 2u);
  EXPECT_EQ(this->received_[0].can_id, 0x18Au);
  EXPECT_EQ(this->received_[0].data, (std::vector<uint8_t>{1, 2, 3}));
  EXPECT_EQ(this->received_[1].can_id, 0x1FFu);
  EXPECT_TRUE(this->received_[1].data.empty());
}

TEST_F(FramesTest, ListenerSkipsRemoteAndExtendedFrames) {
  this->hub_.setup();
  this->hub_.add_frame_listener(0x18A, 0x7FF, on_frame, this);
  canbus::CanFrame rtr = make_frame(0x18A, {});
  rtr.remote_transmission_request = true;
  this->hub_.frame(rtr);
  canbus::CanFrame ext = make_frame(0x18A, {1});
  ext.use_extended_id = true;
  this->hub_.frame(ext);
  EXPECT_TRUE(this->received_.empty());
}

TEST_F(FramesTest, HeartbeatAndEmcyCallbacks) {
  std::vector<std::pair<uint8_t, NMTState>> heartbeats;
  std::vector<std::pair<uint8_t, uint16_t>> emcys;
  this->hub_.add_on_heartbeat_callback(
      [&heartbeats](uint8_t node, NMTState state) { heartbeats.emplace_back(node, state); });
  this->hub_.add_on_emcy_callback(
      [&emcys](uint8_t node, const Emergency &e) { emcys.emplace_back(node, e.error_code); });
  this->hub_.setup();
  this->hub_.frame(0x70A, {0x05});
  this->hub_.frame(0x70B, {0x00});
  this->hub_.frame(0x8A, {0x10, 0x81, 0x11, 0, 0, 0, 0, 0});
  this->hub_.frame(0x80, {});  // SYNC
  this->hub_.frame(0x58A, {0x60, 0, 0, 0, 0, 0, 0, 0});
  ASSERT_EQ(heartbeats.size(), 2u);
  EXPECT_EQ(heartbeats[0], std::make_pair(uint8_t{0x0A}, NMTState::NMT_STATE_OPERATIONAL));
  EXPECT_EQ(heartbeats[1], std::make_pair(uint8_t{0x0B}, NMTState::NMT_STATE_BOOTUP));
  ASSERT_EQ(emcys.size(), 1u);
  EXPECT_EQ(emcys[0], std::make_pair(uint8_t{0x0A}, uint16_t{0x8110}));
}

TEST_F(FramesTest, ListenerSeesSdoRepliesToo) {
  this->hub_.setup();
  this->hub_.add_frame_listener(0x5A1, 0x7FF, on_frame, this);
  std::vector<SDOReply> replies;
  ASSERT_TRUE(this->hub_.read_sdo<uint8_t>(
      0x21, 0x1001, 0,
      [](void *context, const SDOReply &reply) { static_cast<std::vector<SDOReply> *>(context)->push_back(reply); },
      &replies));
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x4F, 0x01, 0x10, 0, 0x05, 0, 0, 0});
  ASSERT_EQ(replies.size(), 1u);
  EXPECT_EQ(replies[0].value, 5u);
  EXPECT_EQ(this->received_.size(), 1u);
}

TEST_F(FramesTest, SendFrame) {
  this->hub_.setup();
  const uint8_t data[] = {1, 2};
  EXPECT_TRUE(this->hub_.send_frame(0x210, data, sizeof(data)));
  ASSERT_EQ(this->bus_.sent.size(), 1u);
  EXPECT_EQ(this->bus_.sent[0].can_id, 0x210u);
  EXPECT_EQ(this->bus_.sent[0].data, (std::vector<uint8_t>{1, 2}));
  const uint8_t long_data[9] = {};
  EXPECT_FALSE(this->hub_.send_frame(0x210, long_data, sizeof(long_data)));
  this->bus_.busy = true;
  EXPECT_FALSE(this->hub_.send_frame(0x210, data, sizeof(data)));
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

}  // namespace esphome::canopen::testing
