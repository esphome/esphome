#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/mbap.h"
#include "esphome/components/modbus/mbap_link.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {
namespace {

class Pipe : public NullUART {
 public:
  bool is_connected() override { return this->up; }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return this->room; }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty()) {
      return false;
    }
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size()) {
      return false;
    }
    std::memcpy(data, this->rx.data(), len);
    this->rx.erase(this->rx.begin(), this->rx.begin() + static_cast<std::ptrdiff_t>(len));
    return true;
  }
  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
  uart::UARTFlushResult flush() override { return this->flushed; }

  bool up{true};
  size_t room{1024};
  uart::UARTFlushResult flushed{uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS};
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
};

class TestLink : public MbapLink {
 public:
  using MbapLink::MbapLink;
  void set_txn(uint16_t txn) { this->txn_ = txn; }
  uint16_t txn() const { return this->txn_; }
  bool pending() const { return this->txn_pending_; }
  uint16_t held() const { return this->held_len_; }
  bool resyncing() const { return this->resync_; }
};

std::vector<uint8_t> rtu(uint8_t address, std::initializer_list<uint8_t> pdu) {
  std::vector<uint8_t> frame{address};
  frame.insert(frame.end(), pdu.begin(), pdu.end());
  uint16_t crc = crc16(frame.data(), static_cast<uint16_t>(frame.size()));
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}

// A frame longer than a short UART queue: the given head, filler bytes, then the CRC.
std::vector<uint8_t> long_rtu(std::initializer_list<uint8_t> head, size_t filler) {
  std::vector<uint8_t> frame(head);
  frame.resize(frame.size() + filler, 0xA5);
  uint16_t crc = crc16(frame.data(), static_cast<uint16_t>(frame.size()));
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}

std::vector<uint8_t> as_mbap(uint16_t txn, const std::vector<uint8_t> &rtu) {
  std::vector<uint8_t> frame(260);
  frame.resize(write_mbap(frame.data(), frame.size(), txn, rtu[0], rtu.data() + 1, rtu.size() - 3));
  return frame;
}

// Each pump() writes up to pipe->room bytes, like a short TX FIFO that empties between loops.
void pump_until_sent(TestLink *link, Pipe *pipe) {
  for (int i = 0; i < 16 && link->held() != 0; i++) {
    link->pump(pipe);
  }
}

void push_mbap(Pipe *pipe, uint16_t txn, uint8_t unit, std::initializer_list<uint8_t> pdu) {
  uint8_t pdu_bytes[16];
  size_t n = 0;
  for (uint8_t byte : pdu) {
    pdu_bytes[n++] = byte;
  }
  uint8_t frame[32];
  size_t len = write_mbap(frame, sizeof(frame), txn, unit, pdu_bytes, n);
  ASSERT_GT(len, 0u);
  pipe->rx.insert(pipe->rx.end(), frame, frame + len);
}

size_t pull(TestLink *link, Pipe *pipe, uint8_t *dst, size_t cap) {
  link->pump(pipe);
  return link->take_rtu(dst, cap);
}

const uint8_t READ_REQ[] = {0x03, 0x00, 0x00, 0x00, 0x01};
const uint8_t READ_RSP[] = {0x03, 0x02, 0x12, 0x34};
const auto RTU_REQ = rtu(0x01, {0x03, 0x00, 0x00, 0x00, 0x01});

TEST(MbapLink, RoundTrip) {
  uint8_t frame[16];
  size_t n = write_mbap(frame, sizeof(frame), 0x1234, 0x11, READ_REQ, sizeof(READ_REQ));
  ASSERT_EQ(n, 7u + sizeof(READ_REQ));
  Mbap out;
  size_t used = 0;
  ASSERT_EQ(take_mbap(frame, n, &out, &used), MbapTake::MBAP_TAKE_FRAME);
  EXPECT_EQ(out.txn, 0x1234);
  EXPECT_EQ(out.unit, 0x11);
  EXPECT_EQ(out.pdu_len, sizeof(READ_REQ));
}

TEST(MbapLinkClient, MatchingResponseBecomesRtu) {
  Pipe pipe;
  TestLink link(false);
  link.set_txn(7);
  // Arm pending the way a completed send does. set_txn alone is not a request.
  const auto req = RTU_REQ;
  // txn_pending_ is protected. Sending a frame arms it, then we roll the id back to 7
  // by sending is the wrong id. Push after a real send below.
  pipe.tx.clear();
  link.send_rtu(&pipe, req.data(), req.size());
  ASSERT_TRUE(link.pending());
  uint16_t txn = link.txn();
  push_mbap(&pipe, txn, 1, {0x03, 0x02, 0x12, 0x34});
  uint8_t taken[16];
  size_t n = pull(&link, &pipe, taken, sizeof(taken));
  ASSERT_EQ(n, sizeof(READ_RSP) + 3);
  EXPECT_TRUE(rtu_crc_ok(taken, n));
  EXPECT_EQ(taken[0], 1);
  EXPECT_EQ(taken[1], READ_RSP[0]);
  EXPECT_EQ(taken[3], READ_RSP[2]);
  EXPECT_FALSE(link.pending());
}

TEST(MbapLinkClient, ReplyTakesTheUnitOfTheRequest) {
  Pipe pipe;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  push_mbap(&pipe, link.txn(), 0xFF, {0x03, 0x02, 0x12, 0x34});
  uint8_t taken[16];
  size_t n = pull(&link, &pipe, taken, sizeof(taken));
  ASSERT_EQ(n, 7u);
  EXPECT_EQ(taken[0], 1);
  EXPECT_TRUE(rtu_crc_ok(taken, n));
}

TEST(MbapLinkClient, StaleTransactionIsDropped) {
  Pipe pipe;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  uint16_t txn = link.txn();
  push_mbap(&pipe, static_cast<uint16_t>(txn + 5), 1, {0x03, 0x02, 0x12, 0x34});
  uint8_t taken[16];
  EXPECT_EQ(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), txn);
}

TEST(MbapLinkClient, StaleThenMatchingInOneSegment) {
  Pipe pipe;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  uint16_t txn = link.txn();
  push_mbap(&pipe, static_cast<uint16_t>(txn + 1), 1, {0x03, 0x02, 0x00, 0x00});
  push_mbap(&pipe, txn, 1, {0x03, 0x02, 0x12, 0x34});
  uint8_t taken[16];
  size_t n = pull(&link, &pipe, taken, sizeof(taken));
  ASSERT_EQ(n, sizeof(READ_RSP) + 3);
  EXPECT_EQ(taken[3], READ_RSP[2]);
  EXPECT_EQ(taken[4], READ_RSP[3]);
}

TEST(MbapLinkClient, TransactionWrapsToOne) {
  Pipe pipe;
  TestLink link(false);
  link.set_txn(0xFFFF);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  ASSERT_GE(pipe.tx.size(), 2u);
  EXPECT_EQ(pipe.tx[0], 0x00);
  EXPECT_EQ(pipe.tx[1], 0x01);
  EXPECT_EQ(link.txn(), 1);
  EXPECT_TRUE(link.pending());
}

TEST(MbapLinkClient, BroadcastConsumesATransaction) {
  Pipe pipe;
  TestLink link(false);
  auto broadcast = rtu(0x00, {0x06, 0x00, 0x01, 0x00, 0x02});
  link.send_rtu(&pipe, broadcast.data(), broadcast.size());
  EXPECT_EQ(link.txn(), 1);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(pipe.tx[6], 0);  // unit id
}

TEST(MbapLinkClient, FlushTimeoutKeepsTheTransaction) {
  Pipe pipe;
  pipe.flushed = uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 1);
  push_mbap(&pipe, 1, 1, {0x03, 0x02, 0x12, 0x34});
  uint8_t taken[16];
  EXPECT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
}

TEST(MbapLinkClient, FlushFailedDoesNotCommitTheTransaction) {
  Pipe pipe;
  pipe.flushed = uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_FALSE(link.pending());
  EXPECT_EQ(link.txn(), 0);
  EXPECT_EQ(link.held(), 0u);
}

TEST(MbapLinkClient, ShortWriteIsHeldUntilTheQueueHasRoom) {
  Pipe pipe;
  pipe.room = 0;
  TestLink link(false);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_EQ(pipe.tx.size(), 0u);
  EXPECT_FALSE(link.pending());
  EXPECT_GT(link.held(), 0u);
  pipe.room = 1024;
  link.pump(&pipe);
  EXPECT_FALSE(pipe.tx.empty());
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.held(), 0u);
}

TEST(MbapLinkClient, LongFrameLeavesInPiecesThroughAShortQueue) {
  Pipe pipe;
  pipe.room = 32;
  TestLink link(false);
  // FC16 with 60 registers: 133 bytes as MBAP.
  const auto req = long_rtu({0x01, 0x10, 0x00, 0x00, 0x00, 60, 120}, 120);
  link.send_rtu(&pipe, req.data(), req.size());
  EXPECT_EQ(pipe.tx.size(), 32u);
  EXPECT_FALSE(link.pending());
  pump_until_sent(&link, &pipe);
  EXPECT_EQ(pipe.tx, as_mbap(1, req));
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 1);
}

TEST(MbapLinkClient, NewFrameDoesNotCutIntoAPartlySentOne) {
  Pipe pipe;
  pipe.room = 32;
  TestLink link(false);
  const auto req = long_rtu({0x01, 0x10, 0x00, 0x00, 0x00, 60, 120}, 120);
  link.send_rtu(&pipe, req.data(), req.size());
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  pump_until_sent(&link, &pipe);
  EXPECT_EQ(pipe.tx, as_mbap(1, req));
  EXPECT_EQ(link.txn(), 1);
}

TEST(MbapLinkClient, DisconnectDropsInFlight) {
  Pipe pipe;
  TestLink link(false);
  link.pump(&pipe);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  pipe.up = false;
  link.pump(&pipe);
  EXPECT_FALSE(link.pending());
}

TEST(MbapLinkServer, ReplyUsesTheRequestTransaction) {
  Pipe pipe;
  TestLink link(true);
  push_mbap(&pipe, 0x1234, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  uint8_t taken[16];
  ASSERT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  ASSERT_GE(pipe.tx.size(), 7u);
  EXPECT_EQ(pipe.tx[0], 0x12);
  EXPECT_EQ(pipe.tx[1], 0x34);
  EXPECT_EQ(pipe.tx[6], 1);
  EXPECT_FALSE(link.pending());
}

TEST(MbapLinkServer, BroadcastIsNotAnswered) {
  Pipe pipe;
  TestLink link(true);
  push_mbap(&pipe, 5, 0, {0x06, 0x00, 0x01, 0x00, 0x03});
  uint8_t taken[16];
  ASSERT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_EQ(taken[0], 0);
  EXPECT_FALSE(link.pending());
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_EQ(pipe.tx.size(), 0u);
}

TEST(MbapLinkServer, UnreadRequestIsKept) {
  Pipe pipe;
  TestLink link(true);
  push_mbap(&pipe, 7, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  link.pump(&pipe);
  EXPECT_EQ(link.txn(), 7);
  push_mbap(&pipe, 8, 1, {0x03, 0x00, 0x01, 0x00, 0x01});
  link.pump(&pipe);
  EXPECT_EQ(link.txn(), 7);
  uint8_t taken[16];
  ASSERT_GT(link.take_rtu(taken, sizeof(taken)), 0u);
  EXPECT_EQ(taken[0], 1);
  link.pump(&pipe);
  EXPECT_EQ(link.txn(), 8);
  ASSERT_GT(link.take_rtu(taken, sizeof(taken)), 0u);
}

TEST(MbapLinkServer, ReplyWhileTheNewRequestIsBufferedIsDropped) {
  Pipe pipe;
  TestLink link(true);
  push_mbap(&pipe, 7, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  uint8_t taken[16];
  ASSERT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  push_mbap(&pipe, 8, 1, {0x03, 0x00, 0x01, 0x00, 0x01});
  link.pump(&pipe);
  ASSERT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 8);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_EQ(pipe.tx.size(), 0u);
  EXPECT_TRUE(link.pending());
  EXPECT_EQ(link.txn(), 8);
  ASSERT_GT(link.take_rtu(taken, sizeof(taken)), 0u);
  link.send_rtu(&pipe, RTU_REQ.data(), RTU_REQ.size());
  EXPECT_FALSE(pipe.tx.empty());
  EXPECT_EQ(pipe.tx[1], 8);
}

TEST(MbapLinkServer, LongReplyLeavesWholeBeforeTheNextRequest) {
  Pipe pipe;
  pipe.room = 32;
  TestLink link(true);
  push_mbap(&pipe, 7, 1, {0x03, 0x00, 0x00, 0x00, 0x7D});
  uint8_t taken[16];
  ASSERT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  // 125 registers, the longest read reply: 259 bytes as MBAP.
  const auto reply = long_rtu({0x01, 0x03, 250}, 250);
  link.send_rtu(&pipe, reply.data(), reply.size());
  push_mbap(&pipe, 8, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  link.pump(&pipe);
  EXPECT_FALSE(link.has_rtu());
  pump_until_sent(&link, &pipe);
  EXPECT_EQ(pipe.tx, as_mbap(7, reply));
  link.pump(&pipe);
  ASSERT_GT(link.take_rtu(taken, sizeof(taken)), 0u);
  EXPECT_EQ(link.txn(), 8);
}

TEST(MbapLinkServer, BadProtocolIdSkipsOnlyThatFrame) {
  Pipe pipe;
  TestLink link(true);
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  pipe.rx.insert(pipe.rx.end(), bad, bad + sizeof(bad));
  push_mbap(&pipe, 7, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  uint8_t taken[16];
  EXPECT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_EQ(link.txn(), 7);
  EXPECT_FALSE(link.resyncing());
}

TEST(MbapLinkServer, BadProtocolIdSplitAcrossReadsIsSkipped) {
  Pipe pipe;
  TestLink link(true);
  const uint8_t head[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x01, 0x03};
  const uint8_t tail[] = {0x00, 0x00, 0x00, 0x01};
  pipe.rx.insert(pipe.rx.end(), head, head + sizeof(head));
  uint8_t taken[16];
  EXPECT_EQ(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  pipe.rx.insert(pipe.rx.end(), tail, tail + sizeof(tail));
  push_mbap(&pipe, 8, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  link.pump(&pipe);
  EXPECT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_EQ(link.txn(), 8);
}

TEST(MbapLinkServer, BadLengthWaitsForAQuietStream) {
  Pipe pipe;
  TestLink link(true);
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03};
  pipe.rx.insert(pipe.rx.end(), bad, bad + sizeof(bad));
  uint8_t taken[16];
  EXPECT_EQ(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_TRUE(link.resyncing());
  // Still inside the resync: this request is discarded with the rest of the stream.
  push_mbap(&pipe, 7, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  EXPECT_EQ(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  link.pump(&pipe);
  EXPECT_FALSE(link.resyncing());
  push_mbap(&pipe, 9, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  EXPECT_GT(pull(&link, &pipe, taken, sizeof(taken)), 0u);
  EXPECT_EQ(link.txn(), 9);
}

}  // namespace
}  // namespace esphome::modbus::testing
