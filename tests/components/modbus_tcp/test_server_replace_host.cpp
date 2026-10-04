#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/modbus_tcp/mbap.h"
#include "esphome/components/modbus_tcp/modbus_tcp.h"

namespace {

class ServerLink : public esphome::modbus_tcp::ModbusTcp {
 public:
  ServerLink() { this->set_server(true); }

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

}  // namespace
