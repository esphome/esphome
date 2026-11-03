// Covers the CRC engine, the RX framer and the TX queue without a real UART.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/rs485_frame.h"

namespace esphome::rs485_frame::testing {

namespace {

class RS485FrameHubProbe : public RS485FrameHub {
 public:
  uint16_t crc_for_test(const std::vector<uint8_t> &data, bool include_header) const {
    return this->calculate_crc_(data.data(), data.size(), include_header);
  }
  void build_frame_for_test(const std::vector<uint8_t> &payload, std::vector<uint8_t> &out) {
    this->build_frame_(payload, out);
  }
  void feed_for_test(const std::vector<uint8_t> &bytes) {
    for (uint8_t b : bytes)
      this->handle_rx_byte_(b, 0);
  }
  const std::vector<uint8_t> &rx_payload_for_test() const { return this->rx_payload_; }
};

// The hand-rolled CRCs the component used before switching to the helpers.h functions; the
// helpers must produce identical values for every CRC type and both header variants.
uint16_t reference_crc(CrcType type, uint8_t dle, uint8_t stx, const std::vector<uint8_t> &payload,
                       bool include_header) {
  std::vector<uint8_t> data;
  if (include_header) {
    data.push_back(dle);
    data.push_back(stx);
  }
  data.insert(data.end(), payload.begin(), payload.end());
  switch (type) {
    case CRC_TYPE_NONE:
      return 0;
    case CRC_TYPE_XOR8: {
      uint8_t crc = 0;
      for (auto b : data)
        crc ^= b;
      return crc;
    }
    case CRC_TYPE_CRC16_MODBUS: {
      uint16_t crc = 0xFFFF;
      for (auto b : data) {
        crc ^= b;
        for (uint8_t i = 0; i < 8; i++)
          crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : crc >> 1;
      }
      return crc;
    }
    default: {
      uint32_t sum = 0;
      for (auto b : data)
        sum += b;
      return type == CRC_TYPE_SUM8 ? (sum & 0xFF) : (sum & 0xFFFF);
    }
  }
}

}  // namespace

TEST(RS485FrameHubTest, CrcHelpersMatchReferenceImplementation) {
  std::vector<std::vector<uint8_t>> payloads = {
      {},
      {0x01, 0x02},
      {'1', '2', '3', '4', '5', '6', '7', '8', '9'},
      {0x10, 0x10, 0x00, 0xFF, 0xFE, 0x03, 0x02},
  };
  // A long payload so the 16-bit sums wrap.
  std::vector<uint8_t> long_payload(600);
  for (size_t i = 0; i < long_payload.size(); i++)
    long_payload[i] = static_cast<uint8_t>(0xFF - i);
  payloads.push_back(long_payload);

  for (CrcType type : {CRC_TYPE_NONE, CRC_TYPE_SUM8, CRC_TYPE_SUM16_BIG_ENDIAN, CRC_TYPE_SUM16_LITTLE_ENDIAN,
                       CRC_TYPE_XOR8, CRC_TYPE_CRC16_MODBUS}) {
    RS485FrameHubProbe hub;
    hub.set_framing(0x10, 0x02, 0x03, 0x00);
    hub.set_crc_type(type);
    for (const auto &payload : payloads) {
      for (bool include_header : {false, true}) {
        EXPECT_EQ(hub.crc_for_test(payload, include_header), reference_crc(type, 0x10, 0x02, payload, include_header))
            << "type " << type << " len " << payload.size() << " header " << include_header;
      }
    }
  }
}

TEST(RS485FrameHubTest, Crc16ModbusCheckValue) {
  RS485FrameHubProbe hub;
  hub.set_crc_type(CRC_TYPE_CRC16_MODBUS);
  // The published CRC-16/MODBUS check value for "123456789".
  EXPECT_EQ(hub.crc_for_test({'1', '2', '3', '4', '5', '6', '7', '8', '9'}, false), 0x4B37);
}

// A frame built by the TX encoder must come back through the RX framer as the same payload,
// for both escape modes and with DLE bytes in the payload and the CRC.
TEST(RS485FrameHubTest, RxFramerRoundTripsBuiltFrames) {
  for (uint8_t escape_marker : {uint8_t{0x00}, uint8_t{0x10}}) {
    for (CrcType type : {CRC_TYPE_SUM16_BIG_ENDIAN, CRC_TYPE_CRC16_MODBUS, CRC_TYPE_XOR8, CRC_TYPE_NONE}) {
      RS485FrameHubProbe hub;
      hub.set_framing(0x10, 0x02, 0x03, escape_marker);
      hub.set_crc_type(type);
      hub.setup();

      const std::vector<uint8_t> payload = {0x01, 0x02, 0x10, 0x03, 0x10, 0x10, 0x02};
      std::vector<uint8_t> frame;
      frame.reserve(64);
      hub.build_frame_for_test(payload, frame);
      hub.feed_for_test(frame);

      EXPECT_EQ(hub.get_frames_received(), 1u) << "escape " << int(escape_marker) << " type " << type;
      EXPECT_EQ(hub.get_crc_failures(), 0u);
      EXPECT_EQ(hub.rx_payload_for_test(), payload);
    }
  }
}

TEST(RS485FrameHubTest, RxFramerRejectsBadCrcAndOverlongFrames) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_SUM8);
  hub.set_max_frame_length(8);
  hub.setup();

  // DLE STX 01 02 crc(wrong) DLE ETX
  hub.feed_for_test({0x10, 0x02, 0x01, 0x02, 0x00, 0x10, 0x03});
  EXPECT_EQ(hub.get_crc_failures(), 1u);
  EXPECT_EQ(hub.get_frames_received(), 0u);

  // Longer than max_frame_length: abandoned mid-receive.
  hub.feed_for_test({0x10, 0x02, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x03});
  EXPECT_EQ(hub.get_discarded_frames(), 1u);
  EXPECT_EQ(hub.get_frames_received(), 0u);
}

TEST(RS485FrameHubTest, QueueFrameFramesAndQueues) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_NONE);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x10, 0x02};

  EXPECT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  EXPECT_EQ(hub.get_queue_depth(), 1u);
  EXPECT_EQ(hub.get_command_drops(), 0u);

  // replace_latest: a second frame replaces the first and counts a drop.
  EXPECT_TRUE(hub.queue_raw_frame({0x01, 0x02}, {0x03}));
  EXPECT_EQ(hub.get_queue_depth(), 1u);
  EXPECT_EQ(hub.get_command_drops(), 1u);
}

TEST(RS485FrameHubTest, QueueFrameDropsOverlongAndSnifferFrames) {
  RS485FrameHubProbe hub;
  hub.set_max_frame_length(8);
  hub.setup();
  static const uint8_t DATA[9] = {};

  EXPECT_FALSE(hub.queue_frame(DATA, sizeof(DATA)));
  EXPECT_EQ(hub.get_command_drops(), 1u);

  hub.set_sniffer_only(true);
  EXPECT_FALSE(hub.queue_frame(DATA, 2));
  EXPECT_EQ(hub.get_command_drops(), 2u);
  EXPECT_EQ(hub.get_queue_depth(), 0u);
}

TEST(RS485FrameHubTest, FrameCallbackReceivesPayload) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_XOR8);
  hub.setup();
  std::vector<uint8_t> received;
  hub.add_on_frame_callback([&](const std::vector<uint8_t> &payload) { received = payload; });

  const std::vector<uint8_t> payload = {0x01, 0x03, 0x41};
  std::vector<uint8_t> frame;
  frame.reserve(32);
  hub.build_frame_for_test(payload, frame);
  hub.feed_for_test(frame);

  EXPECT_EQ(received, payload);
  static const uint8_t PREFIX[] = {0x01, 0x03};
  static const uint8_t OTHER[] = {0x01, 0x09};
  EXPECT_TRUE(frame_has_prefix(received, PREFIX, sizeof(PREFIX)));
  EXPECT_FALSE(frame_has_prefix(received, OTHER, sizeof(OTHER)));
}

}  // namespace esphome::rs485_frame::testing
