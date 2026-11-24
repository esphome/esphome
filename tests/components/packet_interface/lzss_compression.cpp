#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "esphome/components/packet_interface/lzss_compression.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::compression_testing {

using Packet = std::vector<uint8_t>;

static Packet to_vector(const PacketBuffer &data) {
  Packet result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

static Packet text(const std::string &value) { return {value.begin(), value.end()}; }

class MockLink : public PacketInterface {
 public:
  explicit MockLink(size_t max_packet_size) : PacketInterface(max_packet_size) {}
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    // Before the packet is read, as a log line in a real layer would run it
    if (this->on_send)
      this->on_send();
    this->sent.push_back(to_vector(data));
    return true;
  }
  size_t get_send_capacity() const override { return this->capacity; }
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }
  void inject(const PacketBuffer &packet) { this->on_receive_from_interface_(packet, {}); }

  std::vector<Packet> sent;
  std::function<void()> on_send;
  size_t capacity{SIZE_MAX};
};

static const std::string JSON_DICTIONARY =
    R"({"name":"","value":,"state":"ON","OFF","unit":"","id":"","type":"temperature","humidity",)"
    R"("time":"","status":"ok","error":null,"enabled":true,"false},{"":[{""}]})";

static const std::string WEATHER =
    R"({"station":"backyard","temperature":{"value":18.2,"min":12.1,"max":23.9},"humidity":{"value":66,"min":51,)"
    R"("max":90},"wind":{"speed":3.4,"gust":7.9,"direction":"SSW"},"rain":{"rate":0.0,"today":2.4},"uv":3})";

struct Peer {
  Peer(const std::string &dictionary, size_t link_size = 255, size_t max_packet_size = 0, uint8_t check = 0x2A)
      : link(link_size),
        dictionary(dictionary.begin(), dictionary.end()),
        buffers(LzssCompression::buffer_size(link_size, max_packet_size)),
        chains(LzssCompression::CHAIN_WORDS) {
    this->link.set_name("test_link");
    this->compression =
        std::make_unique<LzssCompression>(&this->link, max_packet_size, this->dictionary.data(),
                                          this->dictionary.size(), check, this->buffers.data(), this->chains.data());
    this->compression->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->compression->setup();
  }
  bool send(const Packet &data) { return this->compression->send_to_interface(PacketBuffer(data), {}); }
  void deliver_to(Peer &peer) {
    for (auto &packet : this->link.sent)
      peer.link.inject(packet);
    this->link.sent.clear();
  }

  MockLink link;
  Packet dictionary;
  std::vector<uint8_t> buffers;
  std::vector<uint16_t> chains;
  std::unique_ptr<LzssCompression> compression;
  std::vector<Packet> received;
};

class LzssCompressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    App.~Application();
    new (&App) Application();
    static char name[] = "lzss-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  }
  void TearDown() override {
    App.~Application();
    new (&App) Application();
  }
};

TEST_F(LzssCompressionTest, ReportsTheLinksSendCapacity) {
  Peer peer("");
  peer.link.capacity = 3;
  EXPECT_EQ(peer.compression->get_send_capacity(), 3u);
}

TEST_F(LzssCompressionTest, MaxPacketSizeAllowsForHeader) {
  Peer a(JSON_DICTIONARY, 64);
  EXPECT_EQ(a.compression->get_max_packet_size(), 63u);
  EXPECT_TRUE(a.compression->has_capability(CAPABILITY_COMPRESSION));
  Peer b(JSON_DICTIONARY, 64, 400);
  EXPECT_EQ(b.compression->get_max_packet_size(), 400u);
}

TEST_F(LzssCompressionTest, JsonRoundTripIsSmaller) {
  for (const auto *dictionary : {&JSON_DICTIONARY, &WEATHER}) {
    Peer a(*dictionary);
    Peer b(*dictionary);
    // A reading that differs from the dictionary in its values
    std::string reading = WEATHER;
    std::replace(reading.begin(), reading.end(), '2', '5');
    ASSERT_TRUE(a.send(text(reading)));
    ASSERT_EQ(a.link.sent.size(), 1u);
    const Packet &sent = a.link.sent[0];
    EXPECT_EQ(sent[0], LzssCompression::HEADER_COMPRESSED | 0x2A);
    EXPECT_LT(sent.size(), reading.size());
    std::printf("Dictionary of %zu bytes: %zu bytes of JSON sent as %zu\n", dictionary->size(), reading.size(),
                sent.size());
    a.deliver_to(b);
    ASSERT_EQ(b.received.size(), 1u);
    EXPECT_EQ(b.received[0], text(reading));
  }
}

TEST_F(LzssCompressionTest, WithoutDictionary) {
  Peer a("");
  Peer b("");
  Packet repeated = text(R"([{"a":1},{"a":1},{"a":1},{"a":1},{"a":1},{"a":1}])");
  ASSERT_TRUE(a.send(repeated));
  EXPECT_LT(a.link.sent[0].size(), repeated.size());
  a.deliver_to(b);
  ASSERT_EQ(b.received.size(), 1u);
  EXPECT_EQ(b.received[0], repeated);
}

TEST_F(LzssCompressionTest, IncompressibleIsSentAsItIs) {
  Peer a(JSON_DICTIONARY);
  Peer b(JSON_DICTIONARY);
  Packet noise(100);
  std::mt19937 random(1);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (auto &byte : noise)
    byte = random();
  for (const Packet &packet : {noise, Packet{}, Packet{0x41}}) {
    ASSERT_TRUE(a.send(packet));
    ASSERT_EQ(a.link.sent.size(), 1u);
    EXPECT_EQ(a.link.sent[0].size(), packet.size() + 1);
    EXPECT_EQ(a.link.sent[0][0], 0x2A);
    a.deliver_to(b);
    ASSERT_EQ(b.received.size(), 1u);
    EXPECT_EQ(b.received[0], packet);
    b.received.clear();
  }
}

TEST_F(LzssCompressionTest, NonContiguousPacket) {
  Peer a(JSON_DICTIONARY);
  Peer b(JSON_DICTIONARY);
  Packet head = text(R"({"name":"porch",)");
  Packet tail = text(R"("state":"ON","enabled":true})");
  ASSERT_TRUE(a.compression->send_to_interface(PacketBuffer(head.data(), head.size(), tail.data(), tail.size()), {}));
  a.deliver_to(b);
  ASSERT_EQ(b.received.size(), 1u);
  Packet whole = head;
  whole.insert(whole.end(), tail.begin(), tail.end());
  EXPECT_EQ(b.received[0], whole);
}

TEST_F(LzssCompressionTest, SplitPacketDropped) {
  Peer a(JSON_DICTIONARY);
  Packet head{0x2A, 'a'};
  Packet tail{'b'};
  a.link.inject(PacketBuffer(head.data(), head.size(), tail.data(), tail.size()));
  EXPECT_TRUE(a.received.empty());
  a.link.inject(Packet{0x2A, 'a', 'b'});
  EXPECT_EQ(a.received, std::vector<Packet>{text("ab")});
}

TEST_F(LzssCompressionTest, BadHeadersAndSizesDropped) {
  Peer a(JSON_DICTIONARY, 255, 32);
  a.link.inject(Packet{});  // no header
  Packet oversized(34, 'a');
  oversized[0] = 0x2A;  // sent as it is, with 33 bytes for a 32 byte limit
  a.link.inject(oversized);
  EXPECT_TRUE(a.received.empty());
  oversized.pop_back();
  a.link.inject(oversized);
  EXPECT_EQ(a.received.size(), 1u);
  EXPECT_FALSE(a.send(Packet(33, 'b')));
  EXPECT_TRUE(a.link.sent.empty());
}

TEST_F(LzssCompressionTest, LargerThanLinkOnlyIfItCompresses) {
  Peer a(WEATHER, 64, 400);
  Peer b(WEATHER, 64, 400);
  ASSERT_TRUE(a.send(text(WEATHER)));
  EXPECT_LE(a.link.sent[0].size(), 64u);
  a.deliver_to(b);
  ASSERT_EQ(b.received.size(), 1u);
  EXPECT_EQ(b.received[0], text(WEATHER));
  Packet noise(100);
  std::mt19937 random(2);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (auto &byte : noise)
    byte = random();
  EXPECT_FALSE(a.send(noise));
  EXPECT_TRUE(a.link.sent.empty());
}

TEST_F(LzssCompressionTest, OtherDictionaryDropped) {
  Peer a(JSON_DICTIONARY, 255, 0, 0x2A);
  Peer b(WEATHER, 255, 0, 0x15);
  ASSERT_TRUE(a.send(text(R"({"state":"ON"})")));
  a.deliver_to(b);
  EXPECT_TRUE(b.received.empty());
}

// A send made while one is still going through the layer below would overwrite the buffer it is reading
TEST_F(LzssCompressionTest, NestedSendRefused) {
  Peer a(JSON_DICTIONARY);
  Peer b(JSON_DICTIONARY);
  bool nested = true;
  int calls = 0;
  a.link.on_send = [&]() {
    if (calls++ == 0)
      nested = a.send(text(R"({"state":"OFF"})"));
  };
  ASSERT_TRUE(a.send(text(WEATHER)));
  EXPECT_FALSE(nested);
  a.link.on_send = nullptr;
  a.deliver_to(b);
  ASSERT_EQ(b.received.size(), 1u);
  EXPECT_EQ(b.received[0], text(WEATHER));
  // Sending works again afterwards
  ASSERT_TRUE(a.send(text(R"({"state":"ON"})")));
  a.deliver_to(b);
  EXPECT_EQ(b.received.back(), text(R"({"state":"ON"})"));
}

// Any input is either expanded within max_packet_size or dropped
TEST_F(LzssCompressionTest, MalformedPacketsDropped) {
  Peer a(JSON_DICTIONARY, 255, 32);
  std::mt19937 random(3);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (int round = 0; round < 20000; round++) {
    Packet packet(1 + random() % 40);
    for (auto &byte : packet)
      byte = random();
    packet[0] = LzssCompression::HEADER_COMPRESSED | 0x2A;
    a.link.inject(packet);
    ASSERT_TRUE(a.received.empty() || a.received.back().size() <= 32);
  }
}

// With PSRAM, the buffers are allocated at boot and can be missing
TEST_F(LzssCompressionTest, MissingBuffersFailSetup) {
  MockLink link(255);
  uint16_t chains[LzssCompression::CHAIN_WORDS];
  LzssCompression compression(&link, 0, nullptr, 0, 0, nullptr, chains);
  compression.setup();
  EXPECT_TRUE(compression.is_failed());
  Packet packet{1};
  EXPECT_FALSE(compression.send_to_interface(PacketBuffer(packet), {}));
}

}  // namespace esphome::packet_interface::compression_testing
