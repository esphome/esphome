#include "sx127x_test_helpers.h"
#include "esphome/components/logger/logger.h"
#include <string>

namespace esphome::sx127x::testing {

// Runs a callback for each warning logged, as a logger on_message automation would
class LogCallback {
 public:
  LogCallback() : previous_(logger::global_logger) {
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
    this->logger_.add_log_callback(this,
                                   [](void *instance, uint8_t level, const char *tag, const char *message, size_t len) {
                                     auto *self = static_cast<LogCallback *>(instance);
                                     if (self->callback_) {
                                       self->callback_(std::string(message, len));
                                     }
                                   });
  }
  ~LogCallback() { logger::global_logger = this->previous_; }
  void set_callback(std::function<void(const std::string &)> callback) { this->callback_ = std::move(callback); }

 private:
  std::function<void(const std::string &)> callback_;
  logger::Logger *previous_;
  logger::Logger logger_{0};
};

// A configure() from the warning's log callback recovers the radio, and no retry is left
// behind to reset it again later
TEST(SX127xReentry, ConfigureFromTheFailureWarningLeavesNoRetry) {
  RadioHardware hw;
  LogCallback log;
  hw.configure(true, true);
  bool reentered = false;
  log.set_callback([&](const std::string &message) {
    if (!reentered && message.find("set Warning flag") != std::string::npos) {
      reentered = true;
      hw.stuck = false;
      hw.radio.configure();
    }
  });
  hw.stuck = true;
  hw.radio.set_mode_standby();
  ASSERT_TRUE(reentered);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving());
  hw.radio.set_mode_sleep();
  run_scheduler(1100);
  EXPECT_EQ(hw.mode(), MODE_SLEEP);
}

}  // namespace esphome::sx127x::testing
