#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "esphome/components/packet_interface/basic_fragmentation.h"
#include "esphome/components/packet_interface/hdlc_reliability.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::fragmentation_testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using Packet = std::vector<uint8_t>;

// A link layer that records what is sent, with a limited send capacity
class MockLink : public PacketInterface {
 public:
  explicit MockLink(size_t max_packet_size = 12) : PacketInterface(max_packet_size) {}
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    this->sent.push_back(to_vector(data));
    return true;
  }
  size_t get_send_capacity() const override { return this->capacity; }
  void inject(const Packet &packet, uint16_t source = 0) {
    PacketMetaData meta_data;
    meta_data.source_address = source;
    this->on_receive_from_interface_(PacketBuffer(packet), meta_data);
  }

  std::vector<Packet> sent;
  size_t capacity{SIZE_MAX};
};

struct Peer {
  explicit Peer(PacketInterface *lower) : fragmentation(lower, 100) {
    this->fragmentation.add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->fragmentation.setup();
  }
  bool send(const Packet &data) { return this->fragmentation.send_to_interface(PacketBuffer(data), {}); }

  BasicFragmentation fragmentation;
  std::vector<Packet> received;
};

static Packet make_packet(size_t size) {
  Packet packet(size);
  for (size_t i = 0; i != size; i++)
    packet[i] = i * 7 + 1;
  return packet;
}

class TestHdlc : public HdlcReliability {
 public:
  using HdlcReliability::HdlcReliability;
  using HdlcReliability::on_timeout_;
};

// Each test replaces App with a fresh one; the reliability layer's timers need its scheduler
class BasicFragmentationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    App.~Application();
    new (&App) Application();
    static char name[] = "fragmentation-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  }
  void TearDown() override {
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }

  // Deliver what a has sent to b
  static void deliver(MockLink &from, MockLink &to) {
    for (auto &packet : from.sent)
      to.inject(packet);
    from.sent.clear();
  }

  MockLink link_a_, link_b_;
};

TEST_F(BasicFragmentationTest, SizesAndCapability) {
  Peer a(&this->link_a_);
  EXPECT_EQ(a.fragmentation.get_max_packet_size(), 100);
  EXPECT_TRUE(a.fragmentation.has_capability(CAPABILITY_FRAGMENTATION));
  EXPECT_EQ(this->link_a_.top(), &a.fragmentation);
}

TEST_F(BasicFragmentationTest, SmallPacketIsOneFragment) {
  Peer a(&this->link_a_), b(&this->link_b_);
  a.send({1, 2, 3});
  ASSERT_EQ(this->link_a_.sent.size(), 1);
  EXPECT_EQ(this->link_a_.sent[0].size(), 3 + BasicFragmentation::HEADER_SIZE);
  EXPECT_EQ(this->link_a_.sent[0][1], 0x80);  // index 0, last
  deliver(this->link_a_, this->link_b_);
  EXPECT_EQ(b.received, (std::vector<Packet>{{1, 2, 3}}));
}

TEST_F(BasicFragmentationTest, LargePacketSplitAndJoined) {
  Peer a(&this->link_a_), b(&this->link_b_);
  for (size_t size : {10, 11, 50, 100}) {
    Packet packet = make_packet(size);
    ASSERT_TRUE(a.send(packet));
    EXPECT_EQ(this->link_a_.sent.size(), (size + 9) / 10);  // 10 bytes of payload in each 12 byte fragment
    EXPECT_EQ(this->link_a_.sent.back()[1] & 0x80, 0x80);
    deliver(this->link_a_, this->link_b_);
    EXPECT_EQ(b.received.back(), packet);
  }
  EXPECT_EQ(b.received.size(), 4);
}

TEST_F(BasicFragmentationTest, CompositePacketSplitAcrossFragments) {
  Peer a(&this->link_a_), b(&this->link_b_);
  Packet pre = make_packet(7), mid(13, 0xAA), post(9, 0x55);
  PacketBuffer body(mid.data(), mid.size());
  ASSERT_TRUE(
      a.fragmentation.send_to_interface(PacketBuffer(pre.data(), pre.size(), &body, post.data(), post.size()), {}));
  deliver(this->link_a_, this->link_b_);
  Packet expected = pre;
  expected.insert(expected.end(), mid.begin(), mid.end());
  expected.insert(expected.end(), post.begin(), post.end());
  EXPECT_EQ(b.received, std::vector<Packet>{expected});
}

TEST_F(BasicFragmentationTest, MissingFragmentDropsOnlyThatPacket) {
  Peer a(&this->link_a_), b(&this->link_b_);
  a.send(make_packet(30));
  this->link_a_.sent.erase(this->link_a_.sent.begin() + 1);
  deliver(this->link_a_, this->link_b_);
  EXPECT_TRUE(b.received.empty());
  a.send(make_packet(25));
  deliver(this->link_a_, this->link_b_);
  EXPECT_EQ(b.received, std::vector<Packet>{make_packet(25)});
}

TEST_F(BasicFragmentationTest, NotStartedWithoutRoomForAllFragments) {
  Peer a(&this->link_a_);
  this->link_a_.capacity = 2;
  EXPECT_FALSE(a.send(make_packet(30)));  // needs 3
  EXPECT_TRUE(this->link_a_.sent.empty());
  EXPECT_TRUE(a.send(make_packet(20)));
  EXPECT_FALSE(a.send(make_packet(101)));
}

TEST_F(BasicFragmentationTest, InterleavedSourcesNotMixed) {
  Peer a(&this->link_a_), c(&this->link_a_), b(&this->link_b_);
  a.send(make_packet(20));
  auto from_a = std::move(this->link_a_.sent);
  this->link_a_.sent.clear();
  c.send(make_packet(20));
  auto from_c = std::move(this->link_a_.sent);
  // Fragments from devices 1 and 2 arrive interleaved, perhaps with the same packet number
  this->link_b_.inject(from_a[0], 1);
  this->link_b_.inject(from_c[0], 2);
  this->link_b_.inject(from_a[1], 1);
  this->link_b_.inject(from_c[1], 2);
  for (auto &packet : b.received)
    EXPECT_EQ(packet, make_packet(20));  // nothing mixed from the two
}

TEST_F(BasicFragmentationTest, OverReliabilityWithLosses) {
  // Fragmentation above HDLC on each side, with every fourth frame from a dropped on the way; not every third, as
  // each resend is a burst of three, so the same fragment would be lost every time
  TestHdlc hdlc_a(&this->link_a_), hdlc_b(&this->link_b_);
  hdlc_a.setup();
  hdlc_b.setup();
  // HDLC adds 3 bytes to the 12 byte link, leaving 7 bytes of payload in each fragment
  Peer a(&hdlc_a), b(&hdlc_b);
  Packet packet = make_packet(20);  // 3 fragments, within the queue of 4
  ASSERT_TRUE(a.send(packet));
  int count = 0;
  for (int i = 0; i != 30; i++) {
    hdlc_a.loop();
    hdlc_b.loop();
    if (i % 5 == 4)
      hdlc_a.on_timeout_();
    for (auto &frame : this->link_a_.sent) {
      if (++count % 4 != 0)
        this->link_b_.inject(frame);
    }
    this->link_a_.sent.clear();
    deliver(this->link_b_, this->link_a_);
  }
  EXPECT_EQ(b.received, std::vector<Packet>{packet});
}

TEST_F(BasicFragmentationTest, OversizedSingleFragmentDropped) {
  // A link packet holds more than this layer's max_packet_size
  BasicFragmentation fragmentation(&this->link_a_, 4);
  std::vector<Packet> received;
  fragmentation.add_packet_interface_listener(
      [&received](const PacketBuffer &data, const PacketMetaData &) { received.push_back(to_vector(data)); });
  fragmentation.setup();
  this->link_a_.inject({0x01, 0x80, 1, 2, 3, 4, 5});
  EXPECT_TRUE(received.empty());
  this->link_a_.inject({0x02, 0x80, 1, 2, 3, 4});
  EXPECT_EQ(received, std::vector<Packet>{Packet({1, 2, 3, 4})});
}

TEST_F(BasicFragmentationTest, LinkTooSmallFailsSetup) {
  MockLink link(BasicFragmentation::HEADER_SIZE);
  BasicFragmentation fragmentation(&link, 100);
  fragmentation.setup();
  EXPECT_TRUE(fragmentation.is_failed());
  Packet packet{1};
  EXPECT_FALSE(fragmentation.send_to_interface(PacketBuffer(packet), {}));
}

}  // namespace esphome::packet_interface::fragmentation_testing
