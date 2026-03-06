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

// --- Sensor management (requires USE_SENSOR / USE_BINARY_SENSOR) ---

#ifdef USE_SENSOR
TEST(PacketTransportTest, AddSensor) {
  TestablePacketTransport transport;
  sensor::Sensor s;
  transport.add_sensor("temp", &s);
  ASSERT_EQ(transport.sensors_.size(), 1u);
  EXPECT_STREQ(transport.sensors_[0].id, "temp");
  EXPECT_EQ(transport.sensors_[0].sensor, &s);
  EXPECT_TRUE(transport.sensors_[0].updated);
}

TEST(PacketTransportTest, AddRemoteSensor) {
  TestablePacketTransport transport;
  sensor::Sensor s;
  transport.add_remote_sensor("host1", "remote_temp", &s);
  EXPECT_TRUE(transport.providers_.contains("host1"));
  EXPECT_EQ(transport.remote_sensors_["host1"]["remote_temp"], &s);
}
#endif

#ifdef USE_BINARY_SENSOR
TEST(PacketTransportTest, AddBinarySensor) {
  TestablePacketTransport transport;
  binary_sensor::BinarySensor bs;
  transport.add_binary_sensor("motion", &bs);
  ASSERT_EQ(transport.binary_sensors_.size(), 1u);
  EXPECT_STREQ(transport.binary_sensors_[0].id, "motion");
  EXPECT_EQ(transport.binary_sensors_[0].sensor, &bs);
}

TEST(PacketTransportTest, AddRemoteBinarySensor) {
  TestablePacketTransport transport;
  binary_sensor::BinarySensor bs;
  transport.add_remote_binary_sensor("host1", "remote_motion", &bs);
  EXPECT_TRUE(transport.providers_.contains("host1"));
  EXPECT_EQ(transport.remote_binary_sensors_["host1"]["remote_motion"], &bs);
}
#endif

// --- Unencrypted round-trip tests (require USE_SENSOR / USE_BINARY_SENSOR) ---

#ifdef USE_SENSOR
TEST(PacketTransportTest, UnencryptedSensorRoundTrip) {
  // Encoder
  TestablePacketTransport encoder;
  encoder.init_for_test("sender");
  sensor::Sensor local_sensor;
  local_sensor.state = 42.5f;
  encoder.add_sensor("temp", &local_sensor);

  encoder.send_data_(true);
  ASSERT_EQ(encoder.sent_packets.size(), 1u);

  // Decoder
  TestablePacketTransport decoder;
  decoder.init_for_test("receiver");
  sensor::Sensor remote_sensor;
  remote_sensor.state = -999.0f;  // sentinel
  decoder.add_remote_sensor("sender", "temp", &remote_sensor);

  auto &packet = encoder.sent_packets[0];
  decoder.process_({packet.data(), packet.size()});
  EXPECT_FLOAT_EQ(remote_sensor.state, 42.5f);
}
#endif

#ifdef USE_BINARY_SENSOR
TEST(PacketTransportTest, UnencryptedBinarySensorRoundTrip) {
  TestablePacketTransport encoder;
  encoder.init_for_test("sender");
  binary_sensor::BinarySensor local_bs;
  local_bs.state = true;
  encoder.add_binary_sensor("motion", &local_bs);

  encoder.send_data_(true);
  ASSERT_EQ(encoder.sent_packets.size(), 1u);

  TestablePacketTransport decoder;
  decoder.init_for_test("receiver");
  binary_sensor::BinarySensor remote_bs;
  decoder.add_remote_binary_sensor("sender", "motion", &remote_bs);

  auto &packet = encoder.sent_packets[0];
  decoder.process_({packet.data(), packet.size()});
  EXPECT_TRUE(remote_bs.state);
}
#endif

#if defined(USE_SENSOR) && defined(USE_BINARY_SENSOR)
TEST(PacketTransportTest, MultipleSensorsRoundTrip) {
  TestablePacketTransport encoder;
  encoder.init_for_test("sender");

  sensor::Sensor s1, s2;
  s1.state = 10.0f;
  s2.state = 20.0f;
  encoder.add_sensor("s1", &s1);
  encoder.add_sensor("s2", &s2);

  binary_sensor::BinarySensor bs1;
  bs1.state = true;
  encoder.add_binary_sensor("bs1", &bs1);

  encoder.send_data_(true);
  ASSERT_EQ(encoder.sent_packets.size(), 1u);

  TestablePacketTransport decoder;
  decoder.init_for_test("receiver");
  sensor::Sensor rs1, rs2;
  binary_sensor::BinarySensor rbs1;
  rs1.state = -999.0f;
  rs2.state = -999.0f;
  decoder.add_remote_sensor("sender", "s1", &rs1);
  decoder.add_remote_sensor("sender", "s2", &rs2);
  decoder.add_remote_binary_sensor("sender", "bs1", &rbs1);

  auto &packet = encoder.sent_packets[0];
  decoder.process_({packet.data(), packet.size()});

  EXPECT_FLOAT_EQ(rs1.state, 10.0f);
  EXPECT_FLOAT_EQ(rs2.state, 20.0f);
  EXPECT_TRUE(rbs1.state);
}
#endif

// --- Encrypted round-trip ---

#ifdef USE_SENSOR
TEST(PacketTransportTest, EncryptedSensorRoundTrip) {
  std::vector<uint8_t> key(32);
  for (int i = 0; i < 32; i++)
    key[i] = i;

  TestablePacketTransport encoder;
  encoder.init_for_test("sender");
  encoder.set_encryption_key(key);
  sensor::Sensor local_sensor;
  local_sensor.state = 99.9f;
  encoder.add_sensor("temp", &local_sensor);

  encoder.send_data_(true);
  ASSERT_EQ(encoder.sent_packets.size(), 1u);

  TestablePacketTransport decoder;
  decoder.init_for_test("receiver");
  sensor::Sensor remote_sensor;
  remote_sensor.state = -999.0f;
  decoder.add_remote_sensor("sender", "temp", &remote_sensor);
  decoder.set_provider_encryption("sender", key);

  auto &packet = encoder.sent_packets[0];
  decoder.process_({packet.data(), packet.size()});
  EXPECT_FLOAT_EQ(remote_sensor.state, 99.9f);
}

// --- Selective send ---

TEST(PacketTransportTest, SendDataOnlyUpdated) {
  TestablePacketTransport encoder;
  encoder.init_for_test("sender");

  sensor::Sensor s1, s2;
  s1.state = 1.0f;
  s2.state = 2.0f;
  encoder.add_sensor("s1", &s1);
  encoder.add_sensor("s2", &s2);

  // Mark s1 as not updated, only s2 as updated
  encoder.sensors_[0].updated = false;
  encoder.sensors_[1].updated = true;

  encoder.send_data_(false);
  ASSERT_EQ(encoder.sent_packets.size(), 1u);

  TestablePacketTransport decoder;
  decoder.init_for_test("receiver");
  sensor::Sensor rs1, rs2;
  rs1.state = -999.0f;
  rs2.state = -999.0f;
  decoder.add_remote_sensor("sender", "s1", &rs1);
  decoder.add_remote_sensor("sender", "s2", &rs2);

  auto &packet = encoder.sent_packets[0];
  decoder.process_({packet.data(), packet.size()});

  EXPECT_FLOAT_EQ(rs1.state, -999.0f);  // not updated, not sent
  EXPECT_FLOAT_EQ(rs2.state, 2.0f);     // updated, sent
}
#endif

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

#ifdef USE_SENSOR
TEST(PacketTransportTest, PingKeyIncludedInTransmittedPacket) {
  std::vector<uint8_t> key(32, 0xBB);

  // Responder: encrypted, owns a sensor
  TestablePacketTransport responder;
  responder.init_for_test("responder");
  responder.set_encryption_key(key);
  sensor::Sensor local_sensor;
  local_sensor.state = 77.7f;
  responder.add_sensor("temp", &local_sensor);

  // Requester sends a MAGIC_PING that the responder processes
  auto ping = build_ping_packet("requester", 0xDEADBEEF);
  responder.process_({ping.data(), ping.size()});
  ASSERT_EQ(responder.ping_keys_.size(), 1u);

  // Responder sends sensor data — ping key should be embedded
  responder.send_data_(true);
  ASSERT_EQ(responder.sent_packets.size(), 1u);

  // Requester: encrypted provider, ping-pong enabled, expects key 0xDEADBEEF
  TestablePacketTransport requester;
  requester.init_for_test("requester");
  requester.set_ping_pong_enable(true);
  requester.ping_key_ = 0xDEADBEEF;
  sensor::Sensor remote_sensor;
  remote_sensor.state = -999.0f;
  requester.add_remote_sensor("responder", "temp", &remote_sensor);
  requester.set_provider_encryption("responder", key);

  // The requester decrypts the packet and finds its ping key echoed back,
  // which gates the sensor data — if the key is missing, data is blocked.
  auto &packet = responder.sent_packets[0];
  requester.process_({packet.data(), packet.size()});
  EXPECT_FLOAT_EQ(remote_sensor.state, 77.7f);
}

TEST(PacketTransportTest, MissingPingKeyBlocksSensorData) {
  std::vector<uint8_t> key(32, 0xBB);

  // Responder sends data WITHOUT receiving any MAGIC_PING first — no ping keys
  TestablePacketTransport responder;
  responder.init_for_test("responder");
  responder.set_encryption_key(key);
  sensor::Sensor local_sensor;
  local_sensor.state = 77.7f;
  responder.add_sensor("temp", &local_sensor);
  responder.send_data_(true);
  ASSERT_EQ(responder.sent_packets.size(), 1u);

  // Requester with ping-pong enabled expects a key that isn't in the packet
  TestablePacketTransport requester;
  requester.init_for_test("requester");
  requester.set_ping_pong_enable(true);
  requester.ping_key_ = 0xDEADBEEF;
  sensor::Sensor remote_sensor;
  remote_sensor.state = -999.0f;
  requester.add_remote_sensor("responder", "temp", &remote_sensor);
  requester.set_provider_encryption("responder", key);

  auto &packet = responder.sent_packets[0];
  requester.process_({packet.data(), packet.size()});
  EXPECT_FLOAT_EQ(remote_sensor.state, -999.0f);  // blocked — ping key not found
}
#endif

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
