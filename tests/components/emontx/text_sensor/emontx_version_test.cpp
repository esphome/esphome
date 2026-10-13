#include <gtest/gtest.h>

#include <string>

#include "esphome/components/emontx/emontx.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "../common.h"

namespace esphome::emontx::testing {

namespace {
class EmonTxVersionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // The test main does not construct App as generated code does
    static const bool app_constructed = (new (&App) Application(), true);
    (void) app_constructed;
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_version_text_sensor(&this->version_);
  }

  void TearDown() override {
    App.scheduler.cancel_timeout(&this->sut_, uint32_t{0});
    App.scheduler.call(millis());
  }

  // Lets the delayed 'v' go out
  static void run_timers() {
    App.scheduler.call(millis());
    delay(300);  // NOLINT - test only, the scheduler reads the real clock
    App.scheduler.call(millis());
  }

  std::string tx() const { return std::string(this->uart_.tx.begin(), this->uart_.tx.end()); }

  MockUARTComponent uart_;
  EmonTx sut_;
  text_sensor::TextSensor version_;
};
}  // namespace

// 'v' must not reach the emonTx in the same burst as the line feed, or it joins the pending command
TEST_F(EmonTxVersionTest, SetupRequestsVersionAfterDelay) {
  this->sut_.setup();
  EXPECT_EQ(this->tx(), "\n");

  run_timers();
  EXPECT_EQ(this->tx(), "\nv\n");
}

TEST_F(EmonTxVersionTest, SetupUnlocksBeforeRequestingVersion) {
  this->sut_.set_lock_on_reboot(true);
  this->sut_.setup();
  EXPECT_EQ(this->tx(), "\nemonunlock\n\n");

  run_timers();
  EXPECT_EQ(this->tx(), "\nemonunlock\n\nv\n");
}

TEST_F(EmonTxVersionTest, VersionLinePublished) {
  this->uart_.push_rx("> Firmware:\r\n  - Bootloader: UART (.bin)\r\n  - Version:    v1.1.1\r\n");
  this->sut_.loop();

  ASSERT_TRUE(this->version_.has_state());
  EXPECT_EQ(this->version_.get_state(), "v1.1.1");
}

TEST_F(EmonTxVersionTest, OtherLinesNotPublished) {
  this->uart_.push_rx("  - Build:      768be1b (Default)\r\n{\"P1\":100}\r\n  - Version:\r\n  - Version:   \r\n");
  this->sut_.loop();

  EXPECT_FALSE(this->version_.has_state());
}

// MSG restarts at 1 after a reboot, e.g. a firmware update
TEST_F(EmonTxVersionTest, MsgCounterResetRequestsVersion) {
  this->uart_.push_rx("{\"MSG\":41}\n{\"MSG\":42}\n");
  this->sut_.loop();
  EXPECT_TRUE(this->uart_.tx.empty());

  this->uart_.push_rx("{\"MSG\":1}\n");
  this->sut_.loop();
  EXPECT_EQ(this->tx(), "\n");

  run_timers();
  EXPECT_EQ(this->tx(), "\nv\n");
}

TEST_F(EmonTxVersionTest, PauseWithoutRebootSendsNothing) {
  this->uart_.push_rx("{\"MSG\":41}\n");
  this->sut_.loop();
  this->sut_.set_paused(true);
  this->sut_.loop();
  this->sut_.set_paused(false);
  this->uart_.push_rx("{\"MSG\":45}\n");
  this->sut_.loop();

  EXPECT_TRUE(this->uart_.tx.empty());
}

}  // namespace esphome::emontx::testing
