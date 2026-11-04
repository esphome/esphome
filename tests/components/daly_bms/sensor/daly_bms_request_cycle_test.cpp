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

// Sets the loop clock the component reads through App.get_loop_component_start_time().
void set_loop_time(uint32_t now) { LoopBlockingGuard clock(nullptr, LOG_STR("test"), now); }

void loop_at(DalyBmsComponent &bms, uint32_t now) {
  set_loop_time(now);
  bms.loop();
}

// A response frame from BMS address 0x01 with an empty payload and a valid checksum.
std::vector<uint8_t> response_frame(uint8_t data_id) {
  std::vector<uint8_t> frame{0xA5, 0x01, data_id, 0x08, 0, 0, 0, 0, 0, 0, 0, 0};
  frame.push_back(sum_bytes(frame.data(), frame.size()));
  return frame;
}

void receive(FakeUART &uart, const std::vector<uint8_t> &bytes) {
  uart.rx.insert(uart.rx.end(), bytes.begin(), bytes.end());
}

size_t request_count(const FakeUART &uart) { return uart.tx.size() / FRAME_SIZE; }

uint8_t last_request(const FakeUART &uart) { return uart.tx[uart.tx.size() - FRAME_SIZE + 2]; }

class DalyBmsRequestCycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bms.set_uart_parent(&this->uart);
    this->bms.set_address(0x80);
    this->bms.setup();
    this->bms.update();
    loop_at(this->bms, 1000);
  }

  FakeUART uart;
  DalyBmsComponent bms;
};

TEST_F(DalyBmsRequestCycleTest, UpdateStartsWithBatteryLevel) {
  ASSERT_EQ(request_count(this->uart), 1u);
  EXPECT_EQ(last_request(this->uart), 0x90);
}

// Without a response, the next request waits for the timeout.
TEST_F(DalyBmsRequestCycleTest, NextRequestAfterTimeoutWithoutResponse) {
  loop_at(this->bms, 1100);
  EXPECT_EQ(request_count(this->uart), 1u);
  loop_at(this->bms, 1250);
  ASSERT_EQ(request_count(this->uart), 2u);
  EXPECT_EQ(last_request(this->uart), 0x91);
}

// Once the response is complete and the line is quiet, the next request goes out before the timeout.
TEST_F(DalyBmsRequestCycleTest, NextRequestFollowsCompleteResponse) {
  receive(this->uart, response_frame(0x90));
  loop_at(this->bms, 1010);
  loop_at(this->bms, 1040);
  EXPECT_EQ(request_count(this->uart), 1u);
  loop_at(this->bms, 1060);
  ASSERT_EQ(request_count(this->uart), 2u);
  EXPECT_EQ(last_request(this->uart), 0x91);
}

// A frame arriving shortly after the first one belongs to the same response and delays the next request.
TEST_F(DalyBmsRequestCycleTest, MultiFrameResponseIsReadToTheEnd) {
  receive(this->uart, response_frame(0x95));
  loop_at(this->bms, 1010);
  receive(this->uart, response_frame(0x95));
  loop_at(this->bms, 1030);
  loop_at(this->bms, 1070);
  EXPECT_EQ(request_count(this->uart), 1u);
  loop_at(this->bms, 1080);
  EXPECT_EQ(request_count(this->uart), 2u);
}

// An update during a running cycle must not restart it, or a short update interval would never reach the
// cell voltage and temperature requests at the end of the cycle.
TEST_F(DalyBmsRequestCycleTest, UpdateDuringCycleDoesNotRestartIt) {
  uint32_t now = 1000;
  for (uint8_t expected = 0x91; expected <= 0x96; expected++) {
    now += 250;
    this->bms.update();
    loop_at(this->bms, now);
    ASSERT_EQ(last_request(this->uart), expected);
  }
  EXPECT_EQ(request_count(this->uart), 7u);
}

TEST_F(DalyBmsRequestCycleTest, UpdateAfterCompleteCycleStartsANewOne) {
  uint32_t now = 1000;
  for (int i = 0; i < 6; i++) {
    now += 250;
    loop_at(this->bms, now);
  }
  ASSERT_EQ(request_count(this->uart), 7u);
  loop_at(this->bms, now + 250);
  EXPECT_EQ(request_count(this->uart), 7u);
  this->bms.update();
  loop_at(this->bms, now + 260);
  ASSERT_EQ(request_count(this->uart), 8u);
  EXPECT_EQ(last_request(this->uart), 0x90);
}

}  // namespace

}  // namespace esphome::daly_bms::testing
