#include <gtest/gtest.h>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

struct HeartbeatCase {
  uint32_t can_id;
  uint8_t byte;
  uint8_t node;
  NMTState state;
};

class CanopenDecodeHeartbeat : public ::testing::TestWithParam<HeartbeatCase> {};

TEST_P(CanopenDecodeHeartbeat, NodeAndState) {
  const HeartbeatCase &c = GetParam();
  uint8_t node = 0;
  NMTState state = NMTState::NMT_STATE_BOOTUP;
  ASSERT_TRUE(decode_heartbeat(c.can_id, &c.byte, 1, node, state));
  EXPECT_EQ(node, c.node);
  EXPECT_EQ(state, c.state);
}

INSTANTIATE_TEST_SUITE_P(States, CanopenDecodeHeartbeat,
                         ::testing::Values(HeartbeatCase{0x701, 0x05, 0x01, NMTState::NMT_STATE_OPERATIONAL},
                                           HeartbeatCase{0x77F, 0x7F, 0x7F, NMTState::NMT_STATE_PRE_OPERATIONAL},
                                           HeartbeatCase{0x710, 0x00, 0x10, NMTState::NMT_STATE_BOOTUP},
                                           HeartbeatCase{0x710, 0x04, 0x10, NMTState::NMT_STATE_STOPPED},
                                           HeartbeatCase{0x710, 0x42, 0x10, NMTState::NMT_STATE_UNKNOWN},
                                           // bit 7 is the node guarding toggle bit
                                           HeartbeatCase{0x710, 0x85, 0x10, NMTState::NMT_STATE_OPERATIONAL}));

struct OtherFrameCase {
  uint32_t can_id;
  size_t len;
};

class CanopenDecodeHeartbeatOther : public ::testing::TestWithParam<OtherFrameCase> {};

TEST_P(CanopenDecodeHeartbeatOther, Rejected) {
  uint8_t node = 0;
  NMTState state;
  const uint8_t op[] = {0x05};
  EXPECT_FALSE(decode_heartbeat(GetParam().can_id, op, GetParam().len, node, state));
}

INSTANTIATE_TEST_SUITE_P(Frames, CanopenDecodeHeartbeatOther,
                         ::testing::Values(OtherFrameCase{0x700, 1},  // node 0 doesn't exist
                                           OtherFrameCase{0x780, 1}, OtherFrameCase{0x701, 0},
                                           OtherFrameCase{0x581, 1}));

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

class CanopenDecodeEmcyOther : public ::testing::TestWithParam<OtherFrameCase> {};

TEST_P(CanopenDecodeEmcyOther, Rejected) {
  uint8_t node = 0;
  Emergency e{};
  const uint8_t emcy[8] = {};
  EXPECT_FALSE(decode_emcy(GetParam().can_id, emcy, GetParam().len, node, e));
}

INSTANTIATE_TEST_SUITE_P(Frames, CanopenDecodeEmcyOther,
                         ::testing::Values(OtherFrameCase{0x80, 8},  // SYNC
                                           OtherFrameCase{0x100, 8}, OtherFrameCase{0x81, 7},
                                           OtherFrameCase{0x181, 8}));  // TPDO1

class NmtStateToString : public ::testing::TestWithParam<std::pair<NMTState, const char *>> {};

TEST_P(NmtStateToString, Name) { EXPECT_STREQ(LOG_STR_ARG(nmt_state_to_string(GetParam().first)), GetParam().second); }

INSTANTIATE_TEST_SUITE_P(States, NmtStateToString,
                         ::testing::Values(std::make_pair(NMTState::NMT_STATE_BOOTUP, "bootup"),
                                           std::make_pair(NMTState::NMT_STATE_STOPPED, "stopped"),
                                           std::make_pair(NMTState::NMT_STATE_OPERATIONAL, "operational"),
                                           std::make_pair(NMTState::NMT_STATE_PRE_OPERATIONAL, "pre-operational"),
                                           std::make_pair(NMTState::NMT_STATE_UNKNOWN, "unknown"),
                                           std::make_pair(static_cast<NMTState>(3), "unknown")));

class FramesTest : public ::testing::Test {
 protected:
  struct Received {
    uint32_t can_id;
    std::vector<uint8_t> data;
  };
  FakeCanbus bus_;
  TestHub hub_{&this->bus_, CANopenComponent::NO_NODE_ID};
  std::vector<Received> received_;

  static void on_frame(void *context, uint32_t can_id, const uint8_t *data, uint8_t len) {
    static_cast<FramesTest *>(context)->received_.push_back({can_id, {data, data + len}});
  }
  // listeners live as long as the hub
  void listen_(uint16_t can_id, uint16_t mask) {
    FrameListener &l = this->listeners_[this->listener_count_++];
    l = FrameListener{can_id, mask, on_frame, this};
    this->hub_.add_frame_listener(l);
  }
  FrameListener listeners_[4]{};
  size_t listener_count_{0};
};

TEST_F(FramesTest, ListenerGetsMatchingFrames) {
  // before setup(): kept for it
  this->listen_(0x180, 0x780);
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

TEST_F(FramesTest, ListenerAddedTwiceIsListedOnce) {
  this->hub_.setup();
  this->listen_(0x18A, 0x7FF);
  this->hub_.add_frame_listener(this->listeners_[0]);
  this->hub_.frame(0x18A, {1});
  EXPECT_EQ(this->received_.size(), 1u);
}

// A listener added from a callback gets frames from the next one on; adding the running one again does nothing
TEST_F(FramesTest, ListenerAddedFromACallback) {
  struct Context {
    CANopenComponent *hub;
    FrameListener first;
    FrameListener second;
    int first_calls{0};
    int second_calls{0};
  };
  static Context ctx{};
  ctx = Context{&this->hub_,
                {0x18A, 0x7FF,
                 [](void *, uint32_t, const uint8_t *, uint8_t) {
                   ctx.first_calls++;
                   ctx.hub->add_frame_listener(ctx.second);
                   ctx.hub->add_frame_listener(ctx.first);
                 },
                 nullptr},
                {0x18A, 0x7FF, [](void *, uint32_t, const uint8_t *, uint8_t) { ctx.second_calls++; }, nullptr}};
  this->hub_.setup();
  this->hub_.add_frame_listener(ctx.first);
  this->hub_.frame(0x18A, {1});
  EXPECT_EQ(ctx.first_calls, 1);
  EXPECT_EQ(ctx.second_calls, 0);
  this->hub_.frame(0x18A, {2});
  EXPECT_EQ(ctx.first_calls, 2);
  EXPECT_EQ(ctx.second_calls, 1);
}

TEST_F(FramesTest, ListenerSkipsRemoteAndExtendedFrames) {
  this->hub_.setup();
  this->listen_(0x18A, 0x7FF);
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
  this->hub_.frame(0x8A, {0, 0, 0, 0, 0, 0, 0, 0});  // error reset: logged at INFO, the callback still runs
  this->hub_.frame(0x80, {});                        // SYNC
  this->hub_.frame(0x58A, {0x60, 0, 0, 0, 0, 0, 0, 0});
  ASSERT_EQ(heartbeats.size(), 2u);
  EXPECT_EQ(heartbeats[0], std::make_pair(uint8_t{0x0A}, NMTState::NMT_STATE_OPERATIONAL));
  EXPECT_EQ(heartbeats[1], std::make_pair(uint8_t{0x0B}, NMTState::NMT_STATE_BOOTUP));
  ASSERT_EQ(emcys.size(), 2u);
  EXPECT_EQ(emcys[0], std::make_pair(uint8_t{0x0A}, uint16_t{0x8110}));
  EXPECT_EQ(emcys[1], std::make_pair(uint8_t{0x0A}, uint16_t{0}));
}

TEST_F(FramesTest, ListenerSeesSdoRepliesToo) {
  this->hub_.setup();
  this->listen_(0x5A1, 0x7FF);
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
  EXPECT_FALSE(this->hub_.send_frame(0xA10, data, sizeof(data)));  // not cut to 0x210
  const std::vector<uint8_t> big(257);
  EXPECT_FALSE(this->hub_.send_frame(0x210, big.data(), big.size()));  // not cut to 1 byte
  const uint8_t long_data[9] = {};
  EXPECT_FALSE(this->hub_.send_frame(0x210, long_data, sizeof(long_data)));
  this->bus_.busy = true;
  EXPECT_FALSE(this->hub_.send_frame(0x210, data, sizeof(data)));
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

TEST_F(FramesTest, ListenerMatchingNoStandardIdIsNotAdded) {
  this->hub_.setup();
  this->listen_(0x1000, 0x1000);
  this->hub_.frame(0x000, {1});
  this->hub_.frame(0x18A, {1});
  EXPECT_TRUE(this->received_.empty());
  // high mask bits with can_id 0 there match every standard ID
  this->listen_(0x0, 0x1000);
  this->hub_.frame(0x18A, {1});
  EXPECT_EQ(this->received_.size(), 1u);
}

}  // namespace esphome::canopen::testing
