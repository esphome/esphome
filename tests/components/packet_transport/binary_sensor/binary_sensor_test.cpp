#include "../common.h"

namespace esphome::packet_transport::testing {

TEST(PacketTransportBinarySensorTest, AddBinarySensor) {
  TestablePacketTransport transport;
  binary_sensor::BinarySensor bs;
  transport.add_binary_sensor("motion", &bs);
  ASSERT_EQ(transport.binary_sensors_.size(), 1u);
  EXPECT_STREQ(transport.binary_sensors_[0].id, "motion");
  EXPECT_EQ(transport.binary_sensors_[0].sensor, &bs);
}

TEST(PacketTransportBinarySensorTest, AddRemoteBinarySensor) {
  TestablePacketTransport transport;
  binary_sensor::BinarySensor bs;
  transport.add_remote_binary_sensor("host1", "remote_motion", &bs);
  EXPECT_TRUE(transport.providers_.contains("host1"));
  EXPECT_EQ(transport.remote_binary_sensors_["host1"]["remote_motion"], &bs);
}

TEST(PacketTransportBinarySensorTest, UnencryptedBinarySensorRoundTrip) {
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

TEST(PacketTransportBinarySensorTest, MultipleSensorsRoundTrip) {
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

}  // namespace esphome::packet_transport::testing
