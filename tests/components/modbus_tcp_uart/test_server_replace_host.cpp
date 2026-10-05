#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <thread>

#include "esphome/components/modbus_tcp_uart/mbap.h"
#include "esphome/components/modbus_tcp_uart/modbus_tcp_uart.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

namespace {

class Pipe : public esphome::tcp_uart::TcpUart {
 public:
  bool is_connected() override { return true; }
  size_t available() override { return this->rx_n_; }
  size_t available_for_write() override { return 1024; }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_n_) {
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
  esphome::uart::UARTFlushResult flush() override { return esphome::uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void feed(const uint8_t *data, size_t len) {
    ASSERT_LE(this->rx_n_ + len, sizeof(this->rx_));
    std::memcpy(this->rx_ + this->rx_n_, data, len);
    this->rx_n_ += len;
  }
  void feed_mbap(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = esphome::modbus_tcp_uart::write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    this->feed(frame, n);
  }

  size_t n_{0};
  uint8_t buf_[64]{};
  size_t rx_n_{0};
  uint8_t rx_[64]{};
};

class ServerLink : public esphome::modbus_tcp_uart::ModbusTcpUart {
 public:
  ServerLink() { this->set_server(true); }

  void set_pipe(Pipe *pipe) { this->set_parent(pipe); }

  void push(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = esphome::modbus_tcp_uart::write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    std::memcpy(this->tcp_buf_ + this->tcp_len_, frame, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
    this->deliver_mbap_();
  }

  void hold_reply(uint8_t n) { this->tx_len_ = n; }
  void expire() { this->request_ms_ -= REPLY_TIMEOUT_MS; }

  // Writes an RTU frame with its CRC, the way the hub answers.
  void answer(std::initializer_list<uint8_t> body) {
    uint8_t frame[16];
    std::copy(body.begin(), body.end(), frame);
    uint16_t crc = esphome::crc16(frame, body.size());
    frame[body.size()] = crc & 0xFF;
    frame[body.size() + 1] = crc >> 8;
    this->write_array(frame, body.size() + 2);
  }

  uint16_t txn() const { return this->txn_; }
  bool pending() const { return this->txn_pending_; }
  uint16_t held() const { return this->tx_len_; }
  void deliver() { this->deliver_mbap_(); }
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

TEST(ModbusTcpUartServer, UnreadRequestIsKept) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint16_t waiting = link.available();
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 7);
  EXPECT_EQ(link.available(), waiting);
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

TEST(ModbusTcpUartServer, ReplyWhileTheNewRequestIsBufferedIsDropped) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.expire();
  link.push(8, 1, PDU, sizeof(PDU));
  ASSERT_GT(link.available(), 0u);

  // 01 03 00 00 00 01 84 0A, a complete RTU frame.
  const uint8_t reply[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  link.write_array(reply, sizeof(reply));
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.held(), 0);

  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.write_array(reply, sizeof(reply));
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpUartServer, WholeFrameReplacesAnIncompleteOne) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));

  const uint8_t junk[] = {0x01, 0x03, 0x00};
  const uint8_t reply[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  link.write_array(junk, sizeof(junk));
  EXPECT_EQ(link.held(), sizeof(junk));
  link.write_array(reply, sizeof(reply));
  EXPECT_EQ(link.held(), 0u);
}

TEST(ModbusTcpUartServer, SplitFrameIsAppended) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));

  const uint8_t reply[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  link.write_array(reply, 4);
  link.write_array(reply + 4, 4);
  EXPECT_EQ(link.held(), sizeof(reply));
}

const uint8_t REPLY[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};

TEST(ModbusTcpUartServer, ReplyUsesTheRequestTransaction) {
  Pipe pipe;
  ServerLink link;
  link.set_pipe(&pipe);
  link.push(0x1234, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.write_array(REPLY, sizeof(REPLY));
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
  link.write_array(REPLY, sizeof(REPLY));
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
  link.push(4, 5, PDU, sizeof(PDU));
  EXPECT_EQ(link.available(), 0u);
  EXPECT_FALSE(link.pending());
  link.push(5, 1, write, sizeof(write));
  EXPECT_EQ(link.txn(), 5);
  EXPECT_EQ(link.available(), sizeof(write) + 3);
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
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  link.loop();
  pipe.feed_mbap(8, 1, PDU, sizeof(PDU));
  link.loop();
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.available(), sizeof(PDU) + 3);
}

}  // namespace
