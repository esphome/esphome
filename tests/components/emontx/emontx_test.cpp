#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/emontx/emontx.h"
#include "common.h"

namespace esphome::emontx::testing {

namespace {
class EmonTxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    construct_app();
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.add_on_data_callback([this](StringRef line) { this->lines_.emplace_back(line.c_str(), line.size()); });
    this->sut_.add_on_json_callback([this](JsonObject, StringRef) { this->json_count_++; });
  }

  void TearDown() override { cancel_timers(&this->sut_); }

  std::string tx_() const { return std::string(this->uart_.tx.begin(), this->uart_.tx.end()); }

  MockUARTComponent uart_;
  EmonTx sut_;
  std::vector<std::string> lines_;
  int json_count_{0};
};
}  // namespace

TEST_F(EmonTxTest, LinePassedToDataCallbacks) {
  this->uart_.push_rx("MSG:1\r\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:1");
  EXPECT_EQ(this->json_count_, 0);
}

TEST_F(EmonTxTest, JsonLineParsed) {
  this->uart_.push_rx("{\"P1\":100}\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->json_count_, 1);
}

// The CR check must come before the overflow check, or a full line ending in CRLF would overflow.
TEST_F(EmonTxTest, FullLengthLineWithCrLfDelivered) {
  this->uart_.push_rx(MAX_LINE_LENGTH, 'a');
  this->uart_.push_rx("\r\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], std::string(MAX_LINE_LENGTH, 'a'));
}

TEST_F(EmonTxTest, FullLengthLineWithLfDelivered) {
  this->uart_.push_rx(MAX_LINE_LENGTH, 'a');
  this->uart_.push_rx("\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], std::string(MAX_LINE_LENGTH, 'a'));
}

TEST_F(EmonTxTest, OverflowedLineDiscardedUntilLineFeed) {
  // One byte more than the buffer holds, then more of the same line.
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->uart_.push_rx("tail\n");
  this->uart_.push_rx("MSG:2\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:2");
}

TEST_F(EmonTxTest, OverflowedLineTailNotParsedAsJson) {
  // The tail after the overflow looks like a JSON line and must not be parsed.
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->uart_.push_rx("{\"P1\":100}\n");
  this->sut_.loop();

  EXPECT_TRUE(this->lines_.empty());
  EXPECT_EQ(this->json_count_, 0);
}

TEST_F(EmonTxTest, OverflowAcrossLoopCalls) {
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->sut_.loop();
  this->uart_.push_rx("tail\nMSG:3\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:3");
}

TEST_F(EmonTxTest, LoopLeavesBytesUntouchedWhenPaused) {
  this->uart_.push_rx("{\"P1\":123}\n");
  this->sut_.set_paused(true);
  this->sut_.loop();

  EXPECT_EQ(this->uart_.available(), std::strlen("{\"P1\":123}\n"));
  EXPECT_TRUE(this->lines_.empty());
}

TEST_F(EmonTxTest, SendCommandIsSuppressedWhilePaused) {
  this->sut_.set_paused(true);
  this->sut_.send_command("x");
  EXPECT_TRUE(this->uart_.tx.empty());

  this->sut_.set_paused(false);
  this->sut_.send_command("x");
  EXPECT_EQ(std::string(this->uart_.tx.begin(), this->uart_.tx.end()), "x\n");
}

// Neither the part before the pause nor the part after it may reach on_data
TEST_F(EmonTxTest, ResumeDiscardsInterruptedLine) {
  this->uart_.push_rx("{\"P1\":4");
  this->sut_.loop();
  this->sut_.set_paused(true);
  this->sut_.loop();
  this->sut_.set_paused(false);

  this->uart_.push_rx("56}\n");
  this->sut_.loop();
  EXPECT_TRUE(this->lines_.empty());

  this->uart_.push_rx("{\"P1\":789}\n");
  this->sut_.loop();
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "{\"P1\":789}");
}

TEST_F(EmonTxTest, ResumeDropsBacklogReceivedWhilePaused) {
  this->sut_.set_paused(true);
  this->sut_.loop();
  this->uart_.push_rx("{\"P1\":1}\n{\"P1\":2}\n{\"P1\":");
  this->sut_.set_paused(false);

  this->sut_.loop();
  EXPECT_EQ(this->uart_.available(), 0u);
  EXPECT_TRUE(this->lines_.empty());

  this->uart_.push_rx("3}\n{\"P1\":4}\n");
  this->sut_.loop();
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "{\"P1\":4}");
}

TEST_F(EmonTxTest, ResumeAfterCompleteBacklogKeepsNextLine) {
  this->sut_.set_paused(true);
  this->sut_.loop();
  this->uart_.push_rx("{\"P1\":1}\n");
  this->sut_.set_paused(false);

  this->sut_.loop();
  this->uart_.push_rx("{\"P1\":2}\n");
  this->sut_.loop();
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "{\"P1\":2}");
}

TEST_F(EmonTxTest, ShutdownLocksWhenEnabled) {
  EXPECT_CALL(this->uart_, flush()).Times(1);
  this->sut_.set_lock_on_reboot(LockOnReboot::LOCK_ON_REBOOT_ALWAYS);
  this->sut_.on_shutdown();
  EXPECT_EQ(std::string(this->uart_.tx.begin(), this->uart_.tx.end()), "emonlock\n");
}

// Firmware before v1.2.0 reads "emon..." as the bootloader command
TEST_F(EmonTxTest, ShutdownSendsNothingByDefault) {
  EXPECT_CALL(this->uart_, flush()).Times(0);
  this->sut_.on_shutdown();
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(EmonTxTest, ShutdownSendsNothingWhilePaused) {
  EXPECT_CALL(this->uart_, flush()).Times(0);
  this->sut_.set_lock_on_reboot(LockOnReboot::LOCK_ON_REBOOT_ALWAYS);
  this->sut_.set_paused(true);
  this->sut_.on_shutdown();
  EXPECT_TRUE(this->uart_.tx.empty());
}

// The emonTx would join "emonunlock" to the pending empty line if both came in one burst
TEST_F(EmonTxTest, SetupUnlocksAfterLineFeedWhenEnabled) {
  this->sut_.set_lock_on_reboot(LockOnReboot::LOCK_ON_REBOOT_ALWAYS);
  this->sut_.setup();
  EXPECT_EQ(this->tx_(), "\n");

  run_timers(300);
  EXPECT_EQ(this->tx_(), "\nemonunlock\n");
}

TEST_F(EmonTxTest, SetupSendsNothingByDefault) {
  this->sut_.setup();
  EXPECT_TRUE(this->uart_.tx.empty());
}

class EmonTxLockAutoTest : public EmonTxTest {
 protected:
  void SetUp() override {
    EmonTxTest::SetUp();
    this->sut_.set_lock_on_reboot(LockOnReboot::LOCK_ON_REBOOT_AUTO);
  }

  void report_version_(const char *version) {
    this->uart_.push_rx("  - Version:    ");
    this->uart_.push_rx(version);
    this->uart_.push_rx("\r\n");
    this->sut_.loop();
  }
};

// Only 'v' goes out until the reply shows the firmware knows 'emonunlock'
TEST_F(EmonTxLockAutoTest, SetupOnlyRequestsVersion) {
  this->sut_.setup();
  run_timers(300);
  EXPECT_EQ(this->tx_(), "\nv\n");
}

TEST_F(EmonTxLockAutoTest, SupportedFirmwareUnlockedAndLockedOnShutdown) {
  this->report_version_("v1.2.0");
  EXPECT_TRUE(this->uart_.tx.empty());
  run_timers(300);
  EXPECT_EQ(this->tx_(), "emonunlock\n");

  EXPECT_CALL(this->uart_, flush()).Times(1);
  this->sut_.on_shutdown();
  EXPECT_EQ(this->tx_(), "emonunlock\nemonlock\n");
}

// Firmware before v1.2.0 reads "emon..." as the bootloader command
TEST_F(EmonTxLockAutoTest, OlderFirmwareGetsNoLockCommands) {
  this->report_version_("v1.1.1");
  run_timers(300);
  EXPECT_CALL(this->uart_, flush()).Times(0);
  this->sut_.on_shutdown();
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(EmonTxLockAutoTest, NothingSentWithoutVersion) {
  EXPECT_CALL(this->uart_, flush()).Times(0);
  this->sut_.on_shutdown();
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(EmonTxLockAutoTest, VersionFormats) {
  for (const char *version : {"V1.10.0", "v2.0.0", "1.2.0", "v1.3.0-rc1"}) {
    this->report_version_("v0.99.0");  // So the next one is a change
    this->report_version_(version);
    EXPECT_CALL(this->uart_, flush()).Times(1);
    this->sut_.on_shutdown();
    EXPECT_NE(this->tx_().find("emonlock"), std::string::npos) << version;
    ::testing::Mock::VerifyAndClearExpectations(&this->uart_);
    this->uart_.tx.clear();
  }
  for (const char *version : {"v1.1.1", "v0.99.9-test", "None", "768be1b", "v1", "v.2"}) {
    this->report_version_(version);
    EXPECT_CALL(this->uart_, flush()).Times(0);
    this->sut_.on_shutdown();
    EXPECT_TRUE(this->uart_.tx.empty()) << version;
    ::testing::Mock::VerifyAndClearExpectations(&this->uart_);
  }
}

// The emonTx clears its lock when it reboots, so a repeated report needs no second unlock
TEST_F(EmonTxLockAutoTest, UnlockedOnlyOnce) {
  this->report_version_("v1.2.0");
  run_timers(300);
  this->report_version_("v1.2.0");
  run_timers(300);
  EXPECT_EQ(this->tx_(), "emonunlock\n");
}

TEST_F(EmonTxLockAutoTest, RequestRetriedWithoutReply) {
  this->sut_.setup();
  run_timers(2100);
  EXPECT_EQ(this->tx_(), "\nv\n\n");

  this->report_version_("v1.1.1");
  run_timers(2100);
  EXPECT_EQ(this->tx_(), "\nv\n\nv\n");
}

}  // namespace esphome::emontx::testing
