#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/modbus_tcp/mbap.h"
#include "esphome/components/modbus_tcp/modbus_tcp.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

namespace {

class Pipe : public esphome::tcp_uart::TcpUart {
 public:
  bool is_connected() override { return true; }
  size_t available() override { return 0; }
  size_t available_for_write() override { return 1024; }
  bool read_array(uint8_t *, size_t) override { return false; }
  void write_array(const uint8_t *data, size_t len) override {
    ASSERT_LE(this->n_ + len, sizeof(this->buf_));
    std::memcpy(this->buf_ + this->n_, data, len);
    this->n_ += len;
  }
  esphome::uart::UARTFlushResult flush() override { return esphome::uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  size_t n_{0};
  uint8_t buf_[64]{};
};

class ServerLink : public esphome::modbus_tcp::ModbusTcp {
 public:
  ServerLink() { this->set_server(true); }

  void set_pipe(Pipe *pipe) { this->set_parent(pipe); }

  void load_raw(const uint8_t *data, size_t n) {
    std::memcpy(this->tcp_buf_ + this->tcp_len_, data, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
    this->deliver_mbap_();
  }

  void stage(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = esphome::modbus_tcp::write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    std::memcpy(this->tcp_buf_ + this->tcp_len_, frame, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
  }

  void push(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = esphome::modbus_tcp::write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    std::memcpy(this->tcp_buf_ + this->tcp_len_, frame, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
    this->deliver_mbap_();
  }

  void hold_reply(uint8_t n) { this->tx_len_ = n; }

  uint16_t txn() const { return this->txn_; }
  bool pending() const { return this->txn_pending_; }
  uint16_t held() const { return this->tx_len_; }
  void deliver() { this->deliver_mbap_(); }
};

const uint8_t PDU[] = {0x03, 0x00, 0x00, 0x00, 0x01};

TEST(ModbusTcpServer, UnansweredRequestIsReplaced) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  ASSERT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 7);
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  EXPECT_EQ(taken[0], 1);

  link.hold_reply(4);
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 8);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.held(), 0);
  EXPECT_EQ(link.available(), sizeof(PDU) + 3);
}

TEST(ModbusTcpServer, UnreadRequestIsKept) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint16_t waiting = link.available();
  link.push(8, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.txn(), 7);
  EXPECT_EQ(link.available(), waiting);
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
  link.deliver();
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.available(), waiting);
}

TEST(ModbusTcpServer, SameSegmentComesOutOneAtATime) {
  ServerLink link;
  link.stage(7, 1, PDU, sizeof(PDU));
  link.stage(8, 1, PDU, sizeof(PDU));
  link.stage(9, 1, PDU, sizeof(PDU));
  link.deliver();

  uint16_t one = link.available();
  EXPECT_EQ(one, sizeof(PDU) + 3);
  EXPECT_EQ(link.txn(), 7);

  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, one));
  link.deliver();
  EXPECT_EQ(link.txn(), 8);
  EXPECT_EQ(link.available(), one);

  ASSERT_TRUE(link.read_array(taken, one));
  link.deliver();
  EXPECT_EQ(link.txn(), 9);
  EXPECT_EQ(link.available(), one);

  ASSERT_TRUE(link.read_array(taken, one));
  link.deliver();
  EXPECT_EQ(link.available(), 0u);
}

TEST(ModbusTcpServer, ReplyWhileTheNewRequestIsBufferedIsDropped) {
  ServerLink link;
  link.push(7, 1, PDU, sizeof(PDU));
  uint8_t taken[16];
  ASSERT_TRUE(link.read_array(taken, link.available()));
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

TEST(ModbusTcpServer, WholeFrameReplacesAnIncompleteOne) {
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

TEST(ModbusTcpServer, SplitFrameIsAppended) {
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

TEST(ModbusTcpServer, ReplyUsesTheRequestTransaction) {
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

TEST(ModbusTcpServer, BroadcastIsNotAnswered) {
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

TEST(ModbusTcpServer, BadHeaderDropsTheStream) {
  ServerLink link;
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  link.load_raw(bad, sizeof(bad));
  EXPECT_EQ(link.available(), 0u);
  link.push(7, 1, PDU, sizeof(PDU));
  EXPECT_EQ(link.available(), 0u);
}

}  // namespace
