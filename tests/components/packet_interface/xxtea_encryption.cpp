#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include "esphome/components/packet_interface/xxtea_encryption.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::encryption_testing {

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
  explicit MockLink(size_t max_packet_size = 64) : PacketInterface(max_packet_size) {}
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    this->sent.push_back(to_vector(data));
    return true;
  }
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }

  std::vector<Packet> sent;
};

static constexpr size_t ROLLING_TRAILER_SIZE = 16;
static constexpr size_t BROADCAST_TRAILER_SIZE = 12;
static constexpr size_t TRAILER_SIZE = 4;
static constexpr RollingCode NONE = RollingCode::ROLLING_CODE_NONE;
static constexpr RollingCode ECHO = RollingCode::ROLLING_CODE_ECHO;
static constexpr RollingCode BROADCAST = RollingCode::ROLLING_CODE_BROADCAST;

class TestEncryption : public XxteaEncryption {
 public:
  using XxteaEncryption::XxteaEncryption;
  uint32_t get_boot() const { return this->boot_; }
  uint32_t get_saved_peer_boot() const { return this->saved_peer_boot_; }
  void set_counter(uint32_t counter) { this->counter_ = counter; }
};

struct Peer {
  // Saved values are kept per name, so a restart is a new Peer with the same name
  explicit Peer(uint32_t key_word = 0x12345678, size_t max_packet_size = 64, RollingCode rolling_code = ECHO,
                const char *name = "test_link", bool save_peer_boot = false)
      : link(max_packet_size) {
    this->link.set_name(name);
    this->encryption = std::make_unique<TestEncryption>(&this->link, rolling_code);
    this->encryption->set_key({key_word, 2, 3, 4, 5, 6, 7, 8});
    this->encryption->set_save_peer_boot(save_peer_boot);
    this->encryption->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->encryption->setup();
  }
  bool send(const Packet &data) { return this->encryption->send_to_interface(PacketBuffer(data), {}); }
  // Deliver everything sent so far to the peer
  void deliver_to(Peer &peer) {
    for (auto &packet : this->link.sent)
      peer.link.inject(packet);
    this->link.sent.clear();
  }
  // Exchange a packet each way so that each peer echoes the other's boot number
  void introduce(Peer &peer) {
    this->send({});
    this->deliver_to(peer);
    peer.send({});
    peer.deliver_to(*this);
    this->send({});
    this->deliver_to(peer);
    this->received.clear();
    peer.received.clear();
  }

  MockLink link;
  std::unique_ptr<TestEncryption> encryption;
  std::vector<Packet> received;
};

// Each test replaces App with a fresh one
class XxteaEncryptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    App.~Application();
    new (&App) Application();
    static char name[] = "xxtea-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  }
  void TearDown() override {
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }
};

TEST_F(XxteaEncryptionTest, MaxPacketSizeAllowsForTrailerAndWords) {
  Peer a;
  EXPECT_EQ(a.encryption->get_max_packet_size(), 64 - ROLLING_TRAILER_SIZE);
  EXPECT_EQ(a.link.top(), a.encryption.get());
  EXPECT_TRUE(a.encryption->has_capability(CAPABILITY_ENCRYPTION));
  EXPECT_FALSE(a.encryption->has_capability(CAPABILITY_SESSION));
  Peer b(0x12345678, 63);
  EXPECT_EQ(b.encryption->get_max_packet_size(), 60 - ROLLING_TRAILER_SIZE);
  Peer c(0x12345678, 64, NONE);
  EXPECT_EQ(c.encryption->get_max_packet_size(), 64 - TRAILER_SIZE);
  Peer d(0x12345678, 64, BROADCAST);
  EXPECT_EQ(d.encryption->get_max_packet_size(), 64 - BROADCAST_TRAILER_SIZE);
}

static void round_trip_all_sizes(RollingCode rolling_code, size_t trailer_size) {
  Peer a(0x12345678, 64, rolling_code), b(0x12345678, 64, rolling_code);
  a.introduce(b);
  std::vector<Packet> expected;
  for (size_t size = 0; size <= a.encryption->get_max_packet_size(); size++) {
    Packet packet(size);
    for (size_t i = 0; i != size; i++)
      packet[i] = static_cast<uint8_t>(size + i);
    expected.push_back(packet);
    ASSERT_TRUE(a.send(packet));
    ASSERT_EQ(a.link.sent.back().size() % 4, 0);
    ASSERT_GE(a.link.sent.back().size(), std::max<size_t>(8, size + trailer_size));
  }
  a.deliver_to(b);
  EXPECT_EQ(b.received, expected);
}

TEST_F(XxteaEncryptionTest, RoundTripAllSizes) { round_trip_all_sizes(ECHO, ROLLING_TRAILER_SIZE); }

TEST_F(XxteaEncryptionTest, RoundTripAllSizesWithoutRollingCode) { round_trip_all_sizes(NONE, TRAILER_SIZE); }

TEST_F(XxteaEncryptionTest, RoundTripAllSizesBroadcast) { round_trip_all_sizes(BROADCAST, BROADCAST_TRAILER_SIZE); }

TEST_F(XxteaEncryptionTest, WithoutRollingCodeReplayAccepted) {
  Peer a(0x12345678, 64, NONE), b(0x12345678, 64, NONE);
  a.send({1});
  b.link.inject(a.link.sent[0]);
  b.link.inject(a.link.sent[0]);
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {1}}));
}

TEST_F(XxteaEncryptionTest, RollingCodeSettingMustMatch) {
  for (auto [mode_a, mode_b] : {std::pair{ECHO, NONE}, std::pair{ECHO, BROADCAST}, std::pair{BROADCAST, NONE}}) {
    Peer a(0x12345678, 64, mode_a), b(0x12345678, 64, mode_b);
    // Several rounds, so that echo mode would have learnt the peer's boot number
    for (int i = 0; i != 3; i++) {
      a.send({1, 2, 3, 4, 5, 6, 7, 8});
      b.send({1, 2, 3, 4, 5, 6, 7, 8});
      a.deliver_to(b);
      b.deliver_to(a);
    }
    EXPECT_TRUE(a.received.empty());
    EXPECT_TRUE(b.received.empty());
  }
}

TEST_F(XxteaEncryptionTest, SamePacketEncryptsDifferently) {
  Peer a;
  a.send({1, 2, 3, 4});
  a.send({1, 2, 3, 4});
  ASSERT_EQ(a.link.sent.size(), 2);
  EXPECT_NE(a.link.sent[0], a.link.sent[1]);
}

TEST_F(XxteaEncryptionTest, OversizedPacketRefused) {
  Peer a;
  EXPECT_FALSE(a.send(Packet(a.encryption->get_max_packet_size() + 1)));
  EXPECT_TRUE(a.link.sent.empty());
}

TEST_F(XxteaEncryptionTest, WrongKeyDropped) {
  Peer a, b(0x87654321);
  a.send({1, 2, 3});
  a.deliver_to(b);
  EXPECT_TRUE(b.received.empty());
}

TEST_F(XxteaEncryptionTest, ChangedPacketDropped) {
  Peer a, b;
  a.send({1, 2, 3});
  a.link.sent[0][2] ^= 0x01;
  a.deliver_to(b);
  EXPECT_TRUE(b.received.empty());
}

TEST_F(XxteaEncryptionTest, BadLengthDropped) {
  Peer a, b;
  a.send({1, 2, 3});
  Packet packet = a.link.sent[0];
  b.link.inject(Packet(packet.begin(), packet.end() - 1));
  b.link.inject(Packet(8));
  EXPECT_TRUE(b.received.empty());
}

TEST_F(XxteaEncryptionTest, ReplayDropped) {
  Peer a, b;
  a.introduce(b);
  a.send({1});
  Packet first = a.link.sent[0];
  a.send({2});
  a.deliver_to(b);
  b.link.inject(first);
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {2}}));
}

TEST_F(XxteaEncryptionTest, OlderPacketDropped) {
  Peer a, b;
  a.introduce(b);
  a.send({1});
  a.send({2});
  b.link.inject(a.link.sent[1]);
  b.link.inject(a.link.sent[0]);
  EXPECT_EQ(b.received, (std::vector<Packet>{{2}}));
}

TEST_F(XxteaEncryptionTest, BootNumberIncreasesAndPeerRestartAccepted) {
  Peer b;
  auto a = std::make_unique<Peer>();
  uint32_t boot = a->encryption->get_boot();
  a->introduce(b);
  a->send({1});
  a->send({2});
  a->deliver_to(b);
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {2}}));
  a = std::make_unique<Peer>();  // the peer restarts, and its counter starts again from 0
  EXPECT_EQ(a->encryption->get_boot(), boot + 1);
  a->introduce(b);
  a->send({3});
  a->deliver_to(b);
  EXPECT_EQ(b.received, std::vector<Packet>{{3}});
}

TEST_F(XxteaEncryptionTest, FirstPacketsDroppedUntilEchoed) {
  Peer a, b;
  a.send({1});
  a.deliver_to(b);
  EXPECT_TRUE(b.received.empty());  // a does not know b's boot number yet
  b.send({2});
  b.deliver_to(a);
  EXPECT_EQ(a.received, std::vector<Packet>{{2}});  // b learnt a's from the dropped packet
  a.send({3});
  a.deliver_to(b);
  EXPECT_EQ(b.received, std::vector<Packet>{{3}});
}

TEST_F(XxteaEncryptionTest, PacketsFromBeforeRestartDropped) {
  Peer a;
  auto b = std::make_unique<Peer>();
  a.introduce(*b);
  a.send({1});
  a.send({2});
  std::vector<Packet> recorded = a.link.sent;
  a.link.sent.clear();
  b = std::make_unique<Peer>();  // b restarts and forgets a's counter
  for (auto &packet : recorded)
    b->link.inject(packet);
  EXPECT_TRUE(b->received.empty());
  a.introduce(*b);
  a.send({3});
  a.deliver_to(*b);
  EXPECT_EQ(b->received, std::vector<Packet>{{3}});
}

TEST_F(XxteaEncryptionTest, LinkTooSmallFailsSetup) {
  MockLink link(ROLLING_TRAILER_SIZE + 3);
  TestEncryption encryption(&link, ECHO);
  EXPECT_EQ(encryption.get_max_packet_size(), 0);
  encryption.setup();
  EXPECT_TRUE(encryption.is_failed());
  Packet packet{1};
  EXPECT_FALSE(encryption.send_to_interface(PacketBuffer(packet), {}));
}

TEST_F(XxteaEncryptionTest, SendWithoutBuffersRefused) {
  MockLink link(64);
  TestEncryption encryption(&link, NONE);
  Packet packet{1};
  EXPECT_FALSE(encryption.send_to_interface(PacketBuffer(packet), {}));
  EXPECT_TRUE(link.sent.empty());
}

TEST_F(XxteaEncryptionTest, CounterWrapStartsNewBoot) {
  Peer a, b(0x12345678);
  a.introduce(b);
  uint32_t boot = a.encryption->get_boot();
  a.encryption->set_counter(UINT32_MAX);
  ASSERT_TRUE(a.send({1}));
  EXPECT_EQ(a.encryption->get_boot(), boot + 1);
  ASSERT_TRUE(a.send({2}));
  a.deliver_to(b);
  // Both were accepted: the last packet of the old boot, then the first of the new one
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {2}}));
}

TEST_F(XxteaEncryptionTest, BroadcastAcceptedWithoutEcho) {
  Peer a(0x12345678, 64, BROADCAST, "bc_sender");
  Peer b(0x12345678, 64, BROADCAST, "bc_receiver1"), c(0x12345678, 64, BROADCAST, "bc_receiver2");
  a.send({1});
  a.send({2});
  for (auto &packet : a.link.sent) {
    b.link.inject(packet);
    c.link.inject(packet);
  }
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {2}}));
  EXPECT_EQ(c.received, (std::vector<Packet>{{1}, {2}}));
}

TEST_F(XxteaEncryptionTest, BroadcastReplayAndOlderDropped) {
  Peer a(0x12345678, 64, BROADCAST, "bc_replay_sender"), b(0x12345678, 64, BROADCAST, "bc_replay_receiver");
  a.send({1});
  a.send({2});
  a.send({3});
  b.link.inject(a.link.sent[0]);
  b.link.inject(a.link.sent[2]);
  b.link.inject(a.link.sent[2]);
  b.link.inject(a.link.sent[1]);
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {3}}));
}

TEST_F(XxteaEncryptionTest, BroadcastWithoutSaveAcceptsAnythingAfterRestart) {
  auto a = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_nosave_sender");
  auto b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_nosave_receiver");
  a->send({1});
  Packet old = a->link.sent[0];
  a = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_nosave_sender");  // newer boot of the sender
  a->send({2});
  a->deliver_to(*b);
  EXPECT_EQ(b->received, std::vector<Packet>{{2}});
  b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_nosave_receiver");
  b->link.inject(old);  // a packet from an older boot is accepted after the receiver restarts
  EXPECT_EQ(b->received, std::vector<Packet>{{1}});
  EXPECT_EQ(b->encryption->get_saved_peer_boot(), 0);
}

TEST_F(XxteaEncryptionTest, BroadcastSavedBootSurvivesRestart) {
  auto a = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_save_sender");
  auto b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_save_receiver", true);
  a->send({1});
  Packet old_boot = a->link.sent[0];
  a->link.sent.clear();
  a = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_save_sender");
  uint32_t boot = a->encryption->get_boot();
  a->send({2});
  a->send({3});
  Packet current_boot = a->link.sent[0];
  a->deliver_to(*b);
  EXPECT_EQ(b->received, (std::vector<Packet>{{2}, {3}}));
  EXPECT_EQ(b->encryption->get_saved_peer_boot(), boot);
  b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_save_receiver", true);
  EXPECT_EQ(b->encryption->get_saved_peer_boot(), boot);
  b->link.inject(old_boot);
  EXPECT_TRUE(b->received.empty());
  // A packet from the sender's current boot is accepted, after which older counters are dropped again
  b->link.inject(current_boot);
  b->link.inject(current_boot);
  EXPECT_EQ(b->received, std::vector<Packet>{{2}});
}

TEST_F(XxteaEncryptionTest, ResetAcceptsSenderWithLostBootNumber) {
  auto b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_reset_receiver", true);
  auto a = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_reset_sender");
  a->encryption->set_counter(UINT32_MAX);  // the next send moves the sender on to a new boot
  a->send({1});
  a->send({2});
  a->deliver_to(*b);
  ASSERT_EQ(b->received, (std::vector<Packet>{{1}, {2}}));
  uint32_t boot = a->encryption->get_boot();
  ASSERT_EQ(b->encryption->get_saved_peer_boot(), boot);
  b->received.clear();
  // The sender's flash is erased: same name, but its boot number starts again from 1
  Peer erased(0x12345678, 64, BROADCAST, "bc_reset_other_sender");
  ASSERT_LT(erased.encryption->get_boot(), boot);
  erased.send({3});
  Packet packet = erased.link.sent[0];
  b->link.inject(packet);
  EXPECT_TRUE(b->received.empty());
  b->encryption->reset_peer();
  EXPECT_EQ(b->encryption->get_saved_peer_boot(), 0);
  b->link.inject(packet);
  EXPECT_EQ(b->received, std::vector<Packet>{{3}});
  // The reset was saved too
  b = std::make_unique<Peer>(0x12345678, 64, BROADCAST, "bc_reset_receiver", true);
  EXPECT_EQ(b->encryption->get_saved_peer_boot(), erased.encryption->get_boot());
}

TEST_F(XxteaEncryptionTest, ResetInEchoMode) {
  Peer a, b;
  a.introduce(b);
  a.send({1});
  Packet first = a.link.sent[0];
  a.deliver_to(b);
  b.encryption->reset_peer();
  b.link.inject(first);  // forgotten, so the same packet is accepted again
  EXPECT_EQ(b.received, (std::vector<Packet>{{1}, {1}}));
}

}  // namespace esphome::packet_interface::encryption_testing
