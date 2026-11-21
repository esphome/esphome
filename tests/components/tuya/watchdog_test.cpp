#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/tuya/tuya_uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::tuya::testing {

namespace {

constexpr uint8_t HEARTBEAT = 0x00;
constexpr uint8_t PRODUCT_QUERY = 0x01;
constexpr uint8_t DATAPOINT_REPORT = 0x07;
constexpr uint8_t MISSED_HEARTBEATS = 2;
constexpr uint32_t REPLY_TIMEOUT_MS = 1000;
constexpr uint32_t COMMAND_DELAY_MS = 10;
constexpr uint32_t HEARTBEAT_INTERVAL_ID = 0;
constexpr uint32_t WIFI_STATUS_INTERVAL_ID = 1;
constexpr uint8_t MODULE_VERSION = 0x00;
constexpr uint8_t MCU_VERSION = 0x03;

using Bytes = std::vector<uint8_t>;

// UART with an in-memory receive queue and a record of the bytes written
class RecordingUARTComponent : public uart::UARTComponent {
 public:
  Bytes rx;
  Bytes tx;

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }

  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

Bytes frame(uint8_t cmd, const Bytes &payload, uint8_t version = MCU_VERSION) {
  Bytes out{0x55, 0xAA, version, cmd, static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size())};
  out.insert(out.end(), payload.begin(), payload.end());
  uint8_t checksum = 0;
  for (uint8_t b : out)
    checksum += b;
  out.push_back(checksum);
  return out;
}

// Number of times the frame occurs in the bytes
size_t occurrences(const Bytes &bytes, const Bytes &wanted) {
  size_t found = 0;
  auto it = bytes.begin();
  while ((it = std::search(it, bytes.end(), wanted.begin(), wanted.end())) != bytes.end()) {
    found++;
    it += wanted.size();
  }
  return found;
}

}  // namespace

// Friend of TuyaUART. The reply wait of 1 s is not run: the time of the last send is moved back.
class TuyaWatchdogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_mcu_watchdog(MISSED_HEARTBEATS);
  }

  // the heartbeat: false tests start timers that capture &sut_
  void TearDown() override {
    App.scheduler.cancel_interval(&this->sut_, HEARTBEAT_INTERVAL_ID);
    App.scheduler.cancel_interval(&this->sut_, WIFI_STATUS_INTERVAL_ID);
    App.scheduler.call(millis());
    App.scheduler.trim_freelist();
  }

  // The initialization is over, the watchdog is armed
  void initialized_() {
    this->sut_.init_state_ = TuyaInitState::INIT_DONE;
    this->sut_.working_mode_received_ = true;
  }

  void loop_() {
    LoopBlockingGuard guard(nullptr, nullptr, millis());
    this->sut_.loop();
  }

  // A frame from the MCU
  void mcu_(uint8_t cmd, const Bytes &payload) {
    Bytes in = frame(cmd, payload);
    this->uart_.rx.insert(this->uart_.rx.end(), in.begin(), in.end());
    this->loop_();
  }

  // The next command is written at once
  void allow_send_() { this->sut_.last_command_timestamp_ = millis() - COMMAND_DELAY_MS - 1; }

  void send_heartbeat_() {
    this->allow_send_();
    this->sut_.send_empty_command_(TuyaCommandType::HEARTBEAT);
  }

  // The reply wait of the command sent last is over
  void expire_reply_() {
    this->sut_.last_command_timestamp_ = millis() - REPLY_TIMEOUT_MS - 1;
    this->loop_();
  }

  // A heartbeat without a reply: sent once more, then dropped
  void heartbeat_without_reply_() {
    this->send_heartbeat_();
    this->expire_reply_();
    this->expire_reply_();
  }

  void heartbeat_with_reply_() {
    this->send_heartbeat_();
    this->mcu_(HEARTBEAT, {0x01});
  }

  // The reply comes to the second send
  void heartbeat_with_late_reply_() {
    this->send_heartbeat_();
    this->expire_reply_();
    this->mcu_(HEARTBEAT, {0x01});
  }

  // The friendship does not reach the test bodies
  uint8_t misses_() const { return this->sut_.heartbeat_misses_; }
  bool unarmed_misses_logged_() const { return this->sut_.unarmed_misses_logged_; }
  bool working_mode_received_() const { return this->sut_.working_mode_received_; }
  void retry_init_() { this->sut_.retry_init_(); }

  RecordingUARTComponent uart_;
  TuyaUART sut_;
};

TEST_F(TuyaWatchdogTest, UnansweredHeartbeatIsSentTwiceAndCountsOnce) {
  this->initialized_();

  this->heartbeat_without_reply_();

  EXPECT_EQ(occurrences(this->uart_.tx, frame(HEARTBEAT, {}, MODULE_VERSION)), 2U);
  EXPECT_EQ(this->misses_(), 1U);
}

TEST_F(TuyaWatchdogTest, FirstReplyTimeoutIsNotAMiss) {
  this->initialized_();
  this->send_heartbeat_();

  this->expire_reply_();

  EXPECT_EQ(this->misses_(), 0U);
}

TEST_F(TuyaWatchdogTest, ReplyToTheSecondSendIsNotAMiss) {
  this->initialized_();
  this->send_heartbeat_();
  this->expire_reply_();

  this->mcu_(HEARTBEAT, {0x01});
  this->expire_reply_();

  EXPECT_EQ(this->misses_(), 0U);
  EXPECT_EQ(occurrences(this->uart_.tx, frame(HEARTBEAT, {}, MODULE_VERSION)), 2U);
}

TEST_F(TuyaWatchdogTest, TwoHeartbeatsInARowAnsweredAtTheFirstSendTakeOneMissOff) {
  this->initialized_();
  this->heartbeat_without_reply_();
  ASSERT_EQ(this->misses_(), 1U);

  this->heartbeat_with_reply_();
  EXPECT_EQ(this->misses_(), 1U);
  this->heartbeat_with_reply_();

  EXPECT_EQ(this->misses_(), 0U);
}

TEST_F(TuyaWatchdogTest, AnswerToTheSecondSendTakesNoMissOffAndEndsTheRow) {
  this->initialized_();
  this->heartbeat_without_reply_();

  this->heartbeat_with_reply_();
  this->heartbeat_with_late_reply_();
  this->heartbeat_with_reply_();

  EXPECT_EQ(this->misses_(), 1U);
}

// With a count of 2 a reboot would end the test run
TEST_F(TuyaWatchdogTest, McuThatSkipsAHeartbeatNowAndThenIsNotCountedOut) {
  this->initialized_();

  for (uint8_t round = 0; round < 5; round++) {
    this->heartbeat_without_reply_();
    EXPECT_EQ(this->misses_(), 1U);
    for (uint8_t i = 0; i < 4; i++)
      this->heartbeat_with_reply_();
    EXPECT_EQ(this->misses_(), 0U);
  }
}

// A reply after the reply wait is not an answer
TEST_F(TuyaWatchdogTest, LateReplyDoesNotTakeAMissOff) {
  this->initialized_();
  this->heartbeat_without_reply_();

  this->mcu_(HEARTBEAT, {0x01});

  EXPECT_EQ(this->misses_(), 1U);
}

TEST_F(TuyaWatchdogTest, NoRebootBeforeTheWorkingModeReply) {
  ASSERT_FALSE(this->working_mode_received_());

  // returns: no reboot
  for (uint8_t i = 0; i < MISSED_HEARTBEATS; i++)
    this->heartbeat_without_reply_();

  EXPECT_EQ(this->misses_(), 0U);
}

// The warning of the count before the working mode reply is logged once until an answered heartbeat
TEST_F(TuyaWatchdogTest, CountBeforeTheWorkingModeReplyIsLoggedOnceUntilAnAnswer) {
  for (uint8_t i = 0; i < 2 * MISSED_HEARTBEATS; i++)
    this->heartbeat_without_reply_();
  EXPECT_TRUE(this->unarmed_misses_logged_());

  this->heartbeat_with_reply_();

  EXPECT_FALSE(this->unarmed_misses_logged_());
}

TEST_F(TuyaWatchdogTest, EveryDatapointOfAReportIsAcknowledged) {
  this->initialized_();
  this->sut_.set_datapoint_ack(true);

  // a boolean of two bytes ends the parsing, the datapoint after it is acknowledged too
  this->mcu_(DATAPOINT_REPORT, {0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05});
  for (uint8_t i = 0; i < 2; i++) {
    delay(COMMAND_DELAY_MS + 5);
    this->loop_();
  }

  EXPECT_EQ(occurrences(this->uart_.tx, frame(DATAPOINT_REPORT, {0x01, 0x00}, MODULE_VERSION)), 1U);
  EXPECT_EQ(occurrences(this->uart_.tx, frame(DATAPOINT_REPORT, {0x02, 0x00}, MODULE_VERSION)), 1U);
}

TEST_F(TuyaWatchdogTest, DatapointCutOffByTheEndOfTheReportIsNotAcknowledged) {
  this->initialized_();
  this->sut_.set_datapoint_ack(true);

  // datapoint 2 announces four value bytes, the frame holds two
  this->mcu_(DATAPOINT_REPORT, {0x01, 0x01, 0x00, 0x01, 0x01, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00});
  for (uint8_t i = 0; i < 2; i++) {
    delay(COMMAND_DELAY_MS + 5);
    this->loop_();
  }

  EXPECT_EQ(occurrences(this->uart_.tx, frame(DATAPOINT_REPORT, {0x01, 0x00}, MODULE_VERSION)), 1U);
  EXPECT_EQ(occurrences(this->uart_.tx, frame(DATAPOINT_REPORT, {0x02, 0x00}, MODULE_VERSION)), 0U);
}

TEST_F(TuyaWatchdogTest, ReportIsNotAcknowledgedByDefault) {
  this->initialized_();

  this->mcu_(DATAPOINT_REPORT, {0x01, 0x01, 0x00, 0x01, 0x01});
  delay(COMMAND_DELAY_MS + 5);
  this->loop_();

  EXPECT_TRUE(this->uart_.tx.empty());
}

// heartbeat: false: no heartbeat is written, the initialization starts at the product query
TEST_F(TuyaWatchdogTest, WithoutHeartbeatTheInitializationStartsAtTheProductQuery) {
  this->sut_.set_heartbeat(false);
  this->allow_send_();

  this->sut_.setup();

  EXPECT_EQ(this->uart_.tx, frame(PRODUCT_QUERY, {}, MODULE_VERSION));

  this->uart_.tx.clear();
  this->mcu_(PRODUCT_QUERY, {'p'});
  delay(COMMAND_DELAY_MS + 5);
  this->loop_();

  // no working mode query: the WiFi status is next, sent by its timer
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_EQ(this->sut_.get_init_state(), TuyaInitState::INIT_WIFI);
}

TEST_F(TuyaWatchdogTest, WithoutHeartbeatAnUnansweredProductQueryIsSentAgain) {
  this->sut_.set_heartbeat(false);
  this->allow_send_();
  this->sut_.setup();
  // sent once more, then dropped
  this->expire_reply_();
  this->expire_reply_();
  this->uart_.tx.clear();

  // the tick of the init timer
  this->allow_send_();
  this->retry_init_();

  EXPECT_EQ(this->uart_.tx, frame(PRODUCT_QUERY, {}, MODULE_VERSION));
}

// The host restart ends the process with exit code 0
using TuyaWatchdogDeathTest = TuyaWatchdogTest;

TEST_F(TuyaWatchdogDeathTest, SilentMcuRebootsTheModuleAtTheCount) {
  this->initialized_();

  EXPECT_EXIT(
      {
        for (uint8_t i = 0; i < MISSED_HEARTBEATS; i++)
          this->heartbeat_without_reply_();
      },
      ::testing::ExitedWithCode(0), "");
}

// Seen on a BHT-002: replies 1 to 4 s late in turn, so every other one is in time
TEST_F(TuyaWatchdogDeathTest, McuThatAnswersInTimeOnlyNowAndThenRebootsTheModule) {
  this->initialized_();
  this->heartbeat_without_reply_();
  this->heartbeat_with_reply_();
  this->heartbeat_with_late_reply_();
  ASSERT_EQ(this->misses_(), 1U);

  EXPECT_EXIT(this->heartbeat_without_reply_(), ::testing::ExitedWithCode(0), "");
}

}  // namespace esphome::tuya::testing
