#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "esphome/components/packet_interface/basic_session.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::session_testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using Packet = std::vector<uint8_t>;

// A link layer that records what is sent, so a test can deliver it to the peer
class MockLink : public PacketInterface {
 public:
  MockLink() { this->set_max_packet_size(64); }
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    if (!this->accept)
      return false;
    this->sent.push_back(to_vector(data));
    return true;
  }
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }
  // Act as a reliability layer that resets and discards data
  void set_reliable() { this->capabilities_ = CAPABILITY_RELIABILITY; }
  void report_data_lost() { this->session_callback_.call(false); }

  std::vector<Packet> sent;
  bool accept{true};
};

class TestSession : public BasicSession {
 public:
  using BasicSession::BasicSession;
  using BasicSession::on_interval_;
  unsigned get_not_understood_count() const { return this->not_understood_count_; }
};

struct Peer {
  Peer() {
    this->session = std::make_unique<TestSession>(&this->link);
    this->session->set_keep_alive(1000);
    this->session->set_session_timeout(3000);
    this->session->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->session->add_on_session_state_callback([this](bool connected) { this->states.push_back(connected); });
    this->session->setup();
  }
  bool send(const Packet &data) { return this->session->send_to_interface(PacketBuffer(data), {}); }

  MockLink link;
  std::unique_ptr<TestSession> session;
  std::vector<Packet> received;
  std::vector<bool> states;
};

static bool is_keep_alive(const Packet &packet) { return !packet.empty() && packet[0] == 0x01; }

// Move packets between the peers until nothing more is sent
static void pump(Peer &a, Peer &b) {
  for (int i = 0; i != 20; i++) {
    if (a.link.sent.empty() && b.link.sent.empty())
      return;
    auto from_a = std::move(a.link.sent);
    auto from_b = std::move(b.link.sent);
    a.link.sent.clear();
    b.link.sent.clear();
    for (auto &packet : from_a)
      b.link.inject(packet);
    for (auto &packet : from_b)
      a.link.inject(packet);
  }
  FAIL() << "Peers did not go quiet";
}

// Each test replaces App with a fresh one; the keep-alive interval needs its scheduler
class BasicSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    App.~Application();
    new (&App) Application();
    static char name[] = "session-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  }
  void TearDown() override {
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }

  static void set_time(uint32_t now) { LoopBlockingGuard guard{nullptr, nullptr, now}; }
};

TEST_F(BasicSessionTest, MaxPacketSizeAllowsForHeader) {
  Peer a;
  EXPECT_EQ(a.session->get_max_packet_size(), 64 - BasicSession::HEADER_SIZE);
  EXPECT_EQ(a.link.top(), a.session.get());
  EXPECT_EQ(a.session->get_capabilities(), CAPABILITY_SESSION);
}

TEST_F(BasicSessionTest, KeepAliveSentAtSetup) {
  Peer a;
  ASSERT_EQ(a.link.sent.size(), 1);
  EXPECT_TRUE(is_keep_alive(a.link.sent[0]));
  EXPECT_EQ(a.link.sent[0].size(), 5);
  EXPECT_FALSE(a.session->is_connected());
}

TEST_F(BasicSessionTest, BothConnectWithoutRepeatedAnswers) {
  Peer a, b;
  pump(a, b);
  EXPECT_TRUE(a.session->is_connected());
  EXPECT_TRUE(b.session->is_connected());
  EXPECT_EQ(a.states, std::vector<bool>{true});
  EXPECT_EQ(b.states, std::vector<bool>{true});
}

TEST_F(BasicSessionTest, OneSideStartsLater) {
  Peer a;
  a.link.sent.clear();  // a's first keep-alive is lost
  Peer b;
  pump(a, b);
  EXPECT_TRUE(a.session->is_connected());
  EXPECT_TRUE(b.session->is_connected());
}

TEST_F(BasicSessionTest, DataPassesThroughWithHeader) {
  Peer a, b;
  pump(a, b);
  ASSERT_TRUE(a.send({0x10, 0x11}));
  ASSERT_EQ(a.link.sent.size(), 1);
  EXPECT_EQ(a.link.sent[0], (Packet{0x00, 0x10, 0x11}));
  pump(a, b);
  EXPECT_EQ(b.received, (std::vector<Packet>{{0x10, 0x11}}));
  EXPECT_TRUE(a.received.empty());
}

TEST_F(BasicSessionTest, NotUnderstoodPacketsCountedUntilOneIsUnderstood) {
  Peer a;
  for (int i = 0; i != 20; i++)
    a.link.inject({0x7F, 0x00});
  // The count stops where the mismatch is logged, so it is logged once
  EXPECT_EQ(a.session->get_not_understood_count(), 10);
  a.link.inject({0x00, 0x55});
  EXPECT_EQ(a.session->get_not_understood_count(), 0);
}

TEST_F(BasicSessionTest, DataMarksPeerConnected) {
  Peer a;
  a.link.inject({0x00, 0x42});
  EXPECT_TRUE(a.session->is_connected());
  EXPECT_EQ(a.received, (std::vector<Packet>{{0x42}}));
}

TEST_F(BasicSessionTest, DataBeforeFirstKeepAliveIsNotARestart) {
  Peer a, b;
  a.link.sent.clear();
  b.link.sent.clear();  // b's first keep-alive is lost
  b.send({0x42});
  pump(a, b);
  b.session->on_interval_();
  pump(a, b);
  EXPECT_EQ(a.states, std::vector<bool>{true});
}

TEST_F(BasicSessionTest, SendFailureIsReturned) {
  Peer a;
  a.link.accept = false;
  EXPECT_FALSE(a.send({0x01}));
}

TEST_F(BasicSessionTest, IntervalSendsKeepAlive) {
  Peer a;
  a.link.sent.clear();
  a.session->on_interval_();
  ASSERT_EQ(a.link.sent.size(), 1);
  EXPECT_TRUE(is_keep_alive(a.link.sent[0]));
}

TEST_F(BasicSessionTest, PeerLostAfterTimeout) {
  Peer a, b;
  set_time(1000);
  pump(a, b);
  set_time(4000);
  a.session->on_interval_();
  EXPECT_TRUE(a.session->is_connected());
  set_time(4001);
  a.session->on_interval_();
  EXPECT_FALSE(a.session->is_connected());
  EXPECT_EQ(a.states, (std::vector<bool>{true, false}));
  // Heard again
  b.session->on_interval_();
  pump(a, b);
  EXPECT_TRUE(a.session->is_connected());
  EXPECT_EQ(a.states, (std::vector<bool>{true, false, true}));
}

TEST_F(BasicSessionTest, PeerRestartIsReported) {
  Peer a;
  auto b = std::make_unique<Peer>();
  pump(a, *b);
  b = std::make_unique<Peer>();
  pump(a, *b);
  EXPECT_TRUE(a.session->is_connected());
  EXPECT_EQ(a.states, (std::vector<bool>{true, false, true}));
  EXPECT_EQ(b->states, std::vector<bool>{true});
}

TEST_F(BasicSessionTest, MalformedPacketsDropped) {
  Peer a;
  a.link.inject({});
  a.link.inject({0x02, 0x00});
  a.link.inject({0x01, 0x00, 0x00});
  EXPECT_FALSE(a.session->is_connected());
  EXPECT_TRUE(a.received.empty());
}

TEST_F(BasicSessionTest, DataLostBelowReportedAsSessionLost) {
  MockLink link;
  link.set_reliable();
  TestSession session(&link);
  std::vector<bool> states;
  session.add_on_session_state_callback([&states](bool connected) { states.push_back(connected); });
  session.setup();
  // Not yet connected, so nothing to report
  link.report_data_lost();
  EXPECT_TRUE(states.empty());
  link.inject({0x00, 0x55});
  link.report_data_lost();
  EXPECT_FALSE(session.is_connected());
  // Connected again when the peer is next heard
  link.inject({0x00, 0x56});
  EXPECT_EQ(states, (std::vector<bool>{true, false, true}));
}

TEST_F(BasicSessionTest, LinkTooSmallForKeepAliveFailsSetup) {
  MockLink link;
  link.set_max_packet_size(4);
  TestSession session(&link);
  session.setup();
  EXPECT_TRUE(session.is_failed());
  EXPECT_TRUE(link.sent.empty());
  Packet packet{1};
  EXPECT_FALSE(session.send_to_interface(PacketBuffer(packet), {}));
}

}  // namespace esphome::packet_interface::session_testing
