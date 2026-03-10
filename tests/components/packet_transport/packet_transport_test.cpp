#include "common.h"

namespace esphome::packet_transport::testing {

// --- Configuration setter tests ---

TEST(PacketTransportTest, SetIsProvider) {
  TestablePacketTransport transport;
  transport.set_is_provider(true);
  EXPECT_TRUE(transport.is_provider_);
}

TEST(PacketTransportTest, SetEncryptionKey) {
  TestablePacketTransport transport;
  std::vector<uint8_t> key(32, 0xAB);
  transport.set_encryption_key(key);
  EXPECT_EQ(transport.encryption_key_, key);
  EXPECT_TRUE(transport.is_encrypted_());
}

TEST(PacketTransportTest, NoEncryptionByDefault) {
  TestablePacketTransport transport;
  EXPECT_FALSE(transport.is_encrypted_());
}

TEST(PacketTransportTest, SetRollingCodeEnable) {
  TestablePacketTransport transport;
  transport.set_rolling_code_enable(true);
  EXPECT_TRUE(transport.rolling_code_enable_);
}

TEST(PacketTransportTest, SetPingPongEnable) {
  TestablePacketTransport transport;
  transport.set_ping_pong_enable(true);
  EXPECT_TRUE(transport.ping_pong_enable_);
}

TEST(PacketTransportTest, SetPingPongRecycleTime) {
  TestablePacketTransport transport;
  transport.set_ping_pong_recycle_time(600);
  EXPECT_EQ(transport.ping_pong_recyle_time_, 600u);
}

// --- Provider management ---

TEST(PacketTransportTest, AddProvider) {
  TestablePacketTransport transport;
  transport.add_provider("host1");
  EXPECT_TRUE(transport.providers_.contains("host1"));
  EXPECT_EQ(transport.providers_.size(), 1u);
}

TEST(PacketTransportTest, AddProviderDuplicate) {
  TestablePacketTransport transport;
  transport.add_provider("host1");
  transport.add_provider("host1");
  EXPECT_EQ(transport.providers_.size(), 1u);
}

TEST(PacketTransportTest, SetProviderEncryption) {
  TestablePacketTransport transport;
  transport.add_provider("host1");
  std::vector<uint8_t> key(32, 0xCD);
  transport.set_provider_encryption("host1", key);
  EXPECT_EQ(transport.providers_["host1"].encryption_key, key);
}

// --- Ping key tests ---

TEST(PacketTransportTest, PingKeyStoredWhenEncrypted) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  transport.set_encryption_key(std::vector<uint8_t>(32, 0xAA));

  auto ping = build_ping_packet("requester", 0xDEADBEEF);
  transport.process_({ping.data(), ping.size()});

  ASSERT_EQ(transport.ping_keys_.size(), 1u);
  EXPECT_EQ(transport.ping_keys_["requester"], 0xDEADBEEFu);
}

TEST(PacketTransportTest, PingKeyIgnoredWhenNotEncrypted) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  // No encryption key — add_key_ should be a no-op

  auto ping = build_ping_packet("requester", 0xDEADBEEF);
  transport.process_({ping.data(), ping.size()});

  EXPECT_TRUE(transport.ping_keys_.empty());
}

TEST(PacketTransportTest, PingKeyUpdatedOnRepeat) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  transport.set_encryption_key(std::vector<uint8_t>(32, 0xAA));

  auto ping1 = build_ping_packet("host1", 0x1111);
  transport.process_({ping1.data(), ping1.size()});
  EXPECT_EQ(transport.ping_keys_["host1"], 0x1111u);

  // Same host, new key value — should update in place
  auto ping2 = build_ping_packet("host1", 0x2222);
  transport.process_({ping2.data(), ping2.size()});
  EXPECT_EQ(transport.ping_keys_.size(), 1u);
  EXPECT_EQ(transport.ping_keys_["host1"], 0x2222u);
}

TEST(PacketTransportTest, PingKeyMaxLimit) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  transport.set_encryption_key(std::vector<uint8_t>(32, 0xAA));

  // Fill to MAX_PING_KEYS (4)
  for (int i = 0; i < 4; i++) {
    char name[16];
    snprintf(name, sizeof(name), "host%d", i);
    auto ping = build_ping_packet(name, 0x1000 + i);
    transport.process_({ping.data(), ping.size()});
  }
  EXPECT_EQ(transport.ping_keys_.size(), 4u);

  // 5th key should be discarded
  auto ping = build_ping_packet("host4", 0x9999);
  transport.process_({ping.data(), ping.size()});
  EXPECT_EQ(transport.ping_keys_.size(), 4u);
  EXPECT_FALSE(transport.ping_keys_.contains("host4"));
}

// --- Process error handling ---

TEST(PacketTransportTest, ProcessShortBuffer) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  uint8_t buf[] = {0x53};
  // Too short for a magic number - should return safely
  transport.process_({buf, 1});
}

TEST(PacketTransportTest, ProcessBadMagic) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  uint8_t buf[] = {0xFF, 0xFF, 0x00, 0x00};
  // Wrong magic - should return safely
  transport.process_({buf, sizeof(buf)});
}

TEST(PacketTransportTest, ProcessOwnHostname) {
  TestablePacketTransport transport;
  transport.init_for_test("myself");
  // Build a packet from "myself" using a separate encoder
  TestablePacketTransport fake_sender;
  fake_sender.init_for_test("myself");
  fake_sender.send_data_(true);
  ASSERT_EQ(fake_sender.sent_packets.size(), 1u);

  auto &packet = fake_sender.sent_packets[0];
  // Should be silently ignored because hostname matches our own
  transport.process_({packet.data(), packet.size()});
}

TEST(PacketTransportTest, ProcessUnknownHostname) {
  TestablePacketTransport transport;
  transport.init_for_test("receiver");
  // No providers registered - "unknown" will not be found
  TestablePacketTransport sender;
  sender.init_for_test("unknown");
  sender.send_data_(true);
  ASSERT_EQ(sender.sent_packets.size(), 1u);

  auto &packet = sender.sent_packets[0];
  // Should return safely without crash
  transport.process_({packet.data(), packet.size()});
}

// --- Send disabled ---

TEST(PacketTransportTest, NoSendWhenDisabled) {
  TestablePacketTransport transport;
  transport.init_for_test("sender");
  transport.send_enabled = false;
  transport.send_data_(true);
  EXPECT_TRUE(transport.sent_packets.empty());
}

}  // namespace esphome::packet_transport::testing
