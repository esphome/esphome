// Covers the CRC engine, the RX framer and the TX queue without a real UART.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include "esphome/components/rs485_frame/rs485_frame.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

namespace esphome::rs485_frame::testing {

namespace {

// Records what the hub writes and hands it the bytes queued in rx.
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
    // Stands in for a UART debug callback that queues a frame mid-send; runs once.
    if (auto hook = std::move(this->on_write)) {
      this->on_write = nullptr;
      hook();
    }
  }
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size())
      return false;
    std::copy_n(this->rx.begin(), len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  std::vector<uint8_t> written;
  std::vector<uint8_t> rx;
  std::function<void()> on_write;
};

struct HubConfig {
  uint8_t escape_marker{0x00};
  CrcType crc_type{CRC_TYPE_SUM16_BIG_ENDIAN};
  uint16_t max_frame_length{128};
  uint8_t max_queue_size{1};
  bool sniffer_only{false};
};

// Stand-ins for the static arrays codegen declares per hub, sized for every test here.
struct ProbeStorage {
  uint8_t rx_buffer[256];
  uint8_t tx_buffer[512];
  uint16_t tx_len_buffer[4];
  FakeUART fake_uart;
};

class RS485FrameHubProbe : public ProbeStorage, public RS485FrameHub {
 public:
  explicit RS485FrameHubProbe(HubConfig c = {})
      : RS485FrameHub(0x10, 0x02, 0x03, c.escape_marker, c.crc_type, c.max_frame_length, c.max_queue_size,
                      this->rx_buffer, c.sniffer_only ? nullptr : this->tx_buffer,
                      c.sniffer_only ? nullptr : this->tx_len_buffer) {
    this->set_uart_parent(&this->fake_uart);
  }
  std::vector<uint8_t> crc_for_test(const std::vector<uint8_t> &data, bool include_header) const {
    std::vector<uint8_t> out(this->crc_len_);
    this->encode_crc_(data.data(), data.size(), include_header, out.data());
    return out;
  }
  // The wire frame for payload with a header-inclusive CRC, framed independently of transmit_head_().
  std::vector<uint8_t> build_frame_for_test(const std::vector<uint8_t> &payload) const {
    std::vector<uint8_t> data = payload;
    for (uint8_t b : this->crc_for_test(payload, true))
      data.push_back(b);
    std::vector<uint8_t> out = {this->dle_, this->stx_};
    for (uint8_t b : data) {
      out.push_back(b);
      if (b == this->dle_)
        out.push_back(this->escape_marker_);
    }
    out.push_back(this->dle_);
    out.push_back(this->etx_);
    return out;
  }
  // What dump_frames logs for an unescaped frame.
  std::vector<uint8_t> logged_wire_for_test(const std::vector<uint8_t> &frame) const {
    std::vector<uint8_t> out;
    this->encode_wire_(frame.data(), frame.size(), [&](uint8_t b) { out.push_back(b); });
    return out;
  }
  void feed_for_test(const std::vector<uint8_t> &bytes) {
    for (uint8_t b : bytes)
      this->handle_rx_byte_(b, 0);
  }
  std::vector<uint8_t> rx_payload_for_test() const {
    return {this->rx_payload_, this->rx_payload_ + this->rx_payload_len_};
  }
  // Sends the head frame and returns the bytes written to the UART.
  std::vector<uint8_t> transmit_for_test() {
    this->fake_uart.written.clear();
    this->transmit_head_(0);
    return this->fake_uart.written;
  }
  void send_next_for_test(uint32_t now) { this->send_next_(now); }
  void loop_at_for_test(uint32_t now, const std::vector<uint8_t> &rx = {}) {
    this->fake_uart.rx = rx;
    LoopBlockingGuard guard{nullptr, nullptr, now};
    this->loop();
  }
  uint32_t last_activity_for_test() const { return this->last_activity_time_; }
  size_t queue_depth_for_test() const { return this->tx_queue_count_; }
  // Counts the frames delivered to on_frame callbacks.
  void count_frames() {
    this->add_on_frame_callback([this](std::span<const uint8_t>) { this->frames++; });
  }
  uint32_t frames{0};
};

}  // namespace

// Published check values for "123456789", in wire order, without and with the DLE STX header.
TEST(RS485FrameHubTest, CrcCheckValues) {
  struct CrcCase {
    CrcType type;
    std::vector<uint8_t> payload_only;
    std::vector<uint8_t> header_inclusive;
  };
  const std::vector<CrcCase> cases = {
      {CRC_TYPE_NONE, {}, {}},
      {CRC_TYPE_SUM8, {0xDD}, {0xEF}},
      {CRC_TYPE_SUM16_BIG_ENDIAN, {0x01, 0xDD}, {0x01, 0xEF}},
      {CRC_TYPE_SUM16_LITTLE_ENDIAN, {0xDD, 0x01}, {0xEF, 0x01}},
      {CRC_TYPE_XOR8, {0x31}, {0x23}},
      {CRC_TYPE_CRC16_MODBUS, {0x37, 0x4B}, {0xCE, 0x55}},
  };
  const std::vector<uint8_t> check = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  for (const auto &c : cases) {
    RS485FrameHubProbe hub({.crc_type = c.type});
    EXPECT_EQ(hub.crc_for_test(check, false), c.payload_only) << "type " << c.type;
    EXPECT_EQ(hub.crc_for_test(check, true), c.header_inclusive) << "type " << c.type;
  }
}

// A frame built by the TX encoder must come back through the RX framer as the same payload,
// for both escape modes and with DLE bytes in the payload and the CRC.
TEST(RS485FrameHubTest, RxFramerRoundTripsBuiltFrames) {
  for (uint8_t escape_marker : {uint8_t{0x00}, uint8_t{0x10}}) {
    for (CrcType type : {CRC_TYPE_SUM16_BIG_ENDIAN, CRC_TYPE_CRC16_MODBUS, CRC_TYPE_XOR8, CRC_TYPE_NONE}) {
      RS485FrameHubProbe hub({.escape_marker = escape_marker, .crc_type = type});
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
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_SUM8, .max_frame_length = 8});
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

// A DLE STX inside a frame can only be the next frame starting (the escape byte is never STX),
// so a frame that lost its DLE ETX must not take the following frame down with it.
TEST(RS485FrameHubTest, RxFramerRestartsOnDleStx) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_SUM8});
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

// dump_frames logs the wire bytes the docs describe: DLE STX, the escaped frame and CRC, DLE ETX.
TEST(RS485FrameHubTest, DumpFramesLogsWireBytes) {
  RS485FrameHubProbe hub;
  hub.setup();
  const std::vector<uint8_t> payload = {0x01, 0x10, 0x02};
  std::vector<uint8_t> frame = payload;
  for (uint8_t b : hub.crc_for_test(payload, true))
    frame.push_back(b);
  EXPECT_EQ(hub.logged_wire_for_test(frame), hub.build_frame_for_test(payload));
}

// A DLE followed by anything but STX, ETX or the escape marker drops the frame.
TEST(RS485FrameHubTest, RxFramerDropsBadEscape) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_NONE});
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
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_NONE});
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x10, 0x02};
  EXPECT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  // replace_latest: a second frame replaces the first.
  static const uint8_t TYPE[] = {0x01, 0x02};
  static const uint8_t PAYLOAD[] = {0x03};
  EXPECT_TRUE(hub.queue_raw_frame(TYPE, PAYLOAD));
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  EXPECT_EQ(hub.transmit_for_test(), hub.build_frame_for_test({0x01, 0x02, 0x03}));
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

// The largest frame, all DLE bytes, doubles in size and is escaped across several stack chunks;
// it must leave as the same bytes an independent encoder builds and come back through the RX framer.
TEST(RS485FrameHubTest, TransmitEscapesLargestFrame) {
  for (uint8_t escape_marker : {uint8_t{0x00}, uint8_t{0x10}}) {
    RS485FrameHubProbe hub({.escape_marker = escape_marker, .crc_type = CRC_TYPE_CRC16_MODBUS, .max_frame_length = 64});
    hub.setup();
    hub.count_frames();
    const std::vector<uint8_t> payload(64, 0x10);

    ASSERT_TRUE(hub.queue_frame(payload.data(), payload.size()));
    const auto wire = hub.transmit_for_test();
    EXPECT_EQ(wire, hub.build_frame_for_test(payload));
    EXPECT_GE(wire.size(), 2 * payload.size() + 6u);

    hub.feed_for_test(wire);
    EXPECT_EQ(hub.frames, 1u) << "escape " << int(escape_marker);
    EXPECT_EQ(hub.rx_payload_for_test(), payload);
  }
}

// A frame queued from a callback while replace_latest's only slot is sent must not change the bytes in flight.
TEST(RS485FrameHubTest, ReplaceLatestKeepsFrameInFlight) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_NONE, .max_frame_length = 64});
  hub.setup();
  const std::vector<uint8_t> first(40, 0x01);
  static const uint8_t SECOND[] = {0x02, 0x02};
  ASSERT_TRUE(hub.queue_frame(first.data(), first.size()));
  bool queued = true;
  hub.fake_uart.on_write = [&]() { queued = hub.queue_frame(SECOND, sizeof(SECOND)); };

  EXPECT_EQ(hub.transmit_for_test(), hub.build_frame_for_test(first));
  EXPECT_FALSE(queued);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

TEST(RS485FrameHubTest, FifoQueueKeepsOrderAndDropsWhenFull) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_NONE, .max_queue_size = 2});
  hub.set_queue_policy(QUEUE_FIFO);
  hub.setup();
  static const uint8_t FIRST[] = {0x01, 0x01};
  static const uint8_t SECOND[] = {0x01, 0x02, 0x10};

  EXPECT_TRUE(hub.queue_frame(FIRST, sizeof(FIRST)));
  EXPECT_TRUE(hub.queue_frame(SECOND, sizeof(SECOND)));
  EXPECT_FALSE(hub.queue_frame(FIRST, sizeof(FIRST)));
  EXPECT_EQ(hub.queue_depth_for_test(), 2u);
  EXPECT_EQ(hub.transmit_for_test(), hub.build_frame_for_test({0x01, 0x01}));
  // The ring wraps: the freed slot takes the next frame behind SECOND.
  EXPECT_TRUE(hub.queue_frame(FIRST, sizeof(FIRST)));
  EXPECT_EQ(hub.transmit_for_test(), hub.build_frame_for_test({0x01, 0x02, 0x10}));
  EXPECT_EQ(hub.transmit_for_test(), hub.build_frame_for_test({0x01, 0x01}));
}

// Frames outside 2..max_frame_length are dropped (received frames under 2 bytes are dropped
// too), and a sniffer_only hub has no TX queue and never queues.
TEST(RS485FrameHubTest, QueueFrameDropsInvalidFramesAndSnifferFrames) {
  static const uint8_t DATA[9] = {};
  RS485FrameHubProbe hub({.max_frame_length = 8});
  hub.setup();
  EXPECT_FALSE(hub.queue_frame(DATA, sizeof(DATA)));
  EXPECT_FALSE(hub.queue_frame(DATA, 1));
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);

  RS485FrameHubProbe sniffer({.sniffer_only = true});
  sniffer.setup();
  EXPECT_FALSE(sniffer.queue_frame(DATA, 2));
  EXPECT_EQ(sniffer.queue_depth_for_test(), 0u);
}

TEST(RS485FrameHubTest, FrameCallbackReceivesPayload) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_XOR8});
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
  RS485FrameHubProbe hub;
  hub.set_tx_gate_mode(TX_GATE_IDLE_GAP);
  hub.set_tx_gate_interval(20);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t boot = hub.last_activity_for_test();
  hub.loop_at_for_test(boot + 19);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(boot + 20);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_FALSE(hub.fake_uart.written.empty());
}

// fixed_delay sends the first frame without waiting a full interval.
TEST(RS485FrameHubTest, FixedDelaySendsFirstFrameRightAway) {
  RS485FrameHubProbe hub;
  hub.set_tx_gate_mode(TX_GATE_FIXED_DELAY);
  hub.set_tx_gate_interval(100);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t now = millis();
  hub.loop_at_for_test(now);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);

  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  hub.loop_at_for_test(now + 99);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(now + 100);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

// fixed_delay counts the interval from when the previous frame has left the wire.
TEST(RS485FrameHubTest, FixedDelayWaitsForTheFrameOnTheWire) {
  RS485FrameHubProbe hub;
  hub.fake_uart.set_baud_rate(9600);
  hub.fake_uart.set_data_bits(8);
  hub.fake_uart.set_stop_bits(1);
  hub.set_tx_gate_mode(TX_GATE_FIXED_DELAY);
  hub.set_tx_gate_interval(1);
  hub.setup();
  static const uint8_t DATA[48] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t now = millis();
  hub.loop_at_for_test(now);
  ASSERT_EQ(hub.queue_depth_for_test(), 0u);
  // 10 bits per byte at 9600 baud
  const uint32_t wire_ms = (hub.fake_uart.written.size() * 10 * 1000 + 9599) / 9600;
  ASSERT_GT(wire_ms, 1u);

  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  hub.loop_at_for_test(now + wire_ms);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(now + wire_ms + 1);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
}

// idle_gap rechecks min_silence when the gate delay ends, so RX during the delay holds the frame.
TEST(RS485FrameHubTest, IdleGapDelayWaitsForSilenceAfterRx) {
  RS485FrameHubProbe hub;
  hub.set_tx_gate_mode(TX_GATE_IDLE_GAP);
  hub.set_tx_gate_interval(20);
  hub.set_tx_gate_delay(10);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  const uint32_t boot = hub.last_activity_for_test();
  hub.loop_at_for_test(boot + 20);
  // A byte from another node just before the delay expires
  hub.loop_at_for_test(boot + 29, {0x10, 0x02, 0x01});
  hub.loop_at_for_test(boot + 30);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(boot + 48);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(boot + 49);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_FALSE(hub.fake_uart.written.empty());
}

// frame_trigger answers only in the gap right after the gate frame: bytes that follow it in the same
// receive batch mean another node already took the gap, so the frame waits for the next gate frame.
TEST(RS485FrameHubTest, FrameTriggerYieldsToRxAfterGateFrame) {
  RS485FrameHubProbe hub;
  static const uint8_t GATE[] = {0x01, 0x01};
  hub.set_tx_gate_frame_type(GATE, sizeof(GATE));
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  std::vector<uint8_t> rx = hub.build_frame_for_test({0x01, 0x01});
  rx.insert(rx.end(), {0x10, 0x02, 0x05});
  hub.loop_at_for_test(1000, rx);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  EXPECT_TRUE(hub.fake_uart.written.empty());

  hub.loop_at_for_test(1010, hub.build_frame_for_test({0x01, 0x01}));
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_EQ(hub.fake_uart.written, hub.build_frame_for_test({0x01, 0x02}));
}

// A reply the gate frame's on_frame callback queues into an empty queue is sent in that gap.
TEST(RS485FrameHubTest, FrameTriggerSendsReplyQueuedByCallback) {
  RS485FrameHubProbe hub;
  static const uint8_t GATE[] = {0x01, 0x01};
  hub.set_tx_gate_frame_type(GATE, sizeof(GATE));
  hub.setup();
  static const uint8_t REPLY[] = {0x01, 0x02};
  hub.add_on_frame_callback([&hub](std::span<const uint8_t>) { hub.queue_frame(REPLY, sizeof(REPLY)); });

  hub.loop_at_for_test(1000, hub.build_frame_for_test({0x01, 0x01}));
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_EQ(hub.fake_uart.written, hub.build_frame_for_test({0x01, 0x02}));
}

// read_available() reads only what was waiting when it started, so bytes that arrive while on_frame runs are
// still in the UART; they mean another node took the gap, and are read before the hub would send.
TEST(RS485FrameHubTest, FrameTriggerWaitsForBytesThatArriveDuringTheCallback) {
  RS485FrameHubProbe hub;
  static const uint8_t GATE[] = {0x01, 0x01};
  hub.set_tx_gate_frame_type(GATE, sizeof(GATE));
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));
  hub.add_on_frame_callback([&hub](std::span<const uint8_t>) {
    if (hub.fake_uart.rx.empty())
      hub.fake_uart.rx.insert(hub.fake_uart.rx.end(), {0x10, 0x02});
  });

  hub.loop_at_for_test(1000, hub.build_frame_for_test({0x01, 0x01}));
  EXPECT_TRUE(hub.fake_uart.written.empty());
  EXPECT_EQ(hub.fake_uart.rx, (std::vector<uint8_t>{0x10, 0x02}));
  hub.loop_at_for_test(1010, std::vector<uint8_t>(hub.fake_uart.rx));
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  EXPECT_TRUE(hub.fake_uart.written.empty());
}

// RX during the frame_trigger gate delay gives the gap up; the frame waits for the next gate frame.
TEST(RS485FrameHubTest, FrameTriggerDelayCancelledByRx) {
  RS485FrameHubProbe hub;
  static const uint8_t GATE[] = {0x01, 0x01};
  hub.set_tx_gate_frame_type(GATE, sizeof(GATE));
  hub.set_tx_gate_delay(10);
  hub.setup();
  static const uint8_t DATA[] = {0x01, 0x02};
  ASSERT_TRUE(hub.queue_frame(DATA, sizeof(DATA)));

  hub.loop_at_for_test(1000, hub.build_frame_for_test({0x01, 0x01}));
  hub.loop_at_for_test(1005, {0x10, 0x02});
  hub.loop_at_for_test(1010);
  hub.loop_at_for_test(1100);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  EXPECT_TRUE(hub.fake_uart.written.empty());

  hub.loop_at_for_test(1200, hub.build_frame_for_test({0x01, 0x01}));
  hub.loop_at_for_test(1209);
  EXPECT_EQ(hub.queue_depth_for_test(), 1u);
  hub.loop_at_for_test(1210);
  EXPECT_EQ(hub.queue_depth_for_test(), 0u);
  EXPECT_EQ(hub.fake_uart.written, hub.build_frame_for_test({0x01, 0x02}));
}

// A partial frame that timed out is dropped before the next bytes are read, so a late continuation
// cannot complete it.
TEST(RS485FrameHubTest, RxTimeoutExpiresPartialFrameBeforeNextBytes) {
  RS485FrameHubProbe hub({.crc_type = CRC_TYPE_NONE});
  hub.set_in_frame_timeout(50);
  hub.setup();
  hub.count_frames();

  hub.loop_at_for_test(1000, {0x10, 0x02, 0x01});
  hub.loop_at_for_test(1050, {0x02, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 0u);

  // Within the timeout the continuation completes the frame.
  hub.loop_at_for_test(2000, {0x10, 0x02, 0x01});
  hub.loop_at_for_test(2049, {0x02, 0x10, 0x03});
  EXPECT_EQ(hub.frames, 1u);
}

// The gate delay holds the frame for delay ms, also across a millis() wrap.
TEST(RS485FrameHubTest, GateDelayHoldsFrameAcrossWrap) {
  RS485FrameHubProbe hub;
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
