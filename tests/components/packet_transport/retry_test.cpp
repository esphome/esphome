#include "common.h"

namespace esphome::packet_transport::testing {

class RejectingTransport : public TestablePacketTransport {
 public:
  using PacketTransport::resend_ping_key_;
  using PacketTransport::send_ping_pong_request_;
  mutable unsigned attempts{0};
  unsigned reject_at{1};
  bool try_send_packet(const std::vector<uint8_t> &buf) const override {
    if (++this->attempts == this->reject_at) {
      return false;
    }
    return PacketTransport::try_send_packet(buf);
  }
};

TEST(PacketTransportRetry, RejectedPingRetainsPendingFlagAndPreviousKey) {
  RejectingTransport transport;
  transport.init_for_test("sender");
  transport.set_ping_pong_enable(true);
  transport.ping_key_ = 123;
  transport.resend_ping_key_ = true;
  transport.send_ping_pong_request_();
  EXPECT_TRUE(transport.resend_ping_key_);
  EXPECT_EQ(transport.ping_key_, 123u);
  EXPECT_TRUE(transport.sent_packets.empty());
  transport.loop();
  EXPECT_FALSE(transport.resend_ping_key_);
  ASSERT_EQ(transport.sent_packets.size(), 1u);
  const auto &packet = transport.sent_packets[0];
  uint32_t sent_key;
  memcpy(&sent_key, packet.data() + packet.size() - sizeof(sent_key), sizeof(sent_key));
  EXPECT_EQ(transport.ping_key_, sent_key);
  transport.loop();
  EXPECT_EQ(transport.sent_packets.size(), 1u);
}

#if defined(USE_SENSOR) && defined(USE_BINARY_SENSOR)
TEST(PacketTransportRetry, SplitUpdateKeepsAllValuesAfterLaterPacketRejection) {
  RejectingTransport transport;
  transport.init_for_test("s");
  transport.max_packet_size = 12;
  transport.reject_at = 2;
  sensor::Sensor first, second;
  binary_sensor::BinarySensor third;
  first.state = 10;
  second.state = 20;
  third.state = true;
  transport.set_sensor_count(2);
  transport.add_sensor("a", &first);
  transport.add_sensor("b", &second);
  transport.set_binary_sensor_count(1);
  transport.add_binary_sensor("c", &third);
  transport.send_data_(true);
  ASSERT_EQ(transport.sent_packets.size(), 1u);
  EXPECT_TRUE(transport.updated_);
  EXPECT_TRUE(transport.sensors_[0].updated);
  EXPECT_TRUE(transport.sensors_[1].updated);
  EXPECT_TRUE(transport.binary_sensors_[0].updated);
  transport.loop();
  EXPECT_EQ(transport.sent_packets.size(), 4u);
  EXPECT_FALSE(transport.updated_);
  EXPECT_FALSE(transport.sensors_[0].updated);
  EXPECT_FALSE(transport.sensors_[1].updated);
  EXPECT_FALSE(transport.binary_sensors_[0].updated);
  transport.loop();
  EXPECT_EQ(transport.sent_packets.size(), 4u);
}

TEST(PacketTransportRetry, PeriodicRefreshRemainsPendingWhenSchedulingGateIsClosed) {
  RejectingTransport transport;
  transport.init_for_test("sender");
  sensor::Sensor value;
  transport.set_sensor_count(1);
  transport.add_sensor("temp", &value);
  transport.sensors_[0].updated = false;
  transport.send_enabled = false;
  transport.send_data_(true);
  EXPECT_TRUE(transport.updated_);
  EXPECT_TRUE(transport.sensors_[0].updated);
  EXPECT_EQ(transport.attempts, 0u);
  transport.send_enabled = true;
  transport.reject_at = 0;
  transport.loop();
  EXPECT_FALSE(transport.updated_);
  EXPECT_FALSE(transport.sensors_[0].updated);
  EXPECT_EQ(transport.sent_packets.size(), 1u);
}
#endif

}  // namespace esphome::packet_transport::testing
