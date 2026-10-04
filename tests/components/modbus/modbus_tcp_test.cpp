#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/mbap.h"
#include "esphome/components/modbus/mbap_link.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/gpio.h"

namespace esphome::modbus::testing {
namespace {

class DriverPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::Flags::FLAG_NONE; }
  bool digital_read() override { return this->level; }
  void digital_write(bool value) override { this->level = value; }
  size_t dump_summary(char *buffer, size_t len) const override { return snprintf(buffer, len, "driver"); }

  bool level{false};
};

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
  void write_array(const uint8_t *data, size_t len) override {
    if (this->driver != nullptr) {
      this->driver_on_at_write = this->driver->level;
    }
    this->tx.insert(this->tx.end(), data, data + len);
  }
  uart::UARTFlushResult flush() override { return this->flushed; }

  bool up{true};
  size_t room{1024};
  DriverPin *driver{nullptr};
  bool driver_on_at_write{false};
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

void arm(ModbusForwardHub *hub, Pipe *local, Pipe *peer, bool local_tcp, bool peer_tcp) {
  hub->set_uart_parent(local);
  hub->set_peer(peer);
  hub->set_local_tcp(local_tcp);
  hub->set_peer_tcp(peer_tcp);
  hub->setup();
}

TEST(ModbusForward, TcpRequestBecomesRtuAndTheResponseKeepsTheTransaction) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 0x1234, 0x11, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[0], 0x11);
  EXPECT_EQ(meter.tx[1], 0x03);
  EXPECT_TRUE(rtu_crc_ok(meter.tx.data(), meter.tx.size()));

  auto rsp = rtu(0x11, {0x03, 0x02, 0x12, 0x34});
  meter.rx.insert(meter.rx.end(), rsp.begin(), rsp.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 7u);
  EXPECT_EQ(link.tx[0], 0x12);
  EXPECT_EQ(link.tx[1], 0x34);
  EXPECT_EQ(link.tx[6], 0x11);
  EXPECT_EQ(link.tx[7], 0x03);
  EXPECT_EQ(link.tx[8], 0x02);
}

TEST(ModbusForward, RtuRequestBecomesTcpAndAStaleTransactionIsDropped) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 8u);
  EXPECT_EQ(link.tx[0], 0x00);
  EXPECT_EQ(link.tx[1], 0x01);
  EXPECT_EQ(link.tx[6], 0x01);

  push_mbap(&link, 9, 1, {0x03, 0x02, 0x00, 0x00});
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());

  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  push_mbap(&link, 1, 1, {0x03, 0x02, 0x12, 0x34});
  hub.loop();
  ASSERT_GE(meter.tx.size(), 5u);
  EXPECT_EQ(meter.tx[0], 1);
  EXPECT_EQ(meter.tx[1], 0x03);
  EXPECT_EQ(meter.tx[2], 0x02);
  EXPECT_TRUE(rtu_crc_ok(meter.tx.data(), meter.tx.size()));
}

TEST(ModbusForward, BroadcastIsForwardedAndNotWaitedOn) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  auto broadcast = rtu(0x00, {0x06, 0x00, 0x01, 0x00, 0x02});
  meter.rx.insert(meter.rx.end(), broadcast.begin(), broadcast.end());
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  EXPECT_EQ(link.tx[6], 0);
  EXPECT_EQ(link.tx[1], 1);
  size_t first = link.tx.size();
  hub.loop();
  EXPECT_GT(link.tx.size(), first);
  EXPECT_EQ(link.tx[first + 6], 1);
  EXPECT_EQ(link.tx[first + 1], 2);
}

TEST(ModbusForward, BadMbapFrameIsSkippedWithoutReconnect) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  const uint8_t bad[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  link.rx.insert(link.rx.end(), bad, bad + sizeof(bad));
  push_mbap(&link, 3, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[0], 1);
}

TEST(ModbusForward, ReadToAddressZeroIsNotBroadcast) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 1, 0, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
  push_mbap(&link, 2, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[0], 1);
}

TEST(ModbusForward, ReplyToABroadcastNeverReachesTheRtuBus) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  auto broadcast = rtu(0x00, {0x06, 0x00, 0x01, 0x00, 0x02});
  meter.rx.insert(meter.rx.end(), broadcast.begin(), broadcast.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 8u);
  EXPECT_EQ(link.tx[6], 0);
  // A server that answers a broadcast anyway.
  push_mbap(&link, 1, 0, {0x06, 0x00, 0x01, 0x00, 0x02});
  hub.loop();
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
}

TEST(ModbusForward, BroadcastOnRtuWaitsTheTurnaround) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  hub.set_turnaround_time(40);
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 1, 0, {0x06, 0x00, 0x01, 0x00, 0x02});
  push_mbap(&link, 2, 1, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), 8u);
  EXPECT_EQ(meter.tx[0], 0);
  meter.tx.clear();
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[0], 1);
}

TEST(ModbusForward, FlowControlPinsDriveBothRtuSides) {
  Pipe client;
  Pipe server;
  DriverPin client_pin;
  DriverPin server_pin;
  client.driver = &client_pin;
  server.driver = &server_pin;
  ModbusForwardHub hub;
  hub.set_flow_control_pin(&client_pin);
  hub.set_peer_flow_control_pin(&server_pin);
  arm(&hub, &client, &server, false, false);
  client.rx.insert(client.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  ASSERT_EQ(server.tx.size(), RTU_REQ.size());
  EXPECT_TRUE(server.driver_on_at_write);
  EXPECT_FALSE(server_pin.level);
  auto reply = rtu(0x01, {0x03, 0x02, 0x12, 0x34});
  server.rx.insert(server.rx.end(), reply.begin(), reply.end());
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  hub.loop();
  ASSERT_EQ(client.tx.size(), reply.size());
  EXPECT_TRUE(client.driver_on_at_write);
  EXPECT_FALSE(client_pin.level);
}

TEST(ModbusForward, AbandonedRequestWaitsForTheServerAndItsReplyIsDropped) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, false);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  ASSERT_FALSE(link.tx.empty());
  link.tx.clear();
  // The client gave up and asks again. The server may still answer the first request.
  auto next = rtu(0x01, {0x03, 0x00, 0x02, 0x00, 0x01});
  meter.rx.insert(meter.rx.end(), next.begin(), next.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  auto late = rtu(0x01, {0x03, 0x02, 0xAA, 0xAA});
  link.rx.insert(link.rx.end(), late.begin(), late.end());
  // The first frame is still on the wire for the RTU gap. 5 ms is past that at 115200.
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  hub.loop();
  ASSERT_EQ(link.tx.size(), next.size());
  EXPECT_EQ(link.tx[3], 0x02);
  auto reply = rtu(0x01, {0x03, 0x02, 0x00, 0x07});
  link.rx.insert(link.rx.end(), reply.begin(), reply.end());
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  hub.loop();
  ASSERT_EQ(meter.tx.size(), reply.size());
  EXPECT_EQ(meter.tx[4], 0x07);
}

TEST(ModbusForward, PipelinedTcpRequestsAreAnsweredInOrder) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 0x0B01, 0x01, {0x03, 0x00, 0x00, 0x00, 0x01});
  push_mbap(&link, 0x0B02, 0x01, {0x03, 0x00, 0x01, 0x00, 0x01});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[3], 0x00);
  meter.tx.clear();
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());

  auto first = rtu(0x01, {0x03, 0x02, 0x12, 0x34});
  meter.rx.insert(meter.rx.end(), first.begin(), first.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 11u);
  EXPECT_EQ(link.tx[0], 0x0B);
  EXPECT_EQ(link.tx[1], 0x01);
  EXPECT_EQ(link.tx[9], 0x12);
  EXPECT_EQ(link.tx[10], 0x34);
  link.tx.clear();

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[3], 0x01);
  auto second = rtu(0x01, {0x03, 0x02, 0x00, 0x07});
  meter.rx.insert(meter.rx.end(), second.begin(), second.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 11u);
  EXPECT_EQ(link.tx[1], 0x02);
  EXPECT_EQ(link.tx[9], 0x00);
  EXPECT_EQ(link.tx[10], 0x07);
}

TEST(ModbusForward, ReplyFromAnotherServerOrFunctionIsDropped) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 7, 0x11, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  auto other_server = rtu(0x22, {0x03, 0x02, 0x12, 0x34});
  meter.rx.insert(meter.rx.end(), other_server.begin(), other_server.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  auto other_function = rtu(0x11, {0x04, 0x02, 0x12, 0x34});
  meter.rx.insert(meter.rx.end(), other_function.begin(), other_function.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  auto exception = rtu(0x11, {0x83, 0x02});
  meter.rx.insert(meter.rx.end(), exception.begin(), exception.end());
  hub.loop();
  ASSERT_EQ(link.tx.size(), 9u);
  EXPECT_EQ(link.tx[1], 7);
  EXPECT_EQ(link.tx[7], 0x83);
  EXPECT_EQ(link.tx[8], 0x02);
}

TEST(ModbusForward, SilentServerGetsTcpClientException0B) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  hub.set_send_wait_time(20);
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 0x0102, 0x11, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  hub.loop();
  ASSERT_EQ(link.tx.size(), 9u);
  EXPECT_EQ(link.tx[0], 0x01);
  EXPECT_EQ(link.tx[1], 0x02);
  EXPECT_EQ(link.tx[5], 3);
  EXPECT_EQ(link.tx[6], 0x11);
  EXPECT_EQ(link.tx[7], 0x83);
  EXPECT_EQ(link.tx[8], 0x0B);
}

TEST(ModbusForward, SendWaitTimeEndsWhenTheReplyStarts) {
  Pipe link;
  Pipe meter;
  meter.set_baud_rate(1200);
  ModbusForwardHub hub;
  // The wait ends 30 ms after the 8-byte request has left the wire (67 ms at 1200 baud).
  hub.set_send_wait_time(30);
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 0x0101, 0x01, {0x03, 0x00, 0x00, 0x00, 0x0A});
  hub.loop();
  ASSERT_FALSE(meter.tx.empty());
  // A 25-byte reply starts before the wait ends and ends after it.
  const auto reply = long_rtu({0x01, 0x03, 20}, 20);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  for (size_t at = 0; at < reply.size(); at += 5) {
    meter.rx.insert(meter.rx.end(), reply.begin() + at, reply.begin() + at + 5);
    hub.loop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  hub.loop();
  EXPECT_EQ(link.tx, as_mbap(0x0101, reply));

  // A silent server still ends in exception 0B.
  link.tx.clear();
  std::this_thread::sleep_for(std::chrono::milliseconds(35));
  push_mbap(&link, 0x0102, 0x01, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(110));
  hub.loop();
  ASSERT_EQ(link.tx.size(), 9u);
  EXPECT_EQ(link.tx[7], 0x83);
  EXPECT_EQ(link.tx[8], 0x0B);
}

TEST(ModbusForward, LateReplyIsNeverForwardedAsARequest) {
  Pipe link;
  Pipe meter;
  ModbusForwardHub hub;
  hub.set_send_wait_time(20);
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 1, 0x11, {0x06, 0x00, 0x01, 0x00, 0x05});
  hub.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  hub.loop();
  ASSERT_EQ(link.tx.size(), 9u);
  link.tx.clear();
  meter.tx.clear();

  auto late = rtu(0x11, {0x06, 0x00, 0x01, 0x00, 0x05});
  meter.rx.insert(meter.rx.end(), late.begin(), late.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());

  // After the quiet time on the RTU bus the next request goes out and is answered.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  push_mbap(&link, 2, 0x11, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  EXPECT_EQ(meter.tx[0], 0x11);
  auto reply = rtu(0x11, {0x03, 0x02, 0x12, 0x34});
  meter.rx.insert(meter.rx.end(), reply.begin(), reply.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 11u);
  EXPECT_EQ(link.tx[1], 2);
  EXPECT_EQ(link.tx[9], 0x12);
}

TEST(ModbusForward, RtuClientGetsNothingWhenTheServerIsSilent) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  hub.set_send_wait_time(20);
  arm(&hub, &meter, &link, false, false);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  ASSERT_FALSE(link.tx.empty());
  link.tx.clear();
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  EXPECT_EQ(link.tx.size(), RTU_REQ.size());
}

TEST(ModbusForward, TcpReplyTakesTheUnitOfTheRequest) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  push_mbap(&link, 1, 0xFF, {0x03, 0x02, 0x12, 0x34});
  hub.loop();
  ASSERT_EQ(meter.tx.size(), 7u);
  EXPECT_EQ(meter.tx[0], 0x01);
  EXPECT_TRUE(rtu_crc_ok(meter.tx.data(), meter.tx.size()));
}

TEST(ModbusForward, BackToBackRtuFramesAreNotMerged) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, false);
  auto second = rtu(0x01, {0x03, 0x00, 0x0A, 0x00, 0x01});
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  meter.rx.insert(meter.rx.end(), second.begin(), second.end());
  hub.loop();
  EXPECT_EQ(link.tx.size(), RTU_REQ.size());
}

TEST(ModbusForward, RtuSecondWriteWaitsForTheGap) {
  Pipe meter;
  Pipe link;
  meter.set_baud_rate(300);
  link.set_baud_rate(300);
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, false);
  auto broadcast = rtu(0x00, {0x06, 0x00, 0x01, 0x00, 0x02});
  meter.rx.insert(meter.rx.end(), broadcast.begin(), broadcast.end());
  hub.loop();
  ASSERT_FALSE(link.tx.empty());
  link.tx.clear();
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  hub.loop();
  ASSERT_FALSE(link.tx.empty());
  EXPECT_EQ(link.tx[0], 1);
}

TEST(ModbusForward, TcpFlushFailedStillConsumesTheResponse) {
  Pipe meter;
  Pipe link;
  link.flushed = uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  ASSERT_GE(link.tx.size(), 8u);
  const uint16_t txn = (static_cast<uint16_t>(link.tx[0]) << 8) | link.tx[1];
  link.tx.clear();
  push_mbap(&link, static_cast<uint16_t>(txn + 5), 1, {0x03, 0x02, 0x00, 0x00});
  hub.loop();
  EXPECT_TRUE(meter.tx.empty());
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  push_mbap(&link, txn, 1, {0x03, 0x02, 0x12, 0x34});
  hub.loop();
  ASSERT_FALSE(meter.tx.empty());
  EXPECT_EQ(meter.tx[1], 0x03);
  EXPECT_TRUE(rtu_crc_ok(meter.tx.data(), meter.tx.size()));
}

TEST(ModbusForward, ReplaceWhileTheTcpQueueIsFull) {
  Pipe meter;
  Pipe link;
  link.room = 0;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  auto next = rtu(0x01, {0x03, 0x00, 0x02, 0x00, 0x01});
  meter.rx.insert(meter.rx.end(), next.begin(), next.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  link.room = 1024;
  hub.loop();
  ASSERT_GE(link.tx.size(), 10u);
  EXPECT_EQ(link.tx[0], 0x00);
  EXPECT_EQ(link.tx[1], 0x01);
  EXPECT_EQ(link.tx[9], 0x02);
}

TEST(ModbusForward, LongRequestLeavesWholeThroughAShortTcpQueue) {
  Pipe meter;
  Pipe link;
  link.room = 32;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, true);
  // FC16 with 60 registers: 133 bytes as MBAP.
  const auto req = long_rtu({0x01, 0x10, 0x00, 0x00, 0x00, 60, 120}, 120);
  meter.rx.insert(meter.rx.end(), req.begin(), req.end());
  for (int i = 0; i < 8; i++) {
    hub.loop();
  }
  EXPECT_EQ(link.tx, as_mbap(1, req));
  // The RTU line must be silent for 3.5 characters (1.75 ms above 19200 baud) before the next frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  push_mbap(&link, 1, 1, {0x10, 0x00, 0x00, 0x00, 60});
  hub.loop();
  EXPECT_EQ(meter.tx, rtu(0x01, {0x10, 0x00, 0x00, 0x00, 60}));
}

TEST(ModbusForward, LongReplyLeavesWholeBeforeTheNextReply) {
  Pipe link;
  Pipe meter;
  link.room = 32;
  ModbusForwardHub hub;
  arm(&hub, &link, &meter, true, false);
  push_mbap(&link, 0x0B01, 0x01, {0x03, 0x00, 0x00, 0x00, 0x7D});
  push_mbap(&link, 0x0B02, 0x01, {0x03, 0x00, 0x01, 0x00, 0x01});
  hub.loop();
  ASSERT_FALSE(meter.tx.empty());
  meter.tx.clear();
  // 125 registers, the longest read reply: 259 bytes as MBAP.
  const auto first = long_rtu({0x01, 0x03, 250}, 250);
  meter.rx.insert(meter.rx.end(), first.begin(), first.end());
  hub.loop();
  EXPECT_EQ(link.tx.size(), 32u);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  hub.loop();
  // The second request is on the bus while the first reply is still leaving.
  ASSERT_EQ(meter.tx.size(), RTU_REQ.size());
  const auto second = rtu(0x01, {0x03, 0x02, 0x00, 0x07});
  meter.rx.insert(meter.rx.end(), second.begin(), second.end());
  for (int i = 0; i < 16; i++) {
    hub.loop();
  }
  auto expected = as_mbap(0x0B01, first);
  const auto next = as_mbap(0x0B02, second);
  expected.insert(expected.end(), next.begin(), next.end());
  EXPECT_EQ(link.tx, expected);
}

TEST(ModbusForward, TcpToTcpKeepsEachSidesTransaction) {
  Pipe origin;
  Pipe peer;
  ModbusForwardHub hub;
  arm(&hub, &origin, &peer, true, true);
  push_mbap(&origin, 0x1234, 0x11, {0x03, 0x00, 0x00, 0x00, 0x01});
  hub.loop();
  ASSERT_GE(peer.tx.size(), 8u);
  EXPECT_EQ(peer.tx[0], 0x00);
  EXPECT_EQ(peer.tx[1], 0x01);
  EXPECT_EQ(peer.tx[6], 0x11);
  push_mbap(&peer, 1, 0x11, {0x03, 0x02, 0x12, 0x34});
  hub.loop();
  ASSERT_GE(origin.tx.size(), 8u);
  EXPECT_EQ(origin.tx[0], 0x12);
  EXPECT_EQ(origin.tx[1], 0x34);
  EXPECT_EQ(origin.tx[6], 0x11);
  EXPECT_EQ(origin.tx[7], 0x03);
}

TEST(ModbusForward, BadCrcIsNotForwarded) {
  Pipe meter;
  Pipe link;
  ModbusForwardHub hub;
  arm(&hub, &meter, &link, false, false);
  auto bad = RTU_REQ;
  bad.back() ^= 0xFF;
  meter.rx.insert(meter.rx.end(), bad.begin(), bad.end());
  hub.loop();
  EXPECT_TRUE(link.tx.empty());
  meter.rx.insert(meter.rx.end(), RTU_REQ.begin(), RTU_REQ.end());
  hub.loop();
  EXPECT_FALSE(link.tx.empty());
  EXPECT_EQ(link.tx[0], 1);
}

}  // namespace
}  // namespace esphome::modbus::testing
