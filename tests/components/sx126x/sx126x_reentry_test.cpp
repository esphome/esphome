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

TEST(SX126xReentry, PreflightWarningClearCannotOverwriteNestedSend) {
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion outer, nested;
  hw.configure(true, true);
  hw.auto_complete = false;
  hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  hw.irq = IRQ_TX_DONE;  // Stale IRQ forces a successful preflight exchange.
  unsigned callbacks = 0;
  log.set_callback([&](const std::string &message) {
    if (message.find("cleared Warning flag") != std::string::npos) {
      callbacks++;
      EXPECT_EQ(hw.radio.start_transmit_async(&nested, hw.packet), SX126xError::NONE);
    }
  });
  EXPECT_EQ(hw.radio.start_transmit_async(&outer, hw.packet), SX126xError::SX126X_ERROR_BUSY);
  ASSERT_EQ(callbacks, 1u);
  EXPECT_TRUE(outer.results.empty());
  EXPECT_TRUE(nested.results.empty());
  EXPECT_EQ(hw.sends, 1u);
  // A lost listener also makes cancellation fail, without deliberately dereferencing null.
  ASSERT_TRUE(hw.radio.cancel_transmit_async(&nested));
  EXPECT_EQ(nested.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_CANCELLED}));
  EXPECT_TRUE(outer.results.empty());
}

TEST(SX126xReentry, PreflightWarningClearRechecksRadioReadiness) {
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion outer;
  hw.configure(true, true);
  hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  hw.irq = IRQ_TX_DONE;
  log.set_callback([&](const std::string &message) {
    if (message.find("cleared Warning flag") != std::string::npos) {
      hw.radio.set_mode_sleep(true);
    }
  });
  EXPECT_EQ(hw.radio.start_transmit_async(&outer, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_EQ(hw.sends, 0u);
  EXPECT_TRUE(outer.results.empty());
}

class SX126xPollingReentry : public ::testing::TestWithParam<std::tuple<unsigned, bool>> {};

TEST_P(SX126xPollingReentry, WarningCallbackCannotCompleteReplacementSend) {
  const auto [failed_command, reuse_listener] = GetParam();
  RadioHardware hw;
  RadioLogCallback log;
  ReentryCompletion first, second;
  auto *replacement = reuse_listener ? &first : &second;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&first, hw.packet), SX126xError::NONE);
  hw.clear_trace();
  hw.fail_at = failed_command;
  hw.irq = IRQ_TX_DONE;
  if (failed_command == 0) {
    hw.radio.status_set_warning(LOG_STR("Previous communication failure"));
  }
  unsigned callbacks = 0;
  size_t commands_after_replacement = 0;
  log.set_callback([&](const std::string &message) {
    if (callbacks == 0 && message.find("Warning flag") != std::string::npos) {
      callbacks++;
      hw.fail_at = 0;
      EXPECT_TRUE(hw.radio.cancel_transmit_async(&first));
      EXPECT_EQ(hw.radio.start_transmit_async(replacement, hw.packet), SX126xError::NONE);
      commands_after_replacement = hw.transactions.size();
    }
  });
  {
    LoopBlockingGuard clock{nullptr, nullptr, millis()};
    hw.radio.loop();
  }
  ASSERT_EQ(callbacks, 1u);
  EXPECT_EQ(first.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_CANCELLED}));
  EXPECT_TRUE(second.results.empty());
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.transactions.size(), commands_after_replacement);  // No stale IRQ clear or restoration.
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  ASSERT_EQ(replacement->results.size(), reuse_listener ? 2u : 1u);
  EXPECT_EQ(replacement->results.back(), SX126xError::NONE);
  EXPECT_FALSE(hw.radio.is_transmitting());
}

INSTANTIATE_TEST_SUITE_P(ReadSuccessReadFailureAndClearFailure, SX126xPollingReentry,
                         ::testing::Combine(::testing::Values(0u, 1u, 2u), ::testing::Bool()));

}  // namespace esphome::sx126x::testing
