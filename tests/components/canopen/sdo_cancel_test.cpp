#include <gtest/gtest.h>

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

TEST_P(SdoCancellation, AbortGuardStartsWhenTheAbortLeaves) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  advance_clock(80);
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->last_(0x621)->data[0], 0x80);
  // the guard counts from the abort, not from the cancel: 80 ms blocked don't count
  advance_clock(20);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  advance_clock(60);
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
  advance_clock(150);
  this->hub_.loop();
  advance_clock(60);  // past the 200 ms response timeout
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
  advance_clock(210);
  this->hub_.loop();
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();
  // no guard after an abort that was never sent: the request to the same node goes out right away
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->last_(0x621)->data[0], 0x40);
}

TEST_P(SdoCancellation, AbortGuardHoldsEveryNode) {
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);  // the abort goes out at once
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 0u);
  advance_clock(60);
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x622), 1u);
}

TEST_P(SdoCancellation, AbortGuardCountsFromTheCancel) {
  this->queue_request_();
  this->hub_.loop();
  advance_clock(80);  // waiting for the answer, no loop in between
  this->hub_.cancel_sdo(&this->callbacks_);
  ASSERT_EQ(this->bus_.count(0x621), 2u);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);  // the 80 ms before the cancel don't count
  advance_clock(60);
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

INSTANTIATE_TEST_SUITE_P(ReadAndWrite, SdoCancellation, ::testing::Bool());

}  // namespace esphome::canopen::testing
