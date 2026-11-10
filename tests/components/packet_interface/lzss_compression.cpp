#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }

  std::vector<Packet> sent;
  std::function<void()> on_send;
};

static const std::string JSON_DICTIONARY =
    R"({"name":"","value":,"state":"ON","OFF","unit":"","id":"","type":"temperature","humidity",)"
    R"("time":"","status":"ok","error":null,"enabled":true,"false},{"":[{""}]})";

static const std::string WEATHER =
    R"({"station":"backyard","temperature":{"value":18.2,"min":12.1,"max":23.9},"humidity":{"value":66,"min":51,)"
    R"("max":90},"wind":{"speed":3.4,"gust":7.9,"direction":"SSW"},"rain":{"rate":0.0,"today":2.4},"uv":3})";

struct Peer {
  Peer(const std::string &dictionary, size_t link_size = 255, size_t max_packet_size = 0, uint8_t check = 0x2A)
      : link(link_size), dictionary(dictionary.begin(), dictionary.end()) {
    this->link.set_name("test_link");
    this->compression = std::make_unique<LzssCompression>(&this->link, max_packet_size, this->dictionary.data(),
                                                          this->dictionary.size(), check);
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

TEST_F(LzssCompressionTest, LongMatchesAndOverlap) {
  Peer a("");
  Peer b("");
  Packet run(240, 'x');
  ASSERT_TRUE(a.send(run));
  // A literal and one overlapping match
  EXPECT_LE(a.link.sent[0].size(), 5u);
  a.deliver_to(b);
  ASSERT_EQ(b.received.size(), 1u);
  EXPECT_EQ(b.received[0], run);
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

// Any input either expands into the buffer or is dropped; nothing is written past it
TEST_F(LzssCompressionTest, MalformedPacketsNeverOverrun) {
  Peer a(JSON_DICTIONARY, 255, 32);
  std::mt19937 random(3);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  Packet guard(64, 0xA5);
  for (int round = 0; round < 20000; round++) {
    Packet packet(1 + random() % 40);
    for (auto &byte : packet)
      byte = random();
    packet[0] = LzssCompression::HEADER_COMPRESSED | 0x2A;
    Packet out(32 + guard.size());
    std::memcpy(out.data() + 32, guard.data(), guard.size());
    PacketBuffer buffer(packet);
    size_t length = a.compression->expand(buffer, 1, packet.size(), out.data(), 32);
    ASSERT_TRUE(length == SIZE_MAX || length <= 32);
    ASSERT_EQ(std::memcmp(out.data() + 32, guard.data(), guard.size()), 0);
    a.link.inject(packet);
    ASSERT_TRUE(a.received.empty() || a.received.back().size() <= 32);
  }
}

TEST_F(LzssCompressionTest, RandomRoundTrips) {
  std::mt19937 random(4);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (const auto *dictionary : {&JSON_DICTIONARY, &WEATHER}) {
    Peer a(*dictionary, 512);
    Peer b(*dictionary, 512);
    for (int round = 0; round < 500; round++) {
      // Mostly pieces of JSON, so there are matches of every length and distance
      Packet packet;
      size_t size = random() % 500;
      while (packet.size() < size) {
        if (random() % 3 == 0) {
          packet.push_back(random());
        } else {
          size_t start = random() % WEATHER.size();
          size_t count = std::min<size_t>(1 + random() % 20, WEATHER.size() - start);
          packet.insert(packet.end(), WEATHER.begin() + start, WEATHER.begin() + start + count);
        }
      }
      packet.resize(size);
      ASSERT_TRUE(a.send(packet));
      a.deliver_to(b);
      ASSERT_EQ(b.received.size(), 1u);
      ASSERT_EQ(b.received[0], packet);
      b.received.clear();
    }
  }
}

TEST_F(LzssCompressionTest, Speed) {
  Peer a(JSON_DICTIONARY, 2048, 1024);
  Peer b(JSON_DICTIONARY, 2048, 1024);
  std::string json;
  while (json.size() + WEATHER.size() < 1000)
    json += WEATHER;
  Packet packet = text(json);
  constexpr int rounds = 200;
  auto start = std::chrono::steady_clock::now();
  for (int round = 0; round < rounds; round++) {
    a.send(packet);
    a.link.sent.clear();
  }
  auto middle = std::chrono::steady_clock::now();
  a.send(packet);
  Packet sent = a.link.sent[0];
  for (int round = 0; round < rounds; round++)
    b.link.inject(sent);
  auto end = std::chrono::steady_clock::now();
  ASSERT_EQ(b.received.back(), packet);
  auto us = [](auto d) { return std::chrono::duration_cast<std::chrono::microseconds>(d).count() / rounds; };
  std::printf("%zu bytes compressed to %zu: %lld us to compress, %lld us to expand, on this host\n", packet.size(),
              sent.size(), static_cast<long long>(us(middle - start)), static_cast<long long>(us(end - middle)));
  // The slowest case: every position is searched to the end of the window
  Packet noise(1000);
  std::mt19937 random(5);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (auto &byte : noise)
    byte = random();
  start = std::chrono::steady_clock::now();
  for (int round = 0; round < rounds; round++) {
    a.send(noise);
    a.link.sent.clear();
  }
  end = std::chrono::steady_clock::now();
  std::printf("%zu bytes of noise: %lld us to find that it does not compress, on this host\n", noise.size(),
              static_cast<long long>(us(end - start)));
}

}  // namespace esphome::packet_interface::compression_testing
