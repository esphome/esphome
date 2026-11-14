#include "sx126x_test_helpers.h"
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include <string>
#include <tuple>

namespace esphome::sx126x::testing {

class RadioLogCallback {
 public:
  RadioLogCallback() : previous_(logger::global_logger) {
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
    this->logger_.add_log_callback(this,
                                   [](void *instance, uint8_t level, const char *tag, const char *message, size_t len) {
                                     auto *self = static_cast<RadioLogCallback *>(instance);
                                     if (self->callback_) {
                                       self->callback_(std::string(message, len));
                                     }
                                   });
  }
  ~RadioLogCallback() { logger::global_logger = this->previous_; }
  void set_callback(std::function<void(const std::string &)> callback) { this->callback_ = std::move(callback); }

 private:
  std::function<void(const std::string &)> callback_;
  logger::Logger *previous_;
  logger::Logger logger_{0};
};

struct ReentryCompletion final : SX126xTransmitListener {
  void on_transmit_complete(SX126xError result) override { this->results.push_back(result); }
  std::vector<SX126xError> results;
};

// Setup reports why it failed, not a generic reason
TEST(SX126xReentry, FailedSetupKeepsItsReason) {
  RadioLogCallback log;
  std::vector<std::string> messages;
  log.set_callback([&](const std::string &message) { messages.push_back(message); });
  RadioHardware hw;
  hw.supported_version = false;
  hw.configure(true, true);
  ASSERT_TRUE(hw.radio.is_failed());
  bool found = false;
  for (const auto &message : messages)
    found |= message.find("set Error flag: Unsupported chip version") != std::string::npos;
  EXPECT_TRUE(found);
}

// Status logging is the only user code that can run inside a radio operation. It runs only
// after the operation has committed its state, so a callback sees either a running send or a
// released radio, and the interrupted operation touches nothing after the callback returns.

TEST(SX126xReentry, SuccessfulStartRunsNoUserCode) {
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion outer, nested;
  hw.configure(true, true);
  hw.auto_complete = false;
  hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  hw.irq = IRQ_TX_DONE;  // A stale IRQ must not complete the new send either.
  unsigned callbacks = 0;
  log.set_callback([&](const std::string &message) {
    if (message.find("cleared Warning flag") != std::string::npos) {
      callbacks++;
      EXPECT_FALSE(hw.radio.is_transmitting());  // Only the completion clears the warning.
      EXPECT_EQ(hw.radio.start_transmit_async(&nested, hw.packet), SX126xError::SX126X_ERROR_NONE);
    }
  });
  ASSERT_EQ(hw.radio.start_transmit_async(&outer, hw.packet), SX126xError::SX126X_ERROR_NONE);
  EXPECT_EQ(callbacks, 0u);
  EXPECT_TRUE(outer.results.empty());
  LoopBlockingGuard clock{nullptr, nullptr, millis()};
  hw.radio.loop();
  EXPECT_TRUE(outer.results.empty());  // The stale IRQ was cleared before the send.
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  ASSERT_EQ(callbacks, 1u);
  EXPECT_EQ(outer.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_NONE}));
  EXPECT_TRUE(nested.results.empty());
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.sends, 2u);
  ASSERT_TRUE(hw.radio.cancel_transmit_async(&nested));
  EXPECT_EQ(nested.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_CANCELLED}));
  EXPECT_EQ(outer.results.size(), 1u);
}

class SX126xStartReentry : public ::testing::TestWithParam<bool> {};

TEST_P(SX126xStartReentry, WarningCallbackAfterFailedStartSeesReleasedRadio) {
  const bool cold_sleep = GetParam();
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion outer, nested;
  hw.configure(true, true);
  hw.auto_complete = false;
  hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  hw.clear_trace();
  hw.fail_at = 3;  // Preparation fails; restoration succeeds and clears the warning.
  unsigned callbacks = 0;
  size_t commands_after_callback = 0;
  log.set_callback([&](const std::string &message) {
    if (message.find("cleared Warning flag") != std::string::npos) {
      callbacks++;
      EXPECT_FALSE(hw.radio.is_transmitting());
      EXPECT_EQ(hw.receiving, true);  // Restoration finished before the callback.
      if (cold_sleep) {
        hw.radio.set_mode_sleep(true);
      } else {
        EXPECT_EQ(hw.radio.start_transmit_async(&nested, hw.packet), SX126xError::SX126X_ERROR_NONE);
      }
      commands_after_callback = hw.transactions.size();
    }
  });
  EXPECT_EQ(hw.radio.start_transmit_async(&outer, hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  ASSERT_EQ(callbacks, 1u);
  EXPECT_EQ(hw.transactions.size(), commands_after_callback);  // The failed start touched nothing after it.
  EXPECT_TRUE(outer.results.empty());                          // A rejected start never completes.
  if (cold_sleep) {
    EXPECT_FALSE(hw.radio.is_transmitting());
    EXPECT_EQ(hw.radio.start_transmit_async(&outer, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    EXPECT_EQ(hw.sends, 0u);
    return;
  }
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.sends, 1u);
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  EXPECT_EQ(nested.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_NONE}));
  EXPECT_TRUE(outer.results.empty());
}

INSTANTIATE_TEST_SUITE_P(NestedSendOrColdSleep, SX126xStartReentry, ::testing::Bool());

class SX126xPollingReentry : public ::testing::TestWithParam<std::tuple<unsigned, bool>> {};

TEST_P(SX126xPollingReentry, WarningCallbackCannotCancelOrDisturbAFinishedSend) {
  const auto [failed_command, reuse_listener] = GetParam();
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion first, second;
  auto *replacement = reuse_listener ? &first : &second;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&first, hw.packet), SX126xError::SX126X_ERROR_NONE);
  hw.clear_trace();
  hw.fail_at = failed_command;  // 0: success, 1: IRQ read fails, 2: IRQ clear fails.
  hw.irq = IRQ_TX_DONE;
  hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  unsigned callbacks = 0;
  size_t commands_after_replacement = 0;
  log.set_callback([&](const std::string &message) {
    if (callbacks == 0 && message.find("cleared Warning flag") != std::string::npos) {
      callbacks++;
      EXPECT_TRUE(first.results.empty());                    // Status is reported before the completion.
      EXPECT_FALSE(hw.radio.cancel_transmit_async(&first));  // Already finished: no second completion.
      EXPECT_EQ(hw.radio.start_transmit_async(replacement, hw.packet), SX126xError::SX126X_ERROR_NONE);
      commands_after_replacement = hw.transactions.size();
    }
  });
  {
    LoopBlockingGuard clock{nullptr, nullptr, millis()};
    hw.radio.loop();
  }
  ASSERT_EQ(callbacks, 1u);
  const auto expected = failed_command == 0 ? SX126xError::SX126X_ERROR_NONE : SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  EXPECT_EQ(first.results, (std::vector<SX126xError>{expected}));
  EXPECT_TRUE(second.results.empty());
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.transactions.size(), commands_after_replacement);  // No stale IRQ clear or restoration.
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  ASSERT_EQ(replacement->results.size(), reuse_listener ? 2u : 1u);
  EXPECT_EQ(replacement->results.back(), SX126xError::SX126X_ERROR_NONE);
  EXPECT_FALSE(hw.radio.is_transmitting());
}

INSTANTIATE_TEST_SUITE_P(ReadSuccessReadFailureAndClearFailure, SX126xPollingReentry,
                         ::testing::Combine(::testing::Values(0u, 1u, 2u), ::testing::Bool()));

}  // namespace esphome::sx126x::testing
