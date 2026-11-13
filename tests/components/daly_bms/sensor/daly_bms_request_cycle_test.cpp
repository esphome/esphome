#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/daly_bms/daly_bms.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "../common.h"

namespace esphome::daly_bms::testing {

namespace {

constexpr size_t FRAME_SIZE = 13;
constexpr uint32_t TIMEOUT_MS = 250;

// Sets the loop clock the component reads through App.get_loop_component_start_time().
void set_loop_time(uint32_t now) { LoopBlockingGuard clock(nullptr, LOG_STR("test"), now); }

void loop_at(DalyBmsComponent &bms, uint32_t now) {
  set_loop_time(now);
  bms.loop();
}

// A frame with the given address and first two payload bytes, and a valid checksum.
std::vector<uint8_t> frame(uint8_t address, uint8_t data_id, uint8_t first = 0, uint8_t second = 0) {
  std::vector<uint8_t> frame{0xA5, address, data_id, 0x08, first, second, 0, 0, 0, 0, 0, 0};
  frame.push_back(sum_bytes(frame.data(), frame.size()));
  return frame;
}

// A response frame from BMS address 0x01 with an empty payload.
std::vector<uint8_t> response_frame(uint8_t data_id) { return frame(0x01, data_id); }

void receive(FakeUART &uart, const std::vector<uint8_t> &bytes) {
  uart.rx.insert(uart.rx.end(), bytes.begin(), bytes.end());
}

size_t request_count(const FakeUART &uart) { return uart.tx.size() / FRAME_SIZE; }

uint8_t last_request(const FakeUART &uart) { return uart.tx[uart.tx.size() - FRAME_SIZE + 2]; }

class DalyBmsRequestCycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bms_.set_uart_parent(&this->uart_);
    this->bms_.set_address(0x80);
    this->bms_.setup();
    this->bms_.update();
    loop_at(this->bms_, this->now_);
  }

  // Lets the requests time out one after the other until the given one has been sent.
  void advance_to_(uint8_t data_id) {
    while (last_request(this->uart_) != data_id) {
      this->now_ += TIMEOUT_MS;
      loop_at(this->bms_, this->now_);
    }
  }

  // Answers the status request with the cell count, which sets how many cell voltage frames to wait for, and
  // waits until the cell voltage request has been sent.
  void advance_to_cell_voltages_(uint8_t cells) {
    this->advance_to_(0x94);
    receive(this->uart_, frame(0x01, 0x94, cells));
    while (last_request(this->uart_) != 0x95) {
      this->now_ += 10;
      loop_at(this->bms_, this->now_);
    }
  }

  FakeUART uart_;
  DalyBmsComponent bms_;
  uint32_t now_{1000};
};

TEST_F(DalyBmsRequestCycleTest, UpdateStartsWithBatteryLevel) {
  ASSERT_EQ(request_count(this->uart_), 1u);
  EXPECT_EQ(last_request(this->uart_), 0x90);
}

// Without a response, the next request waits for the timeout.
TEST_F(DalyBmsRequestCycleTest, NextRequestAfterTimeoutWithoutResponse) {
  loop_at(this->bms_, 1100);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1250);
  ASSERT_EQ(request_count(this->uart_), 2u);
  EXPECT_EQ(last_request(this->uart_), 0x91);
}

// Once the response is complete and the line is quiet, the next request goes out before the timeout.
TEST_F(DalyBmsRequestCycleTest, NextRequestFollowsCompleteResponse) {
  receive(this->uart_, response_frame(0x90));
  loop_at(this->bms_, 1010);
  loop_at(this->bms_, 1040);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1060);
  ASSERT_EQ(request_count(this->uart_), 2u);
  EXPECT_EQ(last_request(this->uart_), 0x91);
}

// A frame arriving shortly after the first one belongs to the same response and delays the next request.
TEST_F(DalyBmsRequestCycleTest, MultiFrameResponseIsReadToTheEnd) {
  this->advance_to_cell_voltages_(6);
  receive(this->uart_, response_frame(0x95));
  loop_at(this->bms_, this->now_ + 10);
  receive(this->uart_, response_frame(0x95));
  loop_at(this->bms_, this->now_ + 30);
  loop_at(this->bms_, this->now_ + 70);
  EXPECT_EQ(last_request(this->uart_), 0x95);
  loop_at(this->bms_, this->now_ + 80);
  EXPECT_EQ(last_request(this->uart_), 0x96);
}

// Bytes that reached the buffer while the loop was held up are part of the response, even if the line
// looked quiet long enough: they are read first, the next request waits for the gap after them.
TEST_F(DalyBmsRequestCycleTest, BufferedBytesHoldTheNextRequest) {
  // the battery level response is complete after one frame, so only the buffered bytes hold the next request
  receive(this->uart_, response_frame(0x90));
  loop_at(this->bms_, 1010);
  receive(this->uart_, response_frame(0x90));
  loop_at(this->bms_, 1070);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1110);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1120);
  ASSERT_EQ(request_count(this->uart_), 2u);
  EXPECT_EQ(last_request(this->uart_), 0x91);
}

// A partly received frame holds the next request, even after the quiet gap, until the receive reset drops it.
TEST_F(DalyBmsRequestCycleTest, PartialFrameHoldsTheNextRequest) {
  receive(this->uart_, response_frame(0x90));
  loop_at(this->bms_, 1010);
  receive(this->uart_, {0xA5, 0x01, 0x90, 0x08, 0x00});
  loop_at(this->bms_, 1020);
  loop_at(this->bms_, 1080);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1220);
  ASSERT_EQ(request_count(this->uart_), 2u);
  EXPECT_EQ(last_request(this->uart_), 0x91);
}

// More than seven probes take more than one temperature frame: the next cycle waits for all of them.
TEST_F(DalyBmsRequestCycleTest, TemperaturesWaitForEveryFrame) {
  this->advance_to_(0x94);
  receive(this->uart_, frame(0x01, 0x94, 16, 8));
  this->advance_to_(0x96);
  this->bms_.update();
  receive(this->uart_, response_frame(0x96));
  for (uint32_t t = 10; t <= 80; t += 10)
    loop_at(this->bms_, this->now_ + t);
  EXPECT_EQ(last_request(this->uart_), 0x96);
  receive(this->uart_, response_frame(0x96));
  loop_at(this->bms_, this->now_ + 90);
  loop_at(this->bms_, this->now_ + 150);
  EXPECT_EQ(last_request(this->uart_), 0x90);
}

// A BMS may leave more than the quiet gap between its cell voltage frames: the next request waits until
// every frame the cell count announced is in.
TEST_F(DalyBmsRequestCycleTest, CellVoltagesWaitForEveryFrame) {
  this->advance_to_cell_voltages_(16);
  for (int i = 0; i < 6; i++) {
    receive(this->uart_, response_frame(0x95));
    // the frame is read, then the line stays quiet for 80 ms
    for (uint32_t t = 10; t <= 80; t += 10)
      loop_at(this->bms_, this->now_ + t);
    this->now_ += 80;
    if (i < 5)
      ASSERT_EQ(last_request(this->uart_), 0x95) << "after frame " << i + 1;
  }
  EXPECT_EQ(last_request(this->uart_), 0x96);
}

// Without the cell count the number of cell voltage frames is unknown, so the request waits for the timeout.
TEST_F(DalyBmsRequestCycleTest, CellVoltagesWithoutCellCountWaitForTheTimeout) {
  this->advance_to_(0x95);
  receive(this->uart_, response_frame(0x95));
  loop_at(this->bms_, this->now_ + 10);
  loop_at(this->bms_, this->now_ + 200);
  EXPECT_EQ(last_request(this->uart_), 0x95);
  loop_at(this->bms_, this->now_ + 260);
  EXPECT_EQ(last_request(this->uart_), 0x96);
}

// An RS485 adapter that echoes its own request does not count as the answer.
TEST_F(DalyBmsRequestCycleTest, EchoedRequestIsNotAnAnswer) {
  receive(this->uart_, frame(0x80, 0x90));
  loop_at(this->bms_, 1010);
  loop_at(this->bms_, 1100);
  EXPECT_EQ(request_count(this->uart_), 1u);
  loop_at(this->bms_, 1260);
  ASSERT_EQ(request_count(this->uart_), 2u);
}

// The first cycle runs at boot, before the first update().
TEST(DalyBmsBootTest, FirstCycleRunsAtBoot) {
  FakeUART uart;
  DalyBmsComponent bms;
  bms.set_uart_parent(&uart);
  bms.set_address(0x80);
  bms.setup();
  loop_at(bms, 300);
  ASSERT_EQ(request_count(uart), 1u);
  EXPECT_EQ(last_request(uart), 0x90);
}

// A late answer to an earlier request is decoded, but it does not count as the answer to the current one.
TEST_F(DalyBmsRequestCycleTest, LateFrameOfPreviousRequestDoesNotTriggerEarly) {
  loop_at(this->bms_, 1250);
  ASSERT_EQ(last_request(this->uart_), 0x91);
  receive(this->uart_, response_frame(0x90));
  loop_at(this->bms_, 1260);
  loop_at(this->bms_, 1320);
  EXPECT_EQ(request_count(this->uart_), 2u);
  // the timeout counts from the last byte on the line
  loop_at(this->bms_, 1510);
  ASSERT_EQ(request_count(this->uart_), 3u);
  EXPECT_EQ(last_request(this->uart_), 0x92);
}

// An update during a running cycle must not restart it, or a short update interval would never reach the
// cell voltage and temperature requests at the end of the cycle.
TEST_F(DalyBmsRequestCycleTest, UpdateDuringCycleDoesNotRestartIt) {
  for (uint8_t expected = 0x91; expected <= 0x96; expected++) {
    this->now_ += TIMEOUT_MS;
    this->bms_.update();
    loop_at(this->bms_, this->now_);
    ASSERT_EQ(last_request(this->uart_), expected);
  }
  EXPECT_EQ(request_count(this->uart_), 7u);
}

// The cycle an update asked for while the previous one was running starts right after that one's last
// response, not only at the next update.
TEST_F(DalyBmsRequestCycleTest, RequestedCycleFollowsTheRunningOne) {
  this->bms_.update();
  this->advance_to_(0x96);
  receive(this->uart_, response_frame(0x96));
  loop_at(this->bms_, this->now_ + 10);
  EXPECT_EQ(request_count(this->uart_), 7u);
  loop_at(this->bms_, this->now_ + 70);
  ASSERT_EQ(request_count(this->uart_), 8u);
  EXPECT_EQ(last_request(this->uart_), 0x90);
}

// An update right after the last request waits for its response instead of sending into it.
TEST_F(DalyBmsRequestCycleTest, UpdateWhileLastResponseIsPendingWaitsForIt) {
  this->advance_to_(0x96);
  this->bms_.update();
  loop_at(this->bms_, this->now_ + 10);
  EXPECT_EQ(request_count(this->uart_), 7u);
  receive(this->uart_, response_frame(0x96));
  loop_at(this->bms_, this->now_ + 20);
  loop_at(this->bms_, this->now_ + 60);
  EXPECT_EQ(request_count(this->uart_), 7u);
  loop_at(this->bms_, this->now_ + 80);
  ASSERT_EQ(request_count(this->uart_), 8u);
  EXPECT_EQ(last_request(this->uart_), 0x90);
}

// Without a new update the component stays quiet after the cycle, and the next update starts at once.
TEST_F(DalyBmsRequestCycleTest, UpdateAfterCompleteCycleStartsANewOne) {
  this->advance_to_(0x96);
  ASSERT_EQ(request_count(this->uart_), 7u);
  loop_at(this->bms_, this->now_ + TIMEOUT_MS);
  loop_at(this->bms_, this->now_ + 2 * TIMEOUT_MS);
  EXPECT_EQ(request_count(this->uart_), 7u);
  this->bms_.update();
  loop_at(this->bms_, this->now_ + 2 * TIMEOUT_MS + 10);
  ASSERT_EQ(request_count(this->uart_), 8u);
  EXPECT_EQ(last_request(this->uart_), 0x90);
}

}  // namespace

}  // namespace esphome::daly_bms::testing
