#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "esphome/components/modbus_tcp_uart/mbap.h"
#include "esphome/components/modbus_tcp_uart/modbus_tcp_uart.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

#ifdef USE_HOST

namespace esphome::modbus_tcp_uart::testing {
namespace {

class Pipe : public tcp_uart::TcpUart {
 public:
  bool is_connected() override { return true; }
  size_t available() override { return this->rx_n_; }
  size_t available_for_write() override { return 1024; }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->fail_reads_ || len > this->rx_n_) {
      return false;
    }
    std::memcpy(data, this->rx_, len);
    this->rx_n_ -= len;
    std::memmove(this->rx_, this->rx_ + len, this->rx_n_);
    return true;
  }
  void write_array(const uint8_t *data, size_t len) override {
    ASSERT_LE(this->n_ + len, sizeof(this->buf_));
    std::memcpy(this->buf_ + this->n_, data, len);
    this->n_ += len;
  }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void feed(const uint8_t *data, size_t len) {
    ASSERT_LE(this->rx_n_ + len, sizeof(this->rx_));
    std::memcpy(this->rx_ + this->rx_n_, data, len);
    this->rx_n_ += len;
  }
  void feed_mbap(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    this->feed(frame, n);
  }

  bool fail_reads_{false};
  size_t n_{0};
  uint8_t buf_[300]{};
  size_t rx_n_{0};
  uint8_t rx_[64]{};
};

class ServerLink : public ModbusTcpUart {
 public:
  ServerLink() { this->set_server(true); }

  void set_pipe(Pipe *pipe) { this->set_parent(pipe); }

  void push(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    std::memcpy(this->tcp_buf_ + this->tcp_len_, frame, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
    this->deliver_mbap_();
  }

  void hold_reply(uint8_t n) { this->tx_len_ = n; }
  void expire() { this->request_ms_ -= REPLY_TIMEOUT_MS; }
  // Moves the start of the quiet period back, as if the stream had been quiet that long.
  void quiet_for(uint32_t us) { this->resync_from_us_ -= us; }

  // Writes an RTU frame with its CRC, the way the hub answers.
  void answer(std::initializer_list<uint8_t> body) {
    uint8_t frame[16];
    std::copy(body.begin(), body.end(), frame);
    uint16_t crc = crc16(frame, body.size());
    frame[body.size()] = crc & 0xFF;
    frame[body.size() + 1] = crc >> 8;
    this->write_array(frame, body.size() + 2);
  }

  uint16_t held_part() const { return this->tx_len_; }

  uint16_t txn() const { return this->txn_; }
  bool pending() const { return this->txn_pending_; }
  uint16_t held() const { return this->tx_len_; }
  void deliver() { this->deliver_mbap_(); }
  // As if the attached reader were still inside on_block(), so inject_rx() refuses the block.
  void set_in_rx_sink(bool in) { this->in_rx_sink_ = in; }
  bool unknown_unit_logged() const { return this->drop_log_ms_[DROP_UNKNOWN_UNIT] != 0; }
  bool read_failure_logged() const { return this->drop_log_ms_[DROP_READ] != 0; }
  bool rx_full_logged() const { return this->drop_log_ms_[DROP_RX_FULL] != 0; }
  bool replaced_logged() const { return this->drop_log_ms_[DROP_REPLACED] != 0; }
};

const uint8_t PDU[] = {0x03, 0x00, 0x00, 0x00, 0x01};
const uint8_t PDU_REG1[] = {0x03, 0x00, 0x01, 0x00, 0x01};

TEST(ModbusTcpUartServer, UnansweredRequestIsReplacedWhenOverdue) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  ASSERT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 7);
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  EXPECT_EQ(taken[0], 1);

  link.hold_reply(4);
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 7);
  EXPECT_EQ(link.available(), 0u);
  link.expire();
  link.deliver();
  EXPECT_EQ(link.txn(), 8);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.held(), 0);
  EXPECT_EQ(link.available(), sizeof(PDU) + 3);
}

TEST(ModbusTcpUartServer, RequestWithNoReplyIsLoggedWhenReplaced) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  // The hub never answers, not even in part.
  EXPECT_EQ(link.held(), 0);
  link.expire();
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 8);
  EXPECT_TRUE(link.replaced_logged());
}

TEST(ModbusTcpUartServer, UnreadRequestIsKept) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint16_t waiting = link.available();
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 7);
  EXPECT_EQ(link.available(), waiting);
}

TEST(ModbusTcpUartServer, UnreadRequestIsReplacedWhenOverdue) {
  // No hub and no attached reader: nothing reads the request.
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  link.expire();
  link.push(8, 1, PDU_REG1, sizeof(PDU_REG1));
  EXPECT_EQ(link.txn(), 8);
  EXPECT_TRUE(link.replaced_logged());
  uint8_t taken[16];
  ASSERT_EQ(link.available(), sizeof(PDU_REG1) + 3);
  ASSERT_TRUE(link.read_array(taken, link.available()));
  EXPECT_EQ(taken[3], 0x01);
}

TEST(ModbusTcpUartServer, PipelinedRequestsAreAnsweredInOrder) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  pipe.feed_mbap(0x0B01, 1, PDU, sizeof(PDU));
  pipe.feed_mbap(0x0B02, 1, PDU_REG1, sizeof(PDU_REG1));
  link.loop();
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  // Read but not answered yet. The second request waits.
  link.loop();
  EXPECT_EQ(link.available(), 0u);
  link.answer({0x01, 0x03, 0x02, 0x12, 0x34});
  link.loop();
  ASSERT_EQ(link.available(), sizeof(PDU_REG1) + 3);
  ASSERT_TRUE(link.read_array(taken, link.available()));
  EXPECT_EQ(taken[3], 0x01);
  link.answer({0x01, 0x03, 0x02, 0xBE, 0xEF});

  const uint8_t want[] = {0x0B, 0x01, 0, 0, 0, 5, 1, 0x03, 0x02, 0x12, 0x34,
                          0x0B, 0x02, 0, 0, 0, 5, 1, 0x03, 0x02, 0xBE, 0xEF};
  ASSERT_EQ(pipe.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(pipe.buf_, want, sizeof(want)), 0);
}

TEST(ModbusTcpUartServer, OnlyAFrameFromTheRequestUnitAndFunctionIsTheReply) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(0x0C02, 4, PDU_REG1, sizeof(PDU_REG1));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  // A late reply from unit 3, then a frame from unit 4 with another function.
  link.answer({0x03, 0x03, 0x02, 0x12, 0x34});
  link.answer({0x04, 0x06, 0x00, 0x01, 0x00, 0x55});
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_TRUE(link.pending());
  // An exception reply keeps the function with its top bit set.
  link.answer({0x04, 0x83, 0x02});
  const uint8_t want[] = {0x0C, 0x02, 0, 0, 0, 3, 4, 0x83, 0x02};
  ASSERT_EQ(pipe.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(pipe.buf_, want, sizeof(want)), 0);
}

// 01 03 02 12 34 B5 33, the reply to a one-register read.
const uint8_t REPLY_1234[] = {0x01, 0x03, 0x02, 0x12, 0x34, 0xB5, 0x33};

TEST(ModbusTcpUartServer, ReplyWhileTheNewRequestIsBufferedIsDropped) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.expire();
  link.push(8, 1, PDU, sizeof(PDU));
  ASSERT_GT(link.available(), 0u);

  // The late reply to 7 arrives while 8 is still unread: nothing goes out.
  link.write_array(REPLY_1234, sizeof(REPLY_1234));
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.held(), 0);

  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.write_array(REPLY_1234, sizeof(REPLY_1234));
  const uint8_t want[] = {0x00, 0x08, 0, 0, 0, 5, 1, 0x03, 0x02, 0x12, 0x34};
  ASSERT_EQ(pipe.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(pipe.buf_, want, sizeof(want)), 0);
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpUartServer, PiecesOfAReplyAreJoined) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));

  link.write_array(REPLY_1234, 4);
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_TRUE(link.pending());
  link.write_array(REPLY_1234 + 4, sizeof(REPLY_1234) - 4);
  const uint8_t want[] = {0x00, 0x07, 0, 0, 0, 5, 1, 0x03, 0x02, 0x12, 0x34};
  ASSERT_EQ(pipe.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(pipe.buf_, want, sizeof(want)), 0);
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpUartServer, LongReplyFromAWriterThatKeepsToTheRoom) {
  // 125 registers: a 255-byte reply, written the way uart_tcp writes, at most 128 bytes and never more than the room.
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  const uint8_t read_125[] = {0x03, 0x00, 0x00, 0x00, 125};
  link.push(9, 1, read_125, sizeof(read_125));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));

  uint8_t reply[255] = {0x01, 0x03, 250};
  for (size_t i = 3; i < 253; i++) {
    reply[i] = static_cast<uint8_t>(i);
  }
  const uint16_t crc = crc16(reply, 253);
  reply[253] = crc & 0xFF;
  reply[254] = crc >> 8;
  for (size_t sent = 0; sent < sizeof(reply);) {
    const size_t n = std::min<size_t>({sizeof(reply) - sent, 128, link.available_for_write()});
    ASSERT_GT(n, 0u);
    link.write_array(reply + sent, n);
    sent += n;
  }
  ASSERT_EQ(pipe.n_, 7u + 252u);
  EXPECT_EQ(pipe.buf_[1], 9);
  EXPECT_EQ(std::memcmp(pipe.buf_ + 7, reply + 1, 252), 0);
  EXPECT_EQ(link.held_part(), 0u);
}

// Answers each request it is handed within the same call, as a reader with a fast peer can.
class AnsweringReader : public uart::UARTSink {
 public:
  explicit AnsweringReader(ServerLink *link) : link_(link) {}
  void on_block(const uint8_t *data, size_t len) override {
    this->requests++;
    this->last_len = len;
    this->link_->answer({data[0], 0x03, 0x02, 0x12, 0x34});
  }

  int requests{0};
  size_t last_len{0};

 protected:
  ServerLink *link_;
};

TEST(ModbusTcpUartServer, AttachedReaderGetsTheRequestAndMayAnswerAtOnce) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  AnsweringReader reader(&link);
  link.set_rx_sink(&reader);
  link.push(0x0A0B, 1, PDU, sizeof(PDU));
  EXPECT_EQ(reader.requests, 1);
  EXPECT_EQ(reader.last_len, sizeof(PDU) + 3);
  EXPECT_EQ(link.available(), 0u);
  EXPECT_FALSE(link.pending());
  const uint8_t want[] = {0x0A, 0x0B, 0, 0, 0, 5, 1, 0x03, 0x02, 0x12, 0x34};
  ASSERT_EQ(pipe.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(pipe.buf_, want, sizeof(want)), 0);
}

TEST(ModbusTcpUartServer, ReplyUsesTheRequestTransaction) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(0x1234, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.write_array(REPLY_1234, sizeof(REPLY_1234));
  ASSERT_GE(pipe.n_, 7u);
  EXPECT_EQ(pipe.buf_[0], 0x12);
  EXPECT_EQ(pipe.buf_[1], 0x34);
  EXPECT_EQ(pipe.buf_[6], 1);
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpUartServer, BroadcastIsNotAnswered) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(5, 0, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.write_array(REPLY_1234, sizeof(REPLY_1234));
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpUartServer, RequestToAUnitNoServerAnswersIsDropped) {
  static const uint8_t UNITS[] = {1};
  const uint8_t write[] = {0x06, 0x00, 0x01, 0x00, 0x55};
  ServerLink link;
  link.set_units(UNITS, sizeof(UNITS));
  // A broadcast still reaches the hub.
  link.push(3, 0, write, sizeof(write));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  EXPECT_FALSE(link.unknown_unit_logged());
  link.push(4, 5, PDU, sizeof(PDU));
  EXPECT_EQ(link.available(), 0u);
  EXPECT_FALSE(link.pending());
  EXPECT_TRUE(link.unknown_unit_logged());
  link.push(5, 1, write, sizeof(write));
  EXPECT_EQ(link.txn(), 5);
  EXPECT_EQ(link.available(), sizeof(write) + 3);
}

TEST(ModbusTcpUartServer, RefusedRequestDoesNotBlockTheNext) {
  ServerLink link;
  AnsweringReader reader(&link);
  link.set_rx_sink(&reader);
  link.set_in_rx_sink(true);
  link.push(7, 1, PDU, sizeof(PDU));
  EXPECT_EQ(reader.requests, 0);
  EXPECT_FALSE(link.pending());
  EXPECT_TRUE(link.rx_full_logged());
  // The next request goes to the reader at once, without waiting for REPLY_TIMEOUT_MS.
  link.set_in_rx_sink(false);
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(reader.requests, 1);
  EXPECT_EQ(link.txn(), 8);
}

TEST(ModbusTcpUartServer, FailedReadWhileResyncingIsLogged) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x03};
  pipe.feed(bad, sizeof(bad));
  link.loop();
  EXPECT_FALSE(link.read_failure_logged());
  pipe.feed_mbap(7, 1, PDU, sizeof(PDU));
  pipe.fail_reads_ = true;
  link.loop();
  EXPECT_TRUE(link.read_failure_logged());
  EXPECT_EQ(link.available(), 0u);
}

TEST(ModbusTcpUartServer, BadProtocolIdSkipsOnlyThatFrame) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  // Protocol id 1 with a usable length, then a valid request in the same segment.
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  pipe.feed(bad, sizeof(bad));
  pipe.feed_mbap(7, 1, PDU, sizeof(PDU));
  link.loop();
  EXPECT_EQ(link.txn(), 7);
  EXPECT_EQ(link.available(), sizeof(PDU) + 3);
}

TEST(ModbusTcpUartServer, BadLengthWaitsForAQuietStream) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  // Length 0x0100 is over 254, so nothing says where the frame ends.
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x03};
  pipe.feed(bad, sizeof(bad));
  link.loop();
  // Still inside the quiet period: this request is dropped with the rest of the stream.
  pipe.feed_mbap(7, 1, PDU, sizeof(PDU));
  link.loop();
  EXPECT_EQ(link.available(), 0u);
  link.quiet_for(120000);
  link.loop();
  pipe.feed_mbap(8, 1, PDU, sizeof(PDU));
  link.loop();
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.available(), sizeof(PDU) + 3);
}

}  // namespace
}  // namespace esphome::modbus_tcp_uart::testing

#endif  // USE_HOST
