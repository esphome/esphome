#include <gtest/gtest.h>

#include <array>
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
  // a's boot-up message: on the bus once, received by b, not by a itself
  a.loop();
  EXPECT_EQ(bus.count(0x710), 1u);
  EXPECT_EQ(seen, std::vector<uint8_t>{0x10});
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
  EXPECT_EQ(on_b.frames, (std::vector<std::vector<uint8_t>>{{1, 2, 3}}));
  EXPECT_TRUE(on_a.frames.empty());

  // refused by the platform: not sent, so not delivered either
  bus.busy = true;
  EXPECT_FALSE(a.send_frame(0x123, data, sizeof(data)));
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
  EXPECT_TRUE(seen.empty());
  // resent by the next processing: delivered once, when it's on the bus
  bus.busy = false;
  a.loop();
  EXPECT_EQ(bus.count(0x710), 1u);
  EXPECT_EQ(seen, std::vector<uint8_t>{0x10});
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
