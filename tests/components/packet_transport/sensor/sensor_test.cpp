#include "../common.h"

namespace esphome::packet_transport::testing {

TEST(PacketTransportSensorTest, AddSensor) {
  TestablePacketTransport transport;
  sensor::Sensor s;
  transport.add_sensor("temp", &s);
  ASSERT_EQ(transport.sensors_.size(), 1u);
  EXPECT_STREQ(transport.sensors_[0].id, "temp");
  EXPECT_EQ(transport.sensors_[0].sensor, &s);
  EXPECT_TRUE(transport.sensors_[0].updated);
}

TEST(PacketTransportSensorTest, AddRemoteSensor) {
  TestablePacketTransport transport;
  sensor::Sensor s;
  transport.add_remote_sensor("host1", "remote_temp", &s);
  EXPECT_TRUE(transport.providers_.contains("host1"));
  EXPECT_EQ(transport.remote_sensors_["host1"]["remote_temp"], &s);
}

TEST(PacketTransportSensorTest, UnencryptedSensorRoundTrip) {
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

TEST(PacketTransportSensorTest, EncryptedSensorRoundTrip) {
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

TEST(PacketTransportSensorTest, SendDataOnlyUpdated) {
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

TEST(PacketTransportSensorTest, PingKeyIncludedInTransmittedPacket) {
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

TEST(PacketTransportSensorTest, MissingPingKeyBlocksSensorData) {
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

}  // namespace esphome::packet_transport::testing
