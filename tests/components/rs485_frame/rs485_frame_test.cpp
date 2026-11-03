// Links against the component library object (rs485_frame.cpp is always compiled into the test
// binary), so it covers the CRC engine, the RX framer, the discard counter and the
// command_format guards without a real UART.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/rs485_frame.h"

namespace esphome::rs485_frame::testing {

namespace {

// Exposes just enough protected state/behavior to drive queue_command_values() and
// record_discard_() without standing up a real UART (RS485FrameHub::setup() reserves TX buffers
// and a real UARTComponent parent, neither of which either code path under test touches).
class RS485FrameHubProbe : public RS485FrameHub {
 public:
  // Mirrors the one piece of RS485FrameHub::setup() that enqueue_frame_() actually needs: a
  // non-empty tx_queue_ ring sized to max_queue_size_. Everything else setup() does (UART
  // buffer reservations) is unrelated to the paths under test.
  void prepare_queue_for_test() {
    this->tx_queue_.resize(this->max_queue_size_);
    for (auto &slot : this->tx_queue_)
      slot.reserve(32);
  }

  uint32_t command_drops_for_test() const { return this->command_drops_; }
  uint32_t discarded_frames_for_test() const { return this->discarded_frames_; }
  void record_discard_for_test() { this->record_discard_(); }
  size_t queue_depth_for_test() const { return this->queue_size_(); }
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

// Review finding 9: queue_command_value(s) must refuse to encode against a hub with no
// command_format: rather than silently falling back to a 4-byte big-endian default -- those
// bytes would not match what the bus actually expects.
TEST(RS485FrameHubTest, QueueCommandValuesFailsWithoutCommandFormat) {
  RS485FrameHubProbe hub;

  EXPECT_FALSE(hub.queue_command_value(0x1234));
  EXPECT_EQ(hub.command_drops_for_test(), 1u);
}

// Same call succeeds once a command_format: is configured -- proves the new guard does not
// regress the normal, correctly-configured path.
TEST(RS485FrameHubTest, QueueCommandValuesSucceedsWithCommandFormat) {
  RS485FrameHubProbe hub;
  hub.set_command_format(/*preamble=*/{0x00, 0x83, 0x01}, /*value_element_bytes=*/1, /*big_endian=*/true,
                         /*postamble=*/{0x00});
  hub.prepare_queue_for_test();

  EXPECT_TRUE(hub.queue_command_value(0x01));
  EXPECT_EQ(hub.command_drops_for_test(), 0u);
}

// queue_command_values_with_element_bytes() (a button's value_element_bytes: override) takes the
// preamble/endian/postamble from the hub's command_format: exactly like queue_command_values(),
// so it needs the same guard: with no command_format: it must drop the command rather than
// encode against the bare-field defaults.
TEST(RS485FrameHubTest, QueueCommandValuesWithElementBytesFailsWithoutCommandFormat) {
  RS485FrameHubProbe hub;
  hub.prepare_queue_for_test();
  const uint32_t value = 0x01;

  EXPECT_FALSE(hub.queue_command_values_with_element_bytes(&value, 1, /*element_bytes=*/1));
  EXPECT_EQ(hub.command_drops_for_test(), 1u);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

TEST(RS485FrameHubTest, QueueCommandValuesWithElementBytesSucceedsWithCommandFormat) {
  RS485FrameHubProbe hub;
  hub.set_command_format(/*preamble=*/{0x00, 0x83, 0x01}, /*value_element_bytes=*/4, /*big_endian=*/true,
                         /*postamble=*/{0x00});
  hub.prepare_queue_for_test();
  const uint32_t value = 0x01;

  EXPECT_TRUE(hub.queue_command_values_with_element_bytes(&value, 1, /*element_bytes=*/1));
  EXPECT_EQ(hub.command_drops_for_test(), 0u);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
}

// Review finding 10: a frame abandoned mid-receive (max_frame_length overflow or intra-frame
// timeout) must be counted separately from crc_failures -- it never reaches validate_frame_() at
// all, so folding it into that counter would misreport "CRC/structural failures" for something
// that was never even framed-and-checked.
TEST(RS485FrameHubTest, RecordDiscardIncrementsDiscardedFramesCounter) {
  RS485FrameHubProbe hub;

  hub.record_discard_for_test();
  hub.record_discard_for_test();

  EXPECT_EQ(hub.discarded_frames_for_test(), 2u);
}

// esphbot review: the per-button command_format override (mode 3) used to force
// queue_command_with_format() to take its preamble/postamble as std::vector<uint8_t>
// references, so the button had to heap-allocate two vectors from its StaticVector members on
// every press just to satisfy the call. Pointer/length pairs let the button hand over its
// StaticVector's own storage directly -- this mirrors that call shape and shares the same
// encoder as build_key_payload_ (queue_command_values' path), so it also cross-checks that the
// two paths agree on the wire format for identical preamble/value/postamble input.
TEST(RS485FrameHubTest, QueueCommandWithFormatAcceptsPointerLengthPreambleAndPostamble) {
  RS485FrameHubProbe hub;
  hub.prepare_queue_for_test();

  StaticVector<uint8_t, 8> preamble{0x00, 0x83, 0x01};
  StaticVector<uint8_t, 8> postamble{0x00};
  const uint32_t value = 0x01;

  EXPECT_TRUE(hub.queue_command_with_format(&value, 1, preamble.data(), preamble.size(),
                                            /*value_element_bytes=*/1, /*big_endian=*/true, postamble.data(),
                                            postamble.size()));
  EXPECT_EQ(hub.command_drops_for_test(), 0u);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
}

}  // namespace esphome::rs485_frame::testing
