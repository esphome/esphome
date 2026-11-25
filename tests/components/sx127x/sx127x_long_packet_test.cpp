#include "sx127x_test_helpers.h"
#include <cmath>
#include <numeric>

namespace esphome::sx127x::testing {

static std::vector<uint8_t> make_packet(size_t length) {
  std::vector<uint8_t> packet(length);
  std::iota(packet.begin(), packet.end(), 1);
  return packet;
}

// The first byte is the total length
static int32_t length_from_first_byte(const std::vector<uint8_t> &x) { return x.empty() ? 0 : x[0]; }
static int32_t discard(const std::vector<uint8_t> &x) { return -1; }

// FSK packet mode with a payload length above 64 or a lambda selects long packets
static void configure_long(RadioHardware &hw, uint32_t payload_length, payload_length_func_t func = nullptr) {
  hw.radio.set_modulation(MOD_FSK);
  hw.radio.set_bandwidth(SX127X_BW_50_0);
  hw.radio.set_bitrate(4800);
  hw.radio.set_deviation(5000);
  hw.radio.set_packet_mode(true);
  hw.radio.set_rx_start(true);
  hw.radio.set_crc_enable(false);
  if (func != nullptr) {
    hw.radio.set_payload_length_lambda(func);
  } else {
    hw.radio.set_payload_length(payload_length);
  }
  hw.radio.call();  // The normal component lifecycle runs setup().
}

TEST(SX127xLongPacket, FixedLengthArrivesOverSeveralDrains) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 100);
  ASSERT_FALSE(hw.radio.is_failed());
  // Unlimited length format, with DIO1 as FifoEmpty
  EXPECT_EQ(hw.regs[REG_PAYLOAD_LENGTH_LSB], 0);
  EXPECT_EQ(hw.regs[REG_FIFO_THRESH], TX_START_FIFO_EMPTY | FIFO_THRESHOLD_HALF);
  EXPECT_EQ(hw.regs[REG_DIO_MAPPING1] & DIO1_MAPPING_FIFO_EMPTY, DIO1_MAPPING_FIFO_EMPTY);
  EXPECT_FALSE(hw.radio.is_receiving());

  const std::vector<uint8_t> packet = make_packet(100);
  hw.regs[REG_RSSI_VALUE_FSK] = 120;
  hw.feed({packet.begin(), packet.begin() + 40});
  hw.radio.loop();
  EXPECT_TRUE(hw.rx_fifo.empty());
  EXPECT_TRUE(packets.packets.empty());
  EXPECT_TRUE(hw.radio.is_receiving());

  hw.regs[REG_RSSI_VALUE_FSK] = 200;  // Sampled once, on the first drain
  // Bytes past the length belong to the next packet and are not read into this one
  hw.feed({packet.begin() + 40, packet.end()});
  hw.feed({0xEE, 0xEE});
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], packet);
  EXPECT_FLOAT_EQ(packets.rssi, -60.0f);
  EXPECT_TRUE(std::isnan(packets.snr));
  // Reception restarts with an empty FIFO
  EXPECT_TRUE(hw.receiving());
  EXPECT_TRUE(hw.rx_fifo.empty());
  EXPECT_FALSE(hw.radio.is_receiving());
}

TEST(SX127xLongPacket, LambdaResolvesTheLength) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 0, length_from_first_byte);
  std::vector<uint8_t> packet = make_packet(80);
  packet[0] = 80;
  hw.feed(packet);
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], packet);

  // A second packet of another length
  packet = make_packet(200);
  packet[0] = 200;
  hw.feed(packet);
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 2u);
  EXPECT_EQ(packets.packets[1], packet);
}

TEST(SX127xLongPacket, LambdaDiscardsAPacket) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 0, discard);
  hw.feed(make_packet(50));
  hw.radio.loop();
  EXPECT_TRUE(packets.packets.empty());
  EXPECT_TRUE(hw.rx_fifo.empty());
  EXPECT_TRUE(hw.receiving());
  EXPECT_FALSE(hw.radio.is_receiving());
}

TEST(SX127xLongPacket, FifoOverrunRestartsReception) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 100);
  hw.feed(make_packet(40));
  hw.radio.loop();
  hw.feed(make_packet(10));
  hw.overrun = true;
  hw.radio.loop();
  EXPECT_FALSE(hw.overrun);
  EXPECT_TRUE(packets.packets.empty());
  EXPECT_TRUE(hw.receiving());
  EXPECT_FALSE(hw.radio.is_receiving());
  // The partial packet is dropped, not joined with the next one
  hw.feed(make_packet(100));
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], make_packet(100));
}

TEST(SX127xLongPacket, SendingIsRefused) {
  RadioHardware hw;
  TransmitListener listener;
  configure_long(hw, 100);
  EXPECT_EQ(hw.radio.transmit_packet(make_packet(100)), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, make_packet(100)), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  hw.radio.set_mode_tx();
  EXPECT_TRUE(hw.sent.empty());
  EXPECT_TRUE(listener.results.empty());
  EXPECT_TRUE(hw.receiving());
}

TEST(SX127xLongPacket, ModeSetFromThePacketCallbackIsKept) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 100);
  packets.on_receive = [&]() { hw.radio.set_mode_standby(); };
  hw.feed(make_packet(100));
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(hw.mode(), MODE_STDBY);
}

TEST(SX127xLongPacket, FailedRestartIsReconfiguredWithoutBeingAsked) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 100);
  hw.feed(make_packet(100));
  hw.stuck = true;
  hw.radio.loop();
  // The packet was complete, so it is still delivered
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_FALSE(hw.radio.is_failed());
  EXPECT_TRUE(hw.radio.status_has_warning());
  hw.stuck = false;
  run_scheduler(1100);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving());
  hw.feed(make_packet(100));
  hw.radio.loop();
  EXPECT_EQ(packets.packets.size(), 2u);
}

TEST(SX127xLongPacket, PayloadLengthChangedAtRunTime) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  configure_long(hw, 0, length_from_first_byte);
  // A fixed length replaces the lambda; up to 64 bytes are a short packet, signalled by PayloadReady on DIO0
  hw.radio.set_payload_length(3);
  hw.radio.configure();
  EXPECT_EQ(hw.regs[REG_PAYLOAD_LENGTH_LSB], 3);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
  hw.receive({0x01, 0x02, 0x03});
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], (std::vector<uint8_t>{0x01, 0x02, 0x03}));

  hw.radio.set_payload_length(100);
  hw.radio.configure();
  hw.feed(make_packet(100));
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 2u);
  EXPECT_EQ(packets.packets[1], make_packet(100));
}

}  // namespace esphome::sx127x::testing
