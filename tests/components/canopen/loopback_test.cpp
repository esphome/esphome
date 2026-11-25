#include <gtest/gtest.h>

#include <array>
#include <utility>
#include <vector>

#include "esphome/components/canopen/canopen.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {

// two nodes on one canbus, linked as codegen does: each receives what the other sends (the bus doesn't echo it)
struct LocalPair : ::testing::Test {
  FakeCanbus bus;
  TestHub a{&this->bus, 0x10};
  TestHub b{&this->bus, 0x11};
  void SetUp() override {
    a.set_next_local(&b);
    b.set_next_local(&a);
  }
  void start() {
    a.setup();
    b.setup();
    ASSERT_FALSE(a.is_failed());
    ASSERT_FALSE(b.is_failed());
  }
};

struct Frames {
  std::vector<std::vector<uint8_t>> frames;
  static void on_frame(void *context, uint32_t can_id, const uint8_t *data, uint8_t len) {
    static_cast<Frames *>(context)->frames.emplace_back(data, data + len);
  }
};

TEST_F(LocalPair, HeartbeatReachesTheOtherNode) {
  std::vector<uint8_t> seen;
  b.add_on_heartbeat_callback([&seen](uint8_t node_id, NMTState) { seen.push_back(node_id); });
  a.add_on_heartbeat_callback([&seen](uint8_t node_id, NMTState) { seen.push_back(0x80 | node_id); });
  start();
  // a's boot-up message: on the bus once, received by b in its loop(), not by a itself
  a.loop();
  EXPECT_EQ(bus.count(0x710), 1u);
  EXPECT_TRUE(seen.empty());
  b.loop();  // b also sends its own boot-up message, which a takes in its next loop()
  EXPECT_EQ(seen, std::vector<uint8_t>{0x10});
  a.loop();
  EXPECT_EQ(seen, (std::vector<uint8_t>{0x10, 0x80 | 0x11}));
}

TEST_F(LocalPair, FrameListenerAndSendFrame) {
  Frames on_a, on_b;
  FrameListener listener_a{0x123, 0x7FF, Frames::on_frame, &on_a};
  FrameListener listener_b{0x123, 0x7FF, Frames::on_frame, &on_b};
  a.add_frame_listener(listener_a);
  b.add_frame_listener(listener_b);
  start();
  const uint8_t data[] = {1, 2, 3};
  ASSERT_TRUE(a.send_frame(0x123, data, sizeof(data)));
  EXPECT_EQ(bus.count(0x123), 1u);
  EXPECT_TRUE(on_b.frames.empty());  // queued for b's loop()
  b.loop();
  EXPECT_EQ(on_b.frames, (std::vector<std::vector<uint8_t>>{{1, 2, 3}}));
  a.loop();
  EXPECT_TRUE(on_a.frames.empty());

  // refused by the platform: not sent, so not delivered either
  bus.busy = true;
  EXPECT_FALSE(a.send_frame(0x123, data, sizeof(data)));
  b.loop();
  EXPECT_EQ(on_b.frames.size(), 1u);
}

TEST_F(LocalPair, SdoReadFromTheOtherNode) {
  a.set_identity(0xA59A08F5, 1, 2, 3);
  start();
  struct Result {
    bool done{false};
    SDOReply reply{};
  } result;
  ASSERT_TRUE(b.read_sdo<uint32_t>(
      0x10, 0x1018, 1,
      +[](void *context, const SDOReply &reply) {
        auto *r = static_cast<Result *>(context);
        r->done = true;
        r->reply = reply;
      },
      &result));
  // each node processes what it received in its own loop()
  for (int i = 0; i < 10 && !result.done; i++) {
    b.loop();
    a.loop();
  }
  ASSERT_TRUE(result.done);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(result.reply.as<uint32_t>(), 0xA59A08F5u);
  // the request and the reply went over the bus
  EXPECT_EQ(bus.count(0x610), 1u);
  EXPECT_EQ(bus.count(0x590), 1u);
}

// Objects a write wouldn't change: the EMCY COB-ID (the producer isn't configurable) and the SDO client parameters
// (the client sets its channel for every request). Writes are refused, reads work.
TEST_F(LocalPair, ReadOnlyCommunicationObjects) {
  start();
  struct Result {
    bool done{false};
    SDOReply reply{};
  };
  auto on_reply = +[](void *context, const SDOReply &reply) {
    auto *r = static_cast<Result *>(context);
    r->done = true;
    r->reply = reply;
  };
  auto run = [this](Result &r) {
    for (int i = 0; i < 10 && !r.done; i++) {
      b.loop();
      a.loop();
    }
    ASSERT_TRUE(r.done);
  };
  for (auto [index, sub] : {std::pair<uint16_t, uint8_t>{0x1014, 0}, {0x1280, 1}, {0x1280, 3}}) {
    Result write;
    ASSERT_TRUE(b.write_sdo<uint32_t>(0x10, index, sub, 0x80000000, on_reply, &write));
    run(write);
    EXPECT_EQ(write.reply.result, SDOResult::SDO_RESULT_ABORT) << std::hex << index << ":" << +sub;
    EXPECT_EQ(write.reply.abort_code, 0x06010002u) << std::hex << index << ":" << +sub;  // write to a read only object
  }
  Result read;
  ASSERT_TRUE(b.read_sdo<uint32_t>(0x10, 0x1014, 0, on_reply, &read));
  run(read);
  ASSERT_TRUE(read.reply.ok());
  EXPECT_EQ(read.reply.as<uint32_t>(), 0x90u);  // 0x80 + node id
}

// buffer transfers are segmented (block transfer is optional in servers, and CANopenNode's client doesn't fall back
// when a server rejects it): a 40-byte write and read back
TEST_F(LocalPair, SdoBufferTransfersAreSegmented) {
  uint8_t storage[40] = {};
  a.get_od().add_var(0x2000, OD_READ_WRITE, storage, sizeof(storage));
  start();
  uint8_t data[40];
  for (size_t i = 0; i < sizeof(data); i++)
    data[i] = i + 1;
  struct Result {
    int done{0};
    SDOReply reply{};
    std::vector<uint8_t> read;
  } result;
  auto on_reply = +[](void *context, const SDOReply &reply) {
    auto *r = static_cast<Result *>(context);
    r->done++;
    r->reply = reply;
    if (reply.data != nullptr)
      r->read.assign(reply.data, reply.data + reply.len);
  };
  auto run = [&](int until) {
    for (int i = 0; i < 50 && result.done < until; i++) {
      b.loop();
      a.loop();
    }
  };

  bus.sent.clear();
  ASSERT_TRUE(b.write_sdo_data(0x10, 0x2000, 0, data, sizeof(data), on_reply, &result));
  run(1);
  ASSERT_EQ(result.done, 1);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(std::vector<uint8_t>(storage, storage + 40), std::vector<uint8_t>(data, data + 40));
  // initiate download, segmented with the size indicated (block would be 0xC6)
  const FakeCanbus::Frame *request = bus.find(0x610);
  ASSERT_NE(request, nullptr);
  EXPECT_EQ(request->data[0], 0x21);
  // 40 bytes in 7-byte segments, each acknowledged
  EXPECT_EQ(bus.count(0x610), 1u + 6u);

  bus.sent.clear();
  std::array<uint8_t, 64> buffer{};
  ASSERT_TRUE(b.read_sdo_data(0x10, 0x2000, 0, buffer.data(), buffer.size(), on_reply, &result));
  run(2);
  ASSERT_EQ(result.done, 2);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(result.read, std::vector<uint8_t>(data, data + 40));
  // initiate upload (block would be 0xA4)
  request = bus.find(0x610);
  ASSERT_NE(request, nullptr);
  EXPECT_EQ(request->data[0], 0x40);
}

TEST_F(LocalPair, QueuedFrameDeliveredWhenSent) {
  std::vector<uint8_t> seen;
  b.add_on_heartbeat_callback([&seen](uint8_t node_id, NMTState) { seen.push_back(node_id); });
  start();
  // the boot-up message is refused and stays in CANopenNode's TX buffer
  bus.busy = true;
  a.loop();
  b.loop();
  EXPECT_TRUE(seen.empty());
  // resent by the next processing: delivered once, when it's on the bus
  bus.busy = false;
  a.loop();
  b.loop();
  EXPECT_EQ(bus.count(0x710), 1u);
  EXPECT_EQ(seen, std::vector<uint8_t>{0x10});
}

#ifdef USE_CANOPEN_NMT_MASTER
// CANopenNode keeps one received frame per object: two NMT commands a sends before b's loop() both take effect, in
// order, because b takes them from its queue one at a time.
TEST_F(LocalPair, BackToBackFramesToOneObjectAreKept) {
  start();
  a.loop();
  b.loop();
  ASSERT_EQ(b.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);
  std::vector<NMTState> states;
  b.add_on_nmt_state_callback([&states](NMTState state) { states.push_back(state); });
  ASSERT_TRUE(a.send_nmt(0x11, NMTCommand::NMT_COMMAND_STOP));
  ASSERT_TRUE(a.send_nmt(0x11, NMTCommand::NMT_COMMAND_START));
  b.loop();
  EXPECT_EQ(states, (std::vector<NMTState>{NMTState::NMT_STATE_STOPPED, NMTState::NMT_STATE_OPERATIONAL}));
}
#endif

// A frame a node sends while it handles another reaches the third node after that one, as on a bus.
TEST(CanopenLocal, FramesKeepBusOrder) {
  FakeCanbus bus;
  TestHub a(&bus, 0x10), b(&bus, 0x11), c(&bus, 0x12);
  a.set_next_local(&b);
  b.set_next_local(&c);
  c.set_next_local(&a);
  struct Relay {
    TestHub *hub;
    static void on_frame(void *context, uint32_t, const uint8_t *, uint8_t) {
      const uint8_t data[] = {2};
      static_cast<Relay *>(context)->hub->send_frame(0x124, data, 1);
    }
  } relay{&b};
  std::vector<uint32_t> seen_by_c;
  struct Ids {
    std::vector<uint32_t> *ids;
    static void on_frame(void *context, uint32_t can_id, const uint8_t *, uint8_t) {
      static_cast<Ids *>(context)->ids->push_back(can_id);
    }
  } ids{&seen_by_c};
  FrameListener on_b{0x123, 0x7FF, Relay::on_frame, &relay};
  FrameListener on_c{0x120, 0x7F0, Ids::on_frame, &ids};
  b.add_frame_listener(on_b);
  c.add_frame_listener(on_c);
  a.setup();
  b.setup();
  c.setup();
  const uint8_t data[] = {1};
  ASSERT_TRUE(a.send_frame(0x123, data, 1));
  b.loop();  // b relays 0x124 while it handles 0x123
  c.loop();
  EXPECT_EQ(seen_by_c, (std::vector<uint32_t>{0x123, 0x124}));
}

// The queue holds 16 frames; a frame that finds it full is lost, and the queue goes on afterwards.
TEST_F(LocalPair, FullQueueLosesFrames) {
  Frames on_b;
  FrameListener listener_b{0x123, 0x7FF, Frames::on_frame, &on_b};
  b.add_frame_listener(listener_b);
  start();
  for (uint8_t i = 0; i < 17; i++)
    ASSERT_TRUE(a.send_frame(0x123, &i, 1));
  b.loop();
  ASSERT_EQ(on_b.frames.size(), 16u);
  EXPECT_EQ(on_b.frames.back(), std::vector<uint8_t>{15});
  const uint8_t more = 99;
  ASSERT_TRUE(a.send_frame(0x123, &more, 1));
  b.loop();
  ASSERT_EQ(on_b.frames.size(), 17u);
  EXPECT_EQ(on_b.frames.back(), std::vector<uint8_t>{99});
}

TEST(CanopenLocal, NoOtherNodes) {
  FakeCanbus bus;
  TestHub a(&bus);
  // a ring of one (or none): the node doesn't receive its own frames
  a.set_next_local(&a);
  Frames on_a;
  FrameListener listener_a{0x123, 0x7FF, Frames::on_frame, &on_a};
  a.add_frame_listener(listener_a);
  a.setup();
  const uint8_t data[] = {1};
  ASSERT_TRUE(a.send_frame(0x123, data, 1));
  EXPECT_TRUE(on_a.frames.empty());
}

}  // namespace esphome::canopen::testing
