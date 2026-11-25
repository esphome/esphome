#include <gtest/gtest.h>

#include <array>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

// Parameter: true = a value write is cancelled, false = a value read.
class SdoCancellation : public ::testing::TestWithParam<bool> {
 protected:
  FakeCanbus bus_;
  TestHub hub_{&this->bus_};
  int callbacks_{0};

  void SetUp() override {
    this->hub_.set_response_timeout(200);
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
  }

  static void on_result(void *context, const SDOReply & /*reply*/) { (*static_cast<int *>(context))++; }

  static void advance_clock(uint32_t ms) {
    LoopBlockingGuard clock{nullptr, nullptr, App.get_loop_component_start_time() + ms};
  }

  void queue_request_() {
    if (GetParam()) {
      ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x0F, on_result, &this->callbacks_));
    } else {
      ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &this->callbacks_));
    }
  }

  const FakeCanbus::Frame *last_(uint32_t can_id) const {
    const FakeCanbus::Frame *found = nullptr;
    for (const auto &f : this->bus_.sent) {
      if (f.can_id == can_id)
        found = &f;
    }
    return found;
  }
};

TEST_P(SdoCancellation, UnsentRequestIsReplacedWithAbort) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 0u);

  this->hub_.cancel_sdo(&this->callbacks_);
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();

  ASSERT_EQ(this->bus_.count(0x621), 1u);
  const uint8_t index_low = GetParam() ? 0x40 : 0x41;
  EXPECT_EQ(this->last_(0x621)->data, (std::vector<uint8_t>{0x80, index_low, 0x60, 0, 0, 0, 0, 0x08}));
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, PendingAbortHoldsTheNextRequest) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);

  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();  // the abort is still blocked: its buffer isn't reused for node 0x22
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 0u);  // the guard after the abort holds every node
  advance_clock(60);
  this->hub_.loop();

  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->last_(0x621)->data[0], 0x80);
  EXPECT_EQ(this->bus_.count(0x622), 1u);
  this->hub_.frame(0x5A2, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  EXPECT_EQ(other_callbacks, 1);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, CancellingQueuedContextKeepsActiveRequest) {
  this->queue_request_();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  int queued_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &queued_callbacks));
  this->hub_.cancel_sdo(&queued_callbacks);

  if (GetParam()) {
    this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  } else {
    this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  }
  this->hub_.loop();
  EXPECT_EQ(this->callbacks_, 1);
  EXPECT_EQ(queued_callbacks, 0);
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.count(0x622), 0u);
}

TEST_P(SdoCancellation, AbortGuardStartsAfterTheAbortLeft) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  advance_clock(80);
  this->bus_.busy = false;
  this->hub_.loop();  // the abort replaces the unsent request and goes out
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  // the guard starts, whole, in the pass after the abort left: neither the 80 ms blocked nor the pass count
  advance_clock(10);
  this->hub_.loop();
  advance_clock(40);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  advance_clock(20);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

TEST_P(SdoCancellation, AbortGuardRunsWithAnEmptyQueue) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  advance_clock(80);
  this->bus_.busy = false;
  this->hub_.loop();
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  advance_clock(80);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

TEST_P(SdoCancellation, AbortRefusedForTheResponseTimeoutIsDropped) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();  // the abort waits for the bus
  advance_clock(150);
  this->hub_.loop();
  advance_clock(60);  // past the response timeout
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  // the abort never goes out, the next request does
  EXPECT_EQ(this->bus_.count(0x621), 0u);
  EXPECT_EQ(this->bus_.count(0x622), 1u);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, DroppedAbortDoesNotHoldTheSameNode) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();  // the abort waits for the bus
  advance_clock(210);
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  // no guard after an abort that was never sent: the request to the same node goes out right away
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x40);
}

TEST_P(SdoCancellation, AbortGuardHoldsEveryNode) {
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  EXPECT_EQ(this->bus_.count(0x621), 1u);  // the abort goes out in the next pass
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 0u);
  advance_clock(60);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 1u);
}

TEST_P(SdoCancellation, AbortGuardCountsFromTheAbort) {
  this->queue_request_();
  this->hub_.loop();
  advance_clock(80);  // waiting for the answer, no loop in between
  this->hub_.cancel_sdo(&this->callbacks_);
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  advance_clock(40);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);  // the 80 ms before the cancel don't count
  advance_clock(20);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 3u);
}

TEST_P(SdoCancellation, BlockedAbortTimeoutCountsFromTheCancel) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  advance_clock(150);
  this->hub_.cancel_sdo(&this->callbacks_);
  advance_clock(60);  // past the response timeout from the request, not from the cancel
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
}

// cancel_sdo() from inside the request's transmission (a driver's log handler): the abort waits for the next pass
TEST_P(SdoCancellation, CancelDuringTheFirstSend) {
  this->bus_.on_send = [this](uint32_t can_id) {
    this->bus_.on_send = nullptr;
    this->hub_.cancel_sdo(&this->callbacks_);
  };
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
  this->bus_.on_send = [this](uint32_t can_id) {
    this->bus_.on_send = nullptr;
    this->hub_.cancel_sdo(&this->callbacks_);
  };
  this->hub_.loop();  // the flush sends the queued request, cancelled meanwhile
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  EXPECT_EQ(this->bus_.sent.back().data[0], 0x80);
  EXPECT_EQ(this->hub_.tx_count(), 0u);
  // nothing stuck: the next request goes out after the guard
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  advance_clock(60);
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

}  // namespace esphome::canopen::testing
