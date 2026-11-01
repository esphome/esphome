#include <gtest/gtest.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "esphome/components/packet_interface/hdlc_reliability.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::packet_interface::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using Frame = std::vector<uint8_t>;

// A link layer that records what is sent, so a test can deliver it to the peer
class MockLink : public PacketInterface {
 public:
  explicit MockLink(size_t max_packet_size = 64) : PacketInterface(max_packet_size) {}
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    if (this->budget == 0) {
      this->refused++;
      return false;
    }
    this->budget--;
    this->sent.push_back(to_vector(data));
    return true;
  }
  void inject(const Frame &frame) { this->on_receive_from_interface_(PacketBuffer(frame), {}); }

  std::vector<Frame> sent;
  size_t budget{SIZE_MAX};  // frames accepted before the link refuses, like a full network stack
  size_t refused{0};
};

class TestHdlc : public HdlcReliability {
 public:
  using HdlcReliability::HdlcReliability;
  using HdlcReliability::on_timeout_;
  using HdlcReliability::outstanding_;
};

struct Peer {
  explicit Peer(uint8_t queue_length = 4) {
    this->hdlc = std::make_unique<TestHdlc>(&this->link);
    this->hdlc->set_max_queue_length(queue_length);
    this->hdlc->set_max_retries(3);
    this->hdlc->setup();
    this->hdlc->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->hdlc->add_on_session_state_callback([this](bool connected) { this->states.push_back(connected); });
  }
  // Drop the layer's pending timers (ids 0 to 2) so none outlives it
  ~Peer() {
    for (uint32_t id = 0; id < 3; ++id)
      App.scheduler.cancel_timeout(this->hdlc.get(), id);
  }
  bool send(const Frame &data) { return this->hdlc->send_to_interface(PacketBuffer(data), {}); }

  MockLink link;
  std::unique_ptr<TestHdlc> hdlc;
  std::vector<Frame> received;
  std::vector<bool> states;
};

// Returns true to drop a frame on its way from one peer to the other
using DropFilter = std::function<bool(const Frame &)>;

static bool is_info(const Frame &frame) { return (frame[0] & 1) == 0; }
static bool is_ack(const Frame &frame) { return (frame[0] & 0x0F) == 0x01; }

// Runs a layer's loop only while it has asked for it, as the main loop does
static void run_loop(TestHdlc &hdlc) {
  if (!hdlc.is_idle())
    hdlc.loop();
}

// Move frames between the peers and run their loops until nothing more is sent
static void pump(Peer &a, Peer &b, const DropFilter &drop_a_to_b = nullptr, const DropFilter &drop_b_to_a = nullptr) {
  for (int i = 0; i != 50; i++) {
    run_loop(*a.hdlc);
    run_loop(*b.hdlc);
    if (a.link.sent.empty() && b.link.sent.empty())
      return;
    auto from_a = std::move(a.link.sent);
    auto from_b = std::move(b.link.sent);
    a.link.sent.clear();
    b.link.sent.clear();
    for (auto &frame : from_a) {
      if (!drop_a_to_b || !drop_a_to_b(frame))
        b.link.inject(frame);
    }
    for (auto &frame : from_b) {
      if (!drop_b_to_a || !drop_b_to_a(frame))
        a.link.inject(frame);
    }
  }
  FAIL() << "Peers did not go quiet";
}

static Frame make_frame(uint8_t control, const Frame &payload) {
  Frame frame{control};
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}

class HdlcReliabilityTest : public ::testing::Test {
 protected:
  // Free the timers each Peer cancelled, so none is left in the scheduler for a later test
  void TearDown() override { App.scheduler.call(millis()); }
};

TEST_F(HdlcReliabilityTest, MaxPacketSizeAllowsForOverhead) {
  Peer a;
  EXPECT_EQ(a.hdlc->get_send_capacity(), 4);
  EXPECT_EQ(a.hdlc->get_max_packet_size(), 64 - HdlcReliability::FRAME_OVERHEAD);
  EXPECT_EQ(a.link.top(), a.hdlc.get());
  EXPECT_EQ(a.hdlc->get_capabilities(), CAPABILITY_RELIABILITY);
}

TEST_F(HdlcReliabilityTest, ResetHandshake) {
  Peer a, b;
  a.hdlc->loop();
  ASSERT_EQ(a.link.sent.size(), 1);
  EXPECT_EQ(a.link.sent[0], make_frame(0x2F, {}));  // SABM
  pump(a, b);
  EXPECT_TRUE(a.hdlc->is_connected());
  EXPECT_TRUE(b.hdlc->is_connected());
}

TEST_F(HdlcReliabilityTest, RepeatedSabmBeforeNumberedFramesKeepsQueue) {
  Peer a, b;
  pump(a, b);
  // A frame is queued and sent, but has not reached A, so A has sent no numbered frame
  ASSERT_TRUE(b.send({0x42}));
  auto held = std::move(b.link.sent);
  b.link.sent.clear();
  EXPECT_EQ(b.hdlc->get_send_capacity(), 3);

  // A repeats its SABM because it did not hear the UA; that is not a restart
  b.link.inject(make_frame(0x2F, {}));  // SABM
  EXPECT_TRUE(b.hdlc->is_connected());
  EXPECT_TRUE(b.states.empty());              // no session lost reported
  EXPECT_EQ(b.hdlc->get_send_capacity(), 3);  // the queued frame was kept
  ASSERT_FALSE(b.link.sent.empty());
  EXPECT_EQ(b.link.sent[0], make_frame(0x63, {}));  // UA, then the frame is sent again

  // The frame is still delivered when it reaches A
  for (auto &frame : held)
    a.link.inject(frame);
  EXPECT_EQ(a.received, std::vector<Frame>{{0x42}});
}

TEST_F(HdlcReliabilityTest, SabmAfterNumberedFramesIsARestart) {
  Peer a, b;
  pump(a, b);
  // B has heard a numbered frame, so a SABM now means A has started again
  ASSERT_TRUE(a.send({0x10}));
  pump(a, b);
  b.link.inject(make_frame(0x2F, {}));  // SABM
  EXPECT_TRUE(b.hdlc->is_connected());
  EXPECT_EQ(b.states, std::vector<bool>{false});
}

TEST_F(HdlcReliabilityTest, QueuedBeforeConnectIsSentAfter) {
  Peer a, b;
  ASSERT_TRUE(a.send({0x01}));
  ASSERT_TRUE(a.send({0x02}));
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x01}, {0x02}}));
}

TEST_F(HdlcReliabilityTest, DeliversInOrderBothWays) {
  Peer a, b;
  pump(a, b);
  a.send({0x10});
  b.send({0x20});
  a.send({0x11});
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x10}, {0x11}}));
  EXPECT_EQ(a.received, (std::vector<Frame>{{0x20}}));
}

TEST_F(HdlcReliabilityTest, SequenceNumbersWrap) {
  Peer a(3), b(3);
  pump(a, b);
  std::vector<Frame> expected;
  for (uint8_t i = 0; i != 20; i++) {
    expected.push_back({i, static_cast<uint8_t>(i * 3)});
    ASSERT_TRUE(a.send(expected.back()));
    if (i % 2 == 1)
      pump(a, b);
  }
  pump(a, b);
  EXPECT_EQ(b.received, expected);
}

TEST_F(HdlcReliabilityTest, GapRecoveredByReject) {
  Peer a, b;
  pump(a, b);
  bool dropped = false;
  a.send({0x01});
  a.send({0x02});
  a.send({0x03});
  pump(a, b, [&dropped](const Frame &frame) {
    if (dropped || !is_info(frame))
      return false;
    dropped = true;
    return true;
  });
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x01}, {0x02}, {0x03}}));
}

TEST_F(HdlcReliabilityTest, LossRecoveredByTimeout) {
  Peer a, b;
  pump(a, b);
  a.send({0x42});
  pump(a, b, [](const Frame &frame) { return is_info(frame); });
  EXPECT_TRUE(b.received.empty());
  a.hdlc->on_timeout_();
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x42}}));
}

TEST_F(HdlcReliabilityTest, LostAckDoesNotDuplicate) {
  Peer a, b;
  pump(a, b);
  a.send({0x42});
  pump(a, b, nullptr, [](const Frame &frame) { return is_ack(frame); });
  ASSERT_EQ(b.received.size(), 1);
  a.hdlc->on_timeout_();
  pump(a, b);
  EXPECT_EQ(b.received.size(), 1);
  // The resend was acknowledged, so the full queue is available again
  for (int i = 0; i != 4; i++)
    EXPECT_TRUE(a.send({0x01}));
}

TEST_F(HdlcReliabilityTest, QueueFull) {
  Peer a(2), b(2);
  pump(a, b);
  EXPECT_TRUE(a.send({0x01}));
  EXPECT_TRUE(a.send({0x02}));
  EXPECT_FALSE(a.send({0x03}));
}

TEST_F(HdlcReliabilityTest, OversizedPacketRejected) {
  Peer a;
  EXPECT_FALSE(a.send(Frame(a.hdlc->get_max_packet_size() + 1)));
}

TEST_F(HdlcReliabilityTest, CrcErrorDropped) {
  Peer a, b;
  pump(a, b);
  Frame frame = make_frame(0x00, {0x55});  // I-frame N(S)=0 N(R)=0
  frame[1] ^= 0xFF;
  b.link.inject(frame);
  EXPECT_TRUE(b.received.empty());
  b.link.inject(make_frame(0x00, {0x55}));
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x55}}));
}

TEST_F(HdlcReliabilityTest, RetriesExhaustedResetsLink) {
  Peer a, b;
  pump(a, b);
  // One frame is delivered first, so b can tell a reset from a repeated SABM
  ASSERT_TRUE(a.send({0x00}));
  pump(a, b);
  ASSERT_EQ(b.received, std::vector<Frame>{{0x00}});
  a.send({0x01});
  pump(a, b, [](const Frame &frame) { return is_info(frame); });
  for (int i = 0; i != 4; i++) {
    a.hdlc->on_timeout_();
    pump(a, b, [](const Frame &frame) { return is_info(frame); });
  }
  // The unacknowledged frame was discarded and the link was reset, which both report as data lost
  EXPECT_EQ(b.received, std::vector<Frame>{{0x00}});
  EXPECT_TRUE(a.hdlc->is_connected());
  EXPECT_EQ(a.states, std::vector<bool>{false});
  EXPECT_EQ(b.states, std::vector<bool>{false});
  a.send({0x02});
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0x00}, {0x02}}));
}

TEST_F(HdlcReliabilityTest, PeerRestart) {
  Peer a;
  auto b = std::make_unique<Peer>();
  pump(a, *b);
  a.send({0x01});
  pump(a, *b);
  // The peer restarts with fresh sequence numbers
  b = std::make_unique<Peer>();
  pump(a, *b);
  a.send({0x02});
  b->send({0x03});
  pump(a, *b);
  EXPECT_EQ(b->received, (std::vector<Frame>{{0x02}}));
  EXPECT_EQ(a.received, (std::vector<Frame>{{0x03}}));
  // The restarted peer's reset discarded what a had not yet had acknowledged
  EXPECT_EQ(a.states, std::vector<bool>{false});
  EXPECT_TRUE(b->states.empty());
}

// Waits past the retry after a refused send, servicing timers as the main loop does
static void run_send_retry() {
  const uint32_t start = millis();
  while (millis() - start < 60) {
    App.scheduler.call(millis());
    delay(1);
  }
  App.scheduler.call(millis());
}

TEST_F(HdlcReliabilityTest, RefusedFramesSentAgainWithoutTimeout) {
  Peer a, b;
  // Only the retry of a refused send can fire while the test runs, not a retransmit
  a.hdlc->set_ack_timeout(10000);
  b.hdlc->set_ack_timeout(10000);
  for (uint8_t i = 0; i != 3; i++)
    ASSERT_TRUE(a.send({i}));
  a.link.budget = 3;  // SABM, UA to the peer's SABM, and the first queued frame
  pump(a, b);
  EXPECT_GE(a.link.refused, 1);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0}}));
  a.link.budget = SIZE_MAX;  // the lower layer accepts again, as when a radio stops receiving
  // The layer waits for its retry rather than running every loop
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0}}));
  run_send_retry();
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{0}, {1}, {2}}));
  EXPECT_EQ(a.hdlc->outstanding_(), 0);
}

TEST_F(HdlcReliabilityTest, RefusedAckSentAgain) {
  Peer a, b;
  a.hdlc->set_ack_timeout(10000);
  b.hdlc->set_ack_timeout(10000);
  pump(a, b);
  a.send({1});
  for (auto &frame : a.link.sent)
    b.link.inject(frame);
  a.link.sent.clear();
  b.link.budget = 0;
  b.hdlc->loop();
  EXPECT_TRUE(b.link.sent.empty());
  b.link.budget = SIZE_MAX;
  pump(a, b);
  EXPECT_EQ(a.hdlc->outstanding_(), 1);  // the RR waits for the retry
  run_send_retry();
  pump(a, b);
  EXPECT_EQ(a.hdlc->outstanding_(), 0);
}

TEST_F(HdlcReliabilityTest, AckDelayLetsReplyCarryAck) {
  Peer a, b;
  b.hdlc->set_ack_delay(100);
  pump(a, b);
  a.send({1});
  for (auto &frame : a.link.sent)
    b.link.inject(frame);
  a.link.sent.clear();
  b.hdlc->loop();
  EXPECT_TRUE(b.link.sent.empty());  // no RR yet
  b.send({2});
  ASSERT_EQ(b.link.sent.size(), 1);
  EXPECT_TRUE(is_info(b.link.sent[0]));
  EXPECT_EQ(b.link.sent[0][0] >> 5, 1);  // N(R) acknowledges the frame from a
  b.link.sent.clear();
  b.hdlc->loop();
  EXPECT_TRUE(b.link.sent.empty());  // the RR is no longer owed
}

TEST_F(HdlcReliabilityTest, MaxRetriesOf255ResetsLink) {
  Peer a, b;
  a.hdlc->set_max_retries(255);
  pump(a, b);
  a.send({0x01});
  a.link.sent.clear();
  for (int i = 0; i != 255; i++) {
    a.hdlc->on_timeout_();
    a.link.sent.clear();
  }
  EXPECT_TRUE(a.states.empty());
  a.hdlc->on_timeout_();
  EXPECT_EQ(a.states, std::vector<bool>{false});
}

TEST_F(HdlcReliabilityTest, OversizedFrameDropped) {
  Peer a, b;
  pump(a, b);
  // An I-frame, N(S) 0 and N(R) 0, one byte larger than this layer's packets
  b.link.inject(make_frame(0x00, Frame(b.hdlc->get_max_packet_size() + 1, 0x55)));
  EXPECT_TRUE(b.received.empty());
  b.link.inject(make_frame(0x00, Frame(b.hdlc->get_max_packet_size(), 0x55)));
  EXPECT_EQ(b.received.size(), 1);
}

TEST_F(HdlcReliabilityTest, LinkTooSmallFailsSetup) {
  MockLink link(HdlcReliability::FRAME_OVERHEAD);
  TestHdlc hdlc(&link);
  EXPECT_EQ(hdlc.get_max_packet_size(), 0);
  hdlc.setup();
  EXPECT_TRUE(hdlc.is_failed());
  Frame packet{1};
  EXPECT_FALSE(hdlc.send_to_interface(PacketBuffer(packet), {}));
  EXPECT_EQ(hdlc.get_send_capacity(), 0);
}

TEST_F(HdlcReliabilityTest, AckDelayTimerSendsAck) {
  Peer a, b;
  b.hdlc->set_ack_delay(5);
  pump(a, b);
  a.send({1});
  for (auto &frame : a.link.sent)
    b.link.inject(frame);
  a.link.sent.clear();
  b.hdlc->loop();
  EXPECT_TRUE(b.link.sent.empty());
  delay(10);
  App.scheduler.call(millis());
  b.hdlc->loop();
  ASSERT_EQ(b.link.sent.size(), 1);
  EXPECT_TRUE(is_ack(b.link.sent[0]));
  EXPECT_EQ(b.link.sent[0][0] >> 5, 1);  // N(R) acknowledges the frame from a
}

TEST_F(HdlcReliabilityTest, RefusedRejectFallsBackToAck) {
  Peer a, b;
  pump(a, b);
  a.send({1});
  a.send({2});
  ASSERT_EQ(a.link.sent.size(), 2);
  // The first frame is lost, and the REJ for the gap is refused
  b.link.budget = 0;
  b.link.inject(a.link.sent[1]);
  a.link.sent.clear();
  EXPECT_EQ(b.link.refused, 1);
  b.link.budget = SIZE_MAX;
  b.hdlc->loop();
  ASSERT_EQ(b.link.sent.size(), 1);
  EXPECT_TRUE(is_ack(b.link.sent[0]));
  EXPECT_EQ(b.link.sent[0][0] >> 5, 0);
  a.hdlc->on_timeout_();
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Frame>{{1}, {2}}));
}

}  // namespace esphome::packet_interface::testing
