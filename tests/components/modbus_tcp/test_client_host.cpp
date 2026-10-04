#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/modbus_tcp/mbap.h"
#include "esphome/components/modbus_tcp/modbus_tcp.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

namespace {

class Pipe : public esphome::tcp_uart::TcpUart {
 public:
  bool is_connected() override { return this->up_; }
  size_t available() override { return 0; }
  size_t available_for_write() override { return this->room_; }
  bool read_array(uint8_t *, size_t) override { return false; }
  void write_array(const uint8_t *data, size_t len) override {
    ASSERT_LE(this->n_ + len, sizeof(this->buf_));
    std::memcpy(this->buf_ + this->n_, data, len);
    this->n_ += len;
  }
  esphome::uart::UARTFlushResult flush() override { return this->flushed_; }

  bool up_{true};
  size_t room_{1024};
  size_t n_{0};
  esphome::uart::UARTFlushResult flushed_{esphome::uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS};
  uint8_t buf_[64]{};
};

class ClientLink : public esphome::modbus_tcp::ModbusTcp {
 public:
  explicit ClientLink(Pipe *pipe) { this->set_parent(pipe); }

  void arm(uint16_t txn) {
    this->txn_ = txn;
    this->txn_pending_ = true;
  }

  void set_txn(uint16_t txn) { this->txn_ = txn; }

  void push(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
    uint8_t frame[32];
    size_t n = esphome::modbus_tcp::write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
    ASSERT_GT(n, 0u);
    std::memcpy(this->tcp_buf_ + this->tcp_len_, frame, n);
    this->tcp_len_ += static_cast<uint16_t>(n);
    this->deliver_mbap_();
  }

  void preload(const uint8_t *frame, size_t n) {
    std::memcpy(this->tx_, frame, n);
    this->tx_len_ = static_cast<uint16_t>(n);
  }

  uint16_t txn() const { return this->txn_; }
  bool pending() const { return this->txn_pending_; }
  uint16_t held() const { return this->tx_len_; }
  const uint8_t *held_bytes() const { return this->tx_; }
};

const uint8_t RESPONSE_PDU[] = {0x03, 0x02, 0x12, 0x34};
// 01 03 00 00 00 01 84 0A
const uint8_t RTU[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};

TEST(ModbusTcpClient, MatchingResponseBecomesRtu) {
  Pipe pipe;
  ClientLink link(&pipe);
  link.arm(7);
  link.push(7, 1, RESPONSE_PDU, sizeof(RESPONSE_PDU));

  uint8_t taken[16];
  size_t n = link.available();
  ASSERT_EQ(n, sizeof(RESPONSE_PDU) + 3);
  ASSERT_TRUE(link.read_array(taken, n));
  EXPECT_TRUE(esphome::modbus_tcp::rtu_crc_ok(taken, n));
  EXPECT_EQ(taken[0], 1);
  EXPECT_EQ(taken[1], 0x03);
  EXPECT_EQ(taken[2], 0x02);
  EXPECT_EQ(taken[3], 0x12);
  EXPECT_EQ(taken[4], 0x34);
  EXPECT_FALSE(link.pending());
}

TEST(ModbusTcpClient, StaleTransactionIsDropped) {
  Pipe pipe;
  ClientLink link(&pipe);
  link.arm(7);
  link.push(9, 1, RESPONSE_PDU, sizeof(RESPONSE_PDU));
  EXPECT_EQ(link.available(), 0u);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 7);
}

TEST(ModbusTcpClient, HeldFrameIsReplaced) {
  Pipe pipe;
  pipe.room_ = 0;
  ClientLink link(&pipe);
  link.preload(RTU, sizeof(RTU));

  uint8_t next[8] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};
  uint16_t crc = esphome::crc16(next, 6);
  next[6] = crc & 0xFF;
  next[7] = crc >> 8;
  link.write_array(next, sizeof(next));

  EXPECT_EQ(link.held(), sizeof(next));
  EXPECT_EQ(std::memcmp(link.held_bytes(), next, sizeof(next)), 0);
  EXPECT_EQ(pipe.n_, 0u);
}

TEST(ModbusTcpClient, TransactionWrapsToOne) {
  Pipe pipe;
  ClientLink link(&pipe);
  link.set_txn(0xFFFF);
  link.write_array(RTU, sizeof(RTU));

  ASSERT_GE(pipe.n_, 2u);
  EXPECT_EQ(pipe.buf_[0], 0x00);
  EXPECT_EQ(pipe.buf_[1], 0x01);
  EXPECT_EQ(link.txn(), 1);
  EXPECT_TRUE(link.pending());
}

}  // namespace
