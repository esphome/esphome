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
constexpr uint8_t DATAPOINT_QUERY = 0x08;
constexpr uint8_t MISSED_HEARTBEATS = 2;
constexpr uint32_t RESET_TIME_MS = 100;
constexpr uint32_t REPLY_TIMEOUT_MS = 1000;
constexpr uint32_t COMMAND_DELAY_MS = 10;
constexpr uint8_t MODULE_VERSION = 0x00;
constexpr uint8_t MCU_VERSION = 0x03;

using Bytes = std::vector<uint8_t>;

// UART with an in-memory receive queue and a record of the bytes written
class PlainUARTComponent : public uart::UARTComponent {
 public:
  Bytes rx;
  Bytes tx;
  uart::UARTFlushResult flush_result{uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS};

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
  uart::UARTFlushResult flush() override { return this->flush_result; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

// The same with a TX line that can be held low: every request is recorded
class BreakUARTComponent : public PlainUARTComponent {
 public:
  std::vector<bool> breaks;

  bool set_tx_break(bool enable) override {
    this->breaks.push_back(enable);
    return true;
  }
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

}  // namespace

// Friend of TuyaUART. The reply wait of 1 s is not run: the time of the last send is moved back.
// The reset time runs on the real clock.
class TuyaResetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_mcu_watchdog(MISSED_HEARTBEATS);
    this->sut_.set_mcu_watchdog_reset(RESET_TIME_MS);
  }

  // The bus cannot hold TX low
  void use_plain_bus_() {
    this->bus_ = &this->plain_;
    this->sut_.set_uart_parent(&this->plain_);
  }

  // action: reboot
  void use_reboot_() { this->sut_.set_mcu_watchdog_reset(0); }

  // The initialization is over, the watchdog is armed
  void initialized_() {
    this->sut_.init_state_ = TuyaInitState::INIT_DONE;
    this->sut_.working_mode_received_ = true;
  }

  // Sets the time of the loop pass, as the main loop does before every component
  void tick_() { LoopBlockingGuard guard(nullptr, nullptr, millis()); }

  void loop_() {
    this->tick_();
    this->sut_.loop();
  }

  void miss_heartbeats_(uint8_t count) {
    for (uint8_t i = 0; i < count; i++)
      this->sut_.note_missed_heartbeat_();
  }

  // Misses enough heartbeats to start the reset
  void start_reset_() {
    this->tick_();
    this->miss_heartbeats_(MISSED_HEARTBEATS);
    ASSERT_TRUE(this->sut_.tx_break_active_);
  }

  // Waits for the reset time and runs the loop that releases TX
  void finish_reset_() {
    delay(RESET_TIME_MS + 10);
    this->loop_();
    ASSERT_FALSE(this->sut_.tx_break_active_);
  }

  // Runs of misses with a silent MCU, each reset is waited for
  void resets_without_answer_(uint8_t runs) {
    for (uint8_t i = 0; i < runs; i++) {
      this->tick_();
      this->miss_heartbeats_(MISSED_HEARTBEATS);
      if (this->sut_.tx_break_active_) {
        delay(RESET_TIME_MS + 10);
        this->loop_();
      }
    }
  }

  // A frame from the MCU
  void mcu_(uint8_t cmd, const Bytes &payload) {
    Bytes in = frame(cmd, payload);
    this->bus_->rx.insert(this->bus_->rx.end(), in.begin(), in.end());
    this->loop_();
  }

  // A heartbeat that is written at once
  void send_heartbeat_() {
    this->sut_.last_command_timestamp_ = millis() - COMMAND_DELAY_MS - 1;
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

  // The friendship does not reach the test bodies
  bool active_() const { return this->sut_.tx_break_active_; }
  bool unconfirmed_() const { return this->sut_.tx_break_unconfirmed_; }
  uint32_t resets_() const { return this->sut_.mcu_resets_; }
  uint8_t misses_() const { return this->sut_.heartbeat_misses_; }
  bool working_mode_received_() const { return this->sut_.working_mode_received_; }
  void queue_heartbeat_() { this->sut_.send_empty_command_(TuyaCommandType::HEARTBEAT); }

  BreakUARTComponent uart_;
  PlainUARTComponent plain_;
  PlainUARTComponent *bus_{&this->uart_};
  TuyaUART sut_;
};

TEST_F(TuyaResetTest, BusWithoutBreakReportsIt) {
  PlainUARTComponent plain;
  EXPECT_FALSE(plain.set_tx_break(true));
}

TEST_F(TuyaResetTest, NoResetBelowTheCount) {
  this->miss_heartbeats_(MISSED_HEARTBEATS - 1);

  EXPECT_TRUE(this->uart_.breaks.empty());
  EXPECT_FALSE(this->active_());
}

TEST_F(TuyaResetTest, TxIsHeldLowAtTheCount) {
  this->start_reset_();

  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
  EXPECT_EQ(this->resets_(), 1U);
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(TuyaResetTest, NothingIsWrittenOrReadWhileTxIsLow) {
  this->start_reset_();
  this->uart_.tx.clear();

  this->queue_heartbeat_();
  Bytes in = frame(HEARTBEAT, {0x01});
  this->uart_.rx = in;
  this->loop_();

  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_EQ(this->uart_.rx, in);
  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
}

TEST_F(TuyaResetTest, TxIsReleasedAfterTheResetTime) {
  this->start_reset_();
  this->uart_.tx.clear();

  this->finish_reset_();

  EXPECT_EQ(this->uart_.breaks, (std::vector<bool>{true, false}));
  // the datapoints are queried again
  EXPECT_EQ(this->uart_.tx, frame(DATAPOINT_QUERY, {}, MODULE_VERSION));
}

TEST_F(TuyaResetTest, TxStaysLowBeforeTheResetTime) {
  this->start_reset_();

  this->loop_();

  EXPECT_TRUE(this->active_());
  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
}

TEST_F(TuyaResetTest, BytesReceivedWhileTxIsLowAreDiscarded) {
  this->start_reset_();
  this->uart_.rx = frame(HEARTBEAT, {0x01});

  this->finish_reset_();
  this->loop_();

  EXPECT_TRUE(this->uart_.rx.empty());
  EXPECT_TRUE(this->unconfirmed_());
}

TEST_F(TuyaResetTest, HeartbeatAnsweredInTimeConfirmsTheReset) {
  this->start_reset_();
  this->finish_reset_();
  ASSERT_TRUE(this->unconfirmed_());
  // the datapoint query of the release gets no reply
  this->expire_reply_();
  this->expire_reply_();

  this->heartbeat_with_reply_();

  EXPECT_FALSE(this->unconfirmed_());
  EXPECT_FALSE(this->sut_.status_has_warning());
}

// A stalled MCU answers late: no heartbeat waits for this reply
TEST_F(TuyaResetTest, LateHeartbeatReplyDoesNotConfirmTheReset) {
  this->start_reset_();
  this->finish_reset_();

  this->mcu_(HEARTBEAT, {0x01});

  EXPECT_TRUE(this->unconfirmed_());
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(TuyaResetTest, HeartbeatReplyToTheSecondSendDoesNotConfirmTheReset) {
  this->start_reset_();
  this->finish_reset_();
  // the datapoint query of the release gets no reply
  this->expire_reply_();
  this->expire_reply_();

  this->send_heartbeat_();
  this->expire_reply_();
  this->mcu_(HEARTBEAT, {0x01});

  EXPECT_TRUE(this->unconfirmed_());
}

TEST_F(TuyaResetTest, ResetsWithoutAnAnswerGoOnBeforeTheWorkingModeReply) {
  ASSERT_FALSE(this->working_mode_received_());

  // returns: no reboot
  this->resets_without_answer_(4);

  EXPECT_EQ(this->resets_(), 4U);
}

TEST_F(TuyaResetTest, SilentMcuIsResetAgain) {
  this->start_reset_();
  this->finish_reset_();

  this->miss_heartbeats_(MISSED_HEARTBEATS);

  EXPECT_EQ(this->uart_.breaks, (std::vector<bool>{true, false, true}));
  EXPECT_EQ(this->resets_(), 2U);
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(TuyaResetTest, ResetRunsBeforeTheWorkingModeReply) {
  ASSERT_FALSE(this->working_mode_received_());

  this->start_reset_();

  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
}

TEST_F(TuyaResetTest, ResetIsSkippedWhileTheBusStillSends) {
  this->uart_.flush_result = uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  this->miss_heartbeats_(MISSED_HEARTBEATS);

  EXPECT_TRUE(this->uart_.breaks.empty());
  EXPECT_FALSE(this->active_());

  // the next misses try again
  this->uart_.flush_result = uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  this->miss_heartbeats_(MISSED_HEARTBEATS);

  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
}

TEST_F(TuyaResetTest, BusWithoutBreakDoesNotRebootBeforeTheWorkingModeReply) {
  this->use_plain_bus_();

  this->miss_heartbeats_(MISSED_HEARTBEATS);

  EXPECT_FALSE(this->active_());
  EXPECT_EQ(this->resets_(), 0U);
}

TEST_F(TuyaResetTest, UnansweredHeartbeatsStartTheReset) {
  this->initialized_();
  this->tick_();

  for (uint8_t i = 0; i < MISSED_HEARTBEATS; i++)
    this->heartbeat_without_reply_();

  EXPECT_TRUE(this->active_());
  EXPECT_EQ(this->uart_.breaks, std::vector<bool>{true});
}

TEST_F(TuyaResetTest, RebootActionWaitsForTheWorkingModeReply) {
  this->use_reboot_();
  ASSERT_FALSE(this->working_mode_received_());

  // returns: no reboot
  this->miss_heartbeats_(MISSED_HEARTBEATS);

  EXPECT_TRUE(this->uart_.breaks.empty());
}

// The host restart ends the process with exit code 0
using TuyaResetDeathTest = TuyaResetTest;

TEST_F(TuyaResetDeathTest, RebootActionRebootsAtTheCount) {
  this->use_reboot_();
  this->initialized_();

  EXPECT_EXIT(this->miss_heartbeats_(MISSED_HEARTBEATS), ::testing::ExitedWithCode(0), "");
}

// The fourth run of misses finds three resets without an answer
TEST_F(TuyaResetDeathTest, ThreeResetsWithoutAnAnswerRebootTheModule) {
  this->initialized_();

  EXPECT_EXIT(this->resets_without_answer_(4), ::testing::ExitedWithCode(0), "");
}

TEST_F(TuyaResetDeathTest, BusWithoutBreakRebootsAfterTheWorkingModeReply) {
  this->use_plain_bus_();
  this->initialized_();

  EXPECT_EXIT(this->miss_heartbeats_(MISSED_HEARTBEATS), ::testing::ExitedWithCode(0), "");
}

}  // namespace esphome::tuya::testing
