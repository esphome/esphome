// Covers the CRC engine, the RX framer and the TX queue without a real UART.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/rs485_frame.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

namespace esphome::rs485_frame::testing {

namespace {

// Records what the hub writes; reads nothing.
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  std::vector<uint8_t> written;
};

class RS485FrameHubProbe : public RS485FrameHub {
 public:
  uint16_t crc_for_test(const std::vector<uint8_t> &data, bool include_header) const {
    return this->calculate_crc_(data.data(), data.size(), include_header);
  }
  std::vector<uint8_t> build_frame_for_test(const std::vector<uint8_t> &payload) const {
    std::vector<uint8_t> out(payload.size() * 2 + FRAME_OVERHEAD_BYTES);
    out.resize(this->build_frame_(payload.data(), payload.size(), out.data()));
    return out;
  }
  void feed_for_test(const std::vector<uint8_t> &bytes) {
    for (uint8_t b : bytes)
      this->handle_rx_byte_(b, 0);
  }
  std::vector<uint8_t> rx_payload_for_test() const {
    return {this->rx_payload_.get(), this->rx_payload_.get() + this->rx_payload_len_};
  }
  void maybe_tx_for_test(uint32_t now) { this->maybe_tx_(now); }
  void send_next_for_test(uint32_t now) { this->send_next_(now); }
  void loop_at_for_test(uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    this->loop();
  }
  uint32_t last_activity_for_test() const { return this->last_activity_time_; }
  size_t queue_depth_for_test() const { return this->tx_queue_count_; }
  bool has_tx_queue_for_test() const { return this->tx_queue_ != nullptr; }
  std::vector<uint8_t> queued_frame_for_test(size_t i) const {
    const size_t index = (this->tx_queue_head_ + i) % this->max_queue_size_;
    const uint8_t *slot = this->tx_slot_(index);
    return {slot, slot + this->tx_queue_len_[index]};
  }
  // Counts the frames delivered to on_frame callbacks.
  void count_frames() {
    this->add_on_frame_callback([this](std::span<const uint8_t>) { this->frames++; });
  }
  uint32_t frames{0};
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
      hub.count_frames();

      const std::vector<uint8_t> payload = {0x01, 0x02, 0x10, 0x03, 0x10, 0x10, 0x02};
      hub.feed_for_test(hub.build_frame_for_test(payload));

      EXPECT_EQ(hub.frames, 1u) << "escape " << int(escape_marker) << " type " << type;
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
  hub.count_frames();

  // DLE STX 01 02 crc(wrong) DLE ETX
  hub.feed_for_test({0x10, 0x02, 0x01, 0x02, 0x00, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 0u);

  // 9 bytes before a correct CRC, over max_frame_length.
  hub.feed_for_test(hub.build_frame_for_test({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}));
  EXPECT_EQ(hub.frames, 0u);

  // The framer recovers: a valid frame right after is delivered.
  hub.feed_for_test({0x10, 0x02, 0x01, 0x02, 0x03, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 1u);

  // More bytes than the receive buffer holds (8 + CRC), with no DLE ETX: reset, then recover.
  hub.feed_for_test({0x10, 0x02});
  hub.feed_for_test(std::vector<uint8_t>(30, 0x01));
  hub.feed_for_test({0x10, 0x02, 0x01, 0x02, 0x03, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 2u);
}

// max_frame_length counts unescaped bytes before the CRC on RX, as it does on TX: the largest
// frame the hub can send must also be accepted, even with every byte escaped.
TEST(RS485FrameHubTest, RxAcceptsLargestFrameTxCanSend) {
  for (uint8_t escape_marker : {uint8_t{0x00}, uint8_t{0x10}}) {
    RS485FrameHubProbe hub;
    hub.set_framing(0x10, 0x02, 0x03, escape_marker);
    hub.set_crc_type(CRC_TYPE_CRC16_MODBUS);
    hub.set_max_frame_length(8);
    hub.setup();
    hub.count_frames();

    const std::vector<uint8_t> payload(8, 0x10);
    hub.feed_for_test(hub.build_frame_for_test(payload));

    EXPECT_EQ(hub.frames, 1u) << "escape " << int(escape_marker);
    EXPECT_EQ(hub.rx_payload_for_test(), payload);
  }
}

// A DLE STX inside a frame can only be the next frame starting (the escape byte is never STX),
// so a frame that lost its DLE ETX must not take the following frame down with it.
TEST(RS485FrameHubTest, RxFramerRestartsOnDleStx) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_SUM8);
  hub.setup();
  hub.count_frames();

  hub.feed_for_test({0x10, 0x02, 0x01, 0x09, 0x05});
  const std::vector<uint8_t> payload = {0x01, 0x03, 0x41};
  hub.feed_for_test(hub.build_frame_for_test(payload));

  EXPECT_EQ(hub.frames, 1u);
  EXPECT_EQ(hub.rx_payload_for_test(), payload);
}

// dump_frames logs a frame longer than one log line without disturbing delivery.
TEST(RS485FrameHubTest, DumpFramesLogsLongFrames) {
  RS485FrameHubProbe hub;
  hub.set_crc_type(CRC_TYPE_SUM16_BIG_ENDIAN);
  hub.set_dump_frames(true);
  hub.setup();
  hub.count_frames();

  std::vector<uint8_t> payload(70);
  for (size_t i = 0; i < payload.size(); i++)
    payload[i] = static_cast<uint8_t>(i);
  hub.feed_for_test(hub.build_frame_for_test(payload));

  EXPECT_EQ(hub.frames, 1u);
  EXPECT_EQ(hub.rx_payload_for_test(), payload);
}

// A DLE followed by anything but STX, ETX or the escape marker drops the frame.
TEST(RS485FrameHubTest, RxFramerDropsBadEscape) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_NONE);
  hub.setup();
  hub.count_frames();

  hub.feed_for_test({0x10, 0x02, 0x01, 0x10, 0x05, 0x02, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 0u);

  const std::vector<uint8_t> payload = {0x01, 0x10, 0x02};
  hub.feed_for_test(hub.build_frame_for_test(payload));
  EXPECT_EQ(hub.frames, 1u);
  EXPECT_EQ(hub.rx_payload_for_test(), payload);
}

TEST(RS485FrameHubTest, QueueFrameFramesAndQueues) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_NONE);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x10, 0x02};

  EXPECT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);

  EXPECT_EQ(hub.queued_frame_for_test(0), hub.build_frame_for_test({0x01, 0x10, 0x02}));

  // replace_latest: a second frame replaces the first.
  static const uint8_t TYPE[] = {0x01, 0x02};
  static const uint8_t PAYLOAD[] = {0x03};
  EXPECT_TRUE(hub.queue_raw_frame(TYPE, PAYLOAD));
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  EXPECT_EQ(hub.queued_frame_for_test(0), hub.build_frame_for_test({0x01, 0x02, 0x03}));
}

// A max-length payload of DLE bytes doubles in size; framing it in place from the end of its
// slot must produce the same frame as framing it from a separate buffer.
TEST(RS485FrameHubTest, QueueFrameFramesWorstCaseInPlace) {
  for (uint8_t escape_marker : {uint8_t{0x00}, uint8_t{0x10}}) {
    RS485FrameHubProbe hub;
    hub.set_framing(0x10, 0x02, 0x03, escape_marker);
    hub.set_crc_type(CRC_TYPE_SUM16_BIG_ENDIAN);
    hub.set_max_frame_length(8);
    hub.setup();
    static const uint8_t DATA[8] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};

    EXPECT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
    const auto expected = hub.build_frame_for_test(std::vector<uint8_t>(DATA, DATA + sizeof(DATA)));
    EXPECT_EQ(expected.size(), 2 * sizeof(DATA) + 6u);
    EXPECT_EQ(hub.queued_frame_for_test(0), expected);
  }
}

TEST(RS485FrameHubTest, FifoQueueKeepsOrderAndDropsWhenFull) {
  RS485FrameHubProbe hub;
  hub.set_crc_type(CRC_TYPE_NONE);
  hub.set_queue_policy(QUEUE_FIFO);
  hub.set_max_queue_size(2);
  hub.setup();
  static const uint8_t FIRST[] = {0x01, 0x01};
  static const uint8_t SECOND[] = {0x01, 0x02, 0x10};

  EXPECT_TRUE(hub.queue_frame(FIRST, sizeof(FIRST)));
  EXPECT_TRUE(hub.queue_frame(SECOND, sizeof(SECOND)));
  EXPECT_FALSE(hub.queue_frame(FIRST, sizeof(FIRST)));
  EXPECT_EQ(hub.queue_depth_for_test(), 2u);
  EXPECT_EQ(hub.queued_frame_for_test(0), hub.build_frame_for_test({0x01, 0x01}));
  EXPECT_EQ(hub.queued_frame_for_test(1), hub.build_frame_for_test({0x01, 0x02, 0x10}));
}

TEST(RS485FrameHubTest, QueueFrameDropsOverlongAndSnifferFrames) {
  RS485FrameHubProbe hub;
  hub.set_max_frame_length(8);
  hub.setup();
  static const uint8_t DATA[9] = {};

  EXPECT_FALSE(hub.queue_frame(DATA, sizeof(DATA)));
  // Received frames under 2 bytes are dropped, so they are not sent either.
  EXPECT_FALSE(hub.queue_frame(DATA, 1));

  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

// A sniffer_only hub allocates no TX queue and never queues.
TEST(RS485FrameHubTest, SnifferHubDropsFrames) {
  RS485FrameHubProbe hub;
  hub.set_sniffer_only(true);
  hub.setup();
  static const uint8_t DATA[2] = {};

  EXPECT_FALSE(hub.queue_frame(DATA, sizeof(DATA)));
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_FALSE(hub.has_tx_queue_for_test());
}

TEST(RS485FrameHubTest, FrameCallbackReceivesPayload) {
  RS485FrameHubProbe hub;
  hub.set_framing(0x10, 0x02, 0x03, 0x00);
  hub.set_crc_type(CRC_TYPE_XOR8);
  hub.setup();
  std::vector<uint8_t> received;
  hub.add_on_frame_callback([&](std::span<const uint8_t> payload) { received.assign(payload.begin(), payload.end()); });

  const std::vector<uint8_t> payload = {0x01, 0x03, 0x41};
  hub.feed_for_test(hub.build_frame_for_test(payload));

  EXPECT_EQ(received, payload);
  static const uint8_t PREFIX[] = {0x01, 0x03};
  static const uint8_t OTHER[] = {0x01, 0x09};
  EXPECT_TRUE(frame_has_prefix(received, PREFIX, sizeof(PREFIX)));
  EXPECT_FALSE(frame_has_prefix(received, OTHER, sizeof(OTHER)));
}

// idle_gap on a bus that has carried no byte yet transmits once min_silence has passed since boot.
TEST(RS485FrameHubTest, IdleGapTransmitsOnQuietBus) {
  FakeUART uart;
  RS485FrameHubProbe hub;
  hub.set_uart_parent(&uart);
  hub.set_tx_gate_mode(TX_GATE_IDLE_GAP);
  hub.set_tx_idle_gap(20);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t boot = hub.last_activity_for_test();
  hub.maybe_tx_for_test(boot + 19);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.maybe_tx_for_test(boot + 20);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_FALSE(uart.written.empty());
}

// fixed_delay sends the first frame without waiting a full interval.
TEST(RS485FrameHubTest, FixedDelaySendsFirstFrameRightAway) {
  FakeUART uart;
  RS485FrameHubProbe hub;
  hub.set_uart_parent(&uart);
  hub.set_tx_gate_mode(TX_GATE_FIXED_DELAY);
  hub.set_tx_fixed_interval(100);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t now = millis();
  hub.maybe_tx_for_test(now);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);

  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  hub.maybe_tx_for_test(now + 99);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.maybe_tx_for_test(now + 100);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

// The gate delay holds the frame for delay ms, also across a millis() wrap.
TEST(RS485FrameHubTest, GateDelayHoldsFrameAcrossWrap) {
  FakeUART uart;
  RS485FrameHubProbe hub;
  hub.set_uart_parent(&uart);
  hub.set_tx_gate_delay(10);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  hub.send_next_for_test(0xFFFFFFF8);
  hub.loop_at_for_test(0x00000001);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(0x00000002);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

}  // namespace esphome::rs485_frame::testing
