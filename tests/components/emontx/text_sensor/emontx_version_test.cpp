#include <gtest/gtest.h>

#include <string>

#include "esphome/components/emontx/emontx.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../common.h"

namespace esphome::emontx::testing {

namespace {
class EmonTxVersionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    construct_app();
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_version_text_sensor(&this->version_);
  }

  void TearDown() override { cancel_timers(&this->sut_); }

  // Lets the delayed 'v' go out
  static void run_timers() { testing::run_timers(300); }

  std::string tx_() const { return std::string(this->uart_.tx.begin(), this->uart_.tx.end()); }

  MockUARTComponent uart_;
  EmonTx sut_;
  text_sensor::TextSensor version_;
};
}  // namespace

// 'v' must not reach the emonTx in the same burst as the line feed, or it joins the pending command
TEST_F(EmonTxVersionTest, SetupRequestsVersionAfterDelay) {
  this->sut_.setup();
  EXPECT_EQ(this->tx_(), "\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nv\n");
}

TEST_F(EmonTxVersionTest, SetupUnlocksBeforeRequestingVersion) {
  this->sut_.set_lock_on_reboot(LockOnReboot::LOCK_ON_REBOOT_ALWAYS);
  this->sut_.setup();
  EXPECT_EQ(this->tx_(), "\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nemonunlock\n\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nemonunlock\n\nv\n");
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
  EXPECT_EQ(this->tx_(), "\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nv\n");
}

TEST_F(EmonTxVersionTest, TextMsgCounterResetRequestsVersion) {
  this->uart_.push_rx("MSG:41,V1:230.1\r\nMSG:1,V1:230.4\r\n");
  this->sut_.loop();
  EXPECT_EQ(this->tx_(), "\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nv\n");
}

// The emonTx may have been flashed during the pause, with MSG already past its old value
TEST_F(EmonTxVersionTest, ResumeRequestsVersion) {
  this->sut_.set_paused(true);
  this->sut_.loop();
  this->sut_.set_paused(false);
  this->sut_.loop();
  EXPECT_EQ(this->tx_(), "\n");

  run_timers();
  EXPECT_EQ(this->tx_(), "\nv\n");
}

TEST_F(EmonTxVersionTest, RequestDroppedByPauseSentAgainOnResume) {
  this->sut_.setup();
  this->sut_.set_paused(true);
  this->sut_.loop();
  run_timers();
  EXPECT_EQ(this->tx_(), "\n");

  this->sut_.set_paused(false);
  this->sut_.loop();
  run_timers();
  EXPECT_EQ(this->tx_(), "\n\nv\n");
}

}  // namespace esphome::emontx::testing
