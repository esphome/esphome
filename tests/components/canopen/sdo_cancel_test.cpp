#include <gtest/gtest.h>

#include <array>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

// Parameter: true = a value write is cancelled, false = a value read.
class SdoCancellation : public SdoTest, public ::testing::WithParamInterface<bool> {
 protected:
  int callbacks_{0};
  int other_callbacks_{0};

  SdoCancellation() { this->response_timeout_ms_ = 200; }

  static void on_result(void *context, const SDOReply & /*reply*/) { (*static_cast<int *>(context))++; }

  void queue_request_() {
    if (GetParam()) {
      ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x0F, on_result, &this->callbacks_));
    } else {
      ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &this->callbacks_));
    }
  }

  // a read from another context, to node 0x21 or another one
  void queue_other_(uint8_t node) {
    ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(node, 0x6041, 0, on_result, &this->other_callbacks_));
  }

  // the request can't be sent (TX buffers full) and is cancelled
  void cancel_blocked_request_() {
    this->bus_.busy = true;
    this->queue_request_();
    this->hub_.loop();
    this->hub_.cancel_sdo(&this->callbacks_);
  }

  // cancel_sdo() from inside the next transmission, as a driver's log handler would
  void cancel_on_next_send_() {
    this->bus_.on_send = [this](uint32_t can_id) {
      this->bus_.on_send = nullptr;
      this->hub_.cancel_sdo(&this->callbacks_);
    };
  }
};

TEST_P(SdoCancellation, UnsentRequestIsReplacedWithAbort) {
  this->cancel_blocked_request_();
  ASSERT_EQ(this->bus_.count(0x621), 0u);

  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();

  ASSERT_EQ(this->bus_.count(0x621), 1u);
  const uint8_t index_low = GetParam() ? 0x40 : 0x41;
  EXPECT_EQ(this->bus_.sent.back().data, (std::vector<uint8_t>{0x80, index_low, 0x60, 0, 0, 0, 0, 0x08}));
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, PendingAbortHoldsTheNextRequest) {
  this->cancel_blocked_request_();
  this->queue_other_(0x22);
  this->hub_.loop();  // the abort is still blocked: its buffer isn't reused for node 0x22
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  EXPECT_EQ(this->bus_.count(0x622), 0u);  // the guard after the abort holds every node
  advance_(60);
  this->hub_.loop();

  EXPECT_EQ(this->bus_.count(0x622), 1u);
  this->hub_.frame(0x5A2, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  EXPECT_EQ(this->other_callbacks_, 1);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, CancellingQueuedContextKeepsActiveRequest) {
  this->queue_request_();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  this->queue_other_(0x22);
  this->hub_.cancel_sdo(&this->other_callbacks_);

  if (GetParam()) {
    this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  } else {
    this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  }
  this->hub_.loop();
  EXPECT_EQ(this->callbacks_, 1);
  EXPECT_EQ(this->other_callbacks_, 0);
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.count(0x622), 0u);
}

TEST_P(SdoCancellation, AbortGuardStartsAfterTheAbortLeft) {
  this->cancel_blocked_request_();
  this->queue_other_(0x21);
  advance_(80);
  this->bus_.busy = false;
  this->hub_.loop();  // the abort replaces the unsent request and goes out
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  // the guard starts, whole, in the pass after the abort left: neither the 80 ms blocked nor the pass count
  advance_(10);
  this->hub_.loop();
  advance_(40);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  advance_(20);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

TEST_P(SdoCancellation, AbortGuardRunsWithAnEmptyQueue) {
  this->cancel_blocked_request_();
  advance_(80);
  this->bus_.busy = false;
  this->hub_.loop();
  this->queue_other_(0x21);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  advance_(80);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

// no guard after an abort that was never sent: even a request to the same node goes out right away
TEST_P(SdoCancellation, AbortRefusedForTheResponseTimeoutIsDropped) {
  this->cancel_blocked_request_();
  this->queue_other_(0x21);
  this->hub_.loop();  // the abort waits for the bus
  advance_(150);
  this->hub_.loop();
  advance_(60);  // past the response timeout
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  // the abort never goes out, the next request does
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x40);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, AbortGuardCountsFromTheAbort) {
  this->queue_request_();
  this->hub_.loop();
  advance_(80);  // waiting for the answer, no loop in between
  this->hub_.cancel_sdo(&this->callbacks_);
  EXPECT_EQ(this->bus_.count(0x621), 1u);  // the abort goes out in the next pass
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  this->queue_other_(0x21);
  this->hub_.loop();
  advance_(40);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);  // the 80 ms before the cancel don't count
  advance_(20);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 3u);
}

TEST_P(SdoCancellation, BlockedAbortTimeoutCountsFromTheCancel) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  advance_(150);
  this->hub_.cancel_sdo(&this->callbacks_);
  advance_(60);  // past the response timeout from the request, not from the cancel
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
}

// the abort waits for the next pass
TEST_P(SdoCancellation, CancelDuringTheFirstSend) {
  this->cancel_on_next_send_();
  this->queue_request_();
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  EXPECT_NE(this->bus_.sent[this->bus_.sent.size() - 2].data[0], 0x80);  // the request first,
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);                       // then its abort
  EXPECT_EQ(this->hub_.tx_count(), 0u);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, CancelDuringARetriedSend) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->bus_.busy = false;
  this->cancel_on_next_send_();
  this->hub_.loop();  // the flush sends the queued request, cancelled meanwhile
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  EXPECT_EQ(this->hub_.tx_count(), 0u);
  // nothing stuck: the next request goes out after the guard
  this->queue_other_(0x22);
  this->hub_.loop();
  advance_(60);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 1u);
  EXPECT_EQ(this->hub_.tx_count(), 0u);
}

// Parameter: true = a buffer write is cancelled, false = a buffer read; both after their first segment.
TEST_P(SdoCancellation, CancelledBufferTransferStopsAfterTheFirstSegment) {
  std::array<uint8_t, 20> buffer{};
  if (GetParam()) {
    for (size_t i = 0; i < buffer.size(); i++)
      buffer[i] = i + 1;
    ASSERT_TRUE(this->hub_.write_sdo_data(0x21, 0x2000, 1, buffer.data(), buffer.size(), on_result, &this->callbacks_));
    this->hub_.loop();
    this->hub_.frame(0x5A1, {0x60, 0x00, 0x20, 1, 0, 0, 0, 0});  // sends the first segment
  } else {
    buffer.fill(0xAA);
    ASSERT_TRUE(this->hub_.read_sdo_data(0x21, 0x1008, 0, buffer.data(), buffer.size(), on_result, &this->callbacks_));
    this->hub_.loop();
    this->hub_.frame(0x5A1, {0x41, 0x08, 0x10, 0, 10, 0, 0, 0});
    this->hub_.frame(0x5A1, {0x00, 'c', 'a', 'n', 'o', 'p', 'e', 'n'});  // copied, the next segment requested
  }
  const size_t sent = this->bus_.count(0x621);
  this->hub_.cancel_sdo(&this->callbacks_);
  buffer.fill(0xEE);  // the caller may reuse it now
  this->hub_.loop();
  // late answers to the cancelled transfer
  if (GetParam()) {
    this->hub_.frame(0x5A1, {0x20, 0, 0, 0, 0, 0, 0, 0});
  } else {
    this->hub_.frame(0x5A1, {0x10 | (4 << 1) | 1, ' ', 'i', 'o', 0, 0, 0, 0});
  }
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), sent + 1);  // only the abort
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  for (uint8_t b : buffer)
    EXPECT_EQ(b, 0xEE);
  EXPECT_EQ(this->callbacks_, 0);
}

INSTANTIATE_TEST_SUITE_P(ReadAndWrite, SdoCancellation, ::testing::Bool());

// On a node, the NMT master's frame waits in another TX buffer of the driver: it goes out with the abort, which keeps
// the client's own buffer until it has left.
TEST(SdoCancellationNode, PendingAbortSurvivesAnNmtCommand) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();  // the boot-up heartbeat, before the bus is blocked
  bus.sent.clear();
  int callbacks = 0, other_callbacks = 0;
  auto on_result = [](void *context, const SDOReply & /*reply*/) { (*static_cast<int *>(context))++; };

  bus.busy = true;
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &callbacks));
  hub.loop();
  ASSERT_TRUE(hub.send_nmt(0x23, NMTCommand::NMT_COMMAND_START));
  hub.cancel_sdo(&callbacks);
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  hub.loop();  // the abort is still blocked: its buffer isn't reused for node 0x22
  bus.busy = false;
  hub.loop();
  hub.loop();
  EXPECT_EQ(bus.count(0x622), 0u);  // the guard after the abort holds every node
  advance_loop_clock(60);
  hub.loop();

  ASSERT_EQ(bus.count(0x621), 1u);
  EXPECT_EQ(bus.find(0x621)->data[0], 0x80);
  EXPECT_EQ(bus.count(0), 1u);  // the NMT command
  EXPECT_EQ(bus.count(0x622), 1u);
  hub.frame(0x5A2, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  EXPECT_EQ(other_callbacks, 1);
  EXPECT_EQ(callbacks, 0);
}

}  // namespace esphome::canopen::testing
