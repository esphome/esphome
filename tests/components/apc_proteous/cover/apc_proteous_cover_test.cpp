#include <gtest/gtest.h>

#include "esphome/core/application.h"

#include "../common.h"

namespace esphome::apc_proteous::testing {

using cover::COVER_CLOSED;
using cover::COVER_OPEN;
using cover::COVER_OPERATION_CLOSING;
using cover::COVER_OPERATION_IDLE;
using cover::COVER_OPERATION_OPENING;

namespace {
class APCProteousCoverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->cover_.set_uart_parent(&this->uart_);
    this->cover_.setup();
    // setup() may restore a saved position, so start every test from a known state.
    this->cover_.position = 0.5f;
    this->cover_.current_operation = COVER_OPERATION_IDLE;
    this->cover_.add_on_state_callback([this]() { this->publishes_++; });
    this->end_quiet_period_();
  }

  void feed_(const char *data) {
    this->uart_.push_rx(data);
    this->cover_.loop();
  }

  void end_quiet_period_() {
    this->cover_.last_command_tx_ =
        App.get_loop_component_start_time() - TestableAPCProteousCover::COMMAND_QUIET_MS - 1;
  }

  // Pretend the pending command was sent long enough ago for a retry to be due.
  void expire_ack_window_() {
    this->cover_.pending_command_time_ =
        App.get_loop_component_start_time() - TestableAPCProteousCover::COMMAND_ACK_MS - 1;
    this->end_quiet_period_();
  }

  void send_position_(float position) { this->cover_.make_call().set_position(position).perform(); }

  MockUARTComponent uart_;
  TestableAPCProteousCover cover_;
  int publishes_{0};
};
}  // namespace

TEST_F(APCProteousCoverTest, TraitsSupportPositionStopAndToggle) {
  auto traits = this->cover_.get_traits();
  EXPECT_TRUE(traits.get_supports_position());
  EXPECT_TRUE(traits.get_supports_stop());
  EXPECT_TRUE(traits.get_supports_toggle());
  EXPECT_FALSE(traits.get_is_assumed_state());
}

// s-status bit 0 is "operating", bit 1 is the direction (set when closing).
TEST_F(APCProteousCoverTest, SStatusSetsOperation) {
  this->feed_("?s=01\r");
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_OPENING);
  EXPECT_EQ(this->publishes_, 1);

  this->feed_("?s=03\r");
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_CLOSING);
  EXPECT_EQ(this->publishes_, 2);

  this->feed_("?s=02\r");
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->publishes_, 3);
}

TEST_F(APCProteousCoverTest, UnchangedStatusDoesNotPublish) {
  this->feed_("?s=00\r");
  EXPECT_EQ(this->publishes_, 0);
  this->feed_("?x=32\r");
  EXPECT_EQ(this->publishes_, 0);
}

// x-status is the position in percent, sent as hex.
TEST_F(APCProteousCoverTest, XStatusSetsPosition) {
  this->feed_("?x=19\r");
  EXPECT_FLOAT_EQ(this->cover_.position, 0.25f);
  EXPECT_EQ(this->publishes_, 1);

  this->feed_("?x=4B\r");
  EXPECT_FLOAT_EQ(this->cover_.position, 0.75f);
  EXPECT_EQ(this->publishes_, 2);
}

TEST_F(APCProteousCoverTest, XStatusNearEndsSnapsToEndpoints) {
  this->feed_("?x=62\r");  // 98%
  EXPECT_FLOAT_EQ(this->cover_.position, COVER_OPEN);
  this->feed_("?x=02\r");  // 2%
  EXPECT_FLOAT_EQ(this->cover_.position, COVER_CLOSED);
  this->feed_("?x=03\r");  // 3% is outside the margin
  EXPECT_FLOAT_EQ(this->cover_.position, 0.03f);
  this->feed_("?x=61\r");  // 97% is outside the margin
  EXPECT_FLOAT_EQ(this->cover_.position, 0.97f);
}

TEST_F(APCProteousCoverTest, MalformedFramesAreIgnored) {
  this->feed_("?x\r");     // too short
  this->feed_("!x=10\r");  // wrong start character
  this->feed_("?x:10\r");  // missing '='
  this->feed_("?x=zz\r");  // not hex
  this->feed_("?q=10\r");  // unknown type
  EXPECT_FLOAT_EQ(this->cover_.position, 0.5f);
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->publishes_, 0);
  EXPECT_EQ(this->cover_.rx_len_, 0);
}

TEST_F(APCProteousCoverTest, LineFeedsAreIgnored) {
  this->feed_("\n?x=\n19\r\n");
  EXPECT_FLOAT_EQ(this->cover_.position, 0.25f);
}

TEST_F(APCProteousCoverTest, FrameSplitAcrossReadsIsAssembled) {
  this->feed_("?x=");
  EXPECT_EQ(this->publishes_, 0);
  this->feed_("19\r");
  EXPECT_FLOAT_EQ(this->cover_.position, 0.25f);
}

// Noise without a carriage return must not grow the buffer without limit, nor stop the
// next frame from being read.
TEST_F(APCProteousCoverTest, UnterminatedNoiseIsDiscarded) {
  this->feed_("0123456789ABCDEF");
  this->feed_("?x=19\r");
  EXPECT_FLOAT_EQ(this->cover_.position, 0.25f);

  this->feed_("0123456789ABCDEF0123456789ABCDEF");
  EXPECT_LE(this->cover_.rx_len_, TestableAPCProteousCover::MAX_RESPONSE_LEN);
}

TEST_F(APCProteousCoverTest, OpenSendsOpenCommand) {
  this->cover_.make_call().set_command_open().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*6\r");
  EXPECT_NE(this->cover_.pending_command_, nullptr);
}

TEST_F(APCProteousCoverTest, CloseSendsCloseCommand) {
  this->cover_.make_call().set_command_close().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*7\r");
  EXPECT_NE(this->cover_.pending_command_, nullptr);
}

// The published state only changes once the controller reports it.
TEST_F(APCProteousCoverTest, CommandDoesNotPublishState) {
  this->cover_.make_call().set_command_open().perform();
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->publishes_, 0);
}

TEST_F(APCProteousCoverTest, OpenIsNotResentWhileAlreadyOpening) {
  this->feed_("?s=01\r");
  this->cover_.make_call().set_command_open().perform();
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, CloseIsNotResentWhileAlreadyClosing) {
  this->feed_("?s=03\r");
  this->cover_.make_call().set_command_close().perform();
  EXPECT_TRUE(this->uart_.tx.empty());
}

// An open command reverses a closing gate.
TEST_F(APCProteousCoverTest, OpenIsSentWhileClosing) {
  this->feed_("?s=03\r");
  this->cover_.make_call().set_command_open().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*6\r");
}

// The stop command toggles start/stop, so sending it to an idle gate would start it.
TEST_F(APCProteousCoverTest, StopIsIgnoredWhenIdle) {
  this->cover_.make_call().set_command_stop().perform();
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, StopIsSentWhenMoving) {
  this->feed_("?s=01\r");
  this->cover_.make_call().set_command_stop().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
}

// A stop straight after an open must cancel it, even before the controller reports motion.
TEST_F(APCProteousCoverTest, StopCancelsUnacknowledgedCommand) {
  this->cover_.make_call().set_command_open().perform();
  this->uart_.tx.clear();
  this->cover_.make_call().set_command_stop().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
}

TEST_F(APCProteousCoverTest, StopClearsPartialTarget) {
  this->send_position_(0.8f);
  this->feed_("?s=01\r");
  this->cover_.make_call().set_command_stop().perform();
  EXPECT_FALSE(this->cover_.target_position_.has_value());
}

TEST_F(APCProteousCoverTest, ToggleSendsStartCommand) {
  this->cover_.make_call().set_command_toggle().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
}

TEST_F(APCProteousCoverTest, ToggleClearsPendingCommandAndTarget) {
  this->send_position_(0.8f);
  this->uart_.tx.clear();
  this->cover_.make_call().set_command_toggle().perform();
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
  EXPECT_FALSE(this->cover_.target_position_.has_value());
}

TEST_F(APCProteousCoverTest, PartialPositionAboveCurrentOpens) {
  this->send_position_(0.8f);
  EXPECT_EQ(this->uart_.tx_string(), "*6\r");
}

TEST_F(APCProteousCoverTest, PartialPositionBelowCurrentCloses) {
  this->send_position_(0.2f);
  EXPECT_EQ(this->uart_.tx_string(), "*7\r");
}

TEST_F(APCProteousCoverTest, PartialPositionAtCurrentSendsNothing) {
  this->send_position_(0.5f);
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, StopIsSentWhenOpeningReachesTarget) {
  this->send_position_(0.7f);
  this->feed_("?s=01\r");
  this->uart_.tx.clear();

  this->feed_("?x=3C\r");  // 60%, short of the target
  EXPECT_TRUE(this->uart_.tx.empty());
  this->feed_("?x=46\r");  // 70%
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
  EXPECT_FALSE(this->cover_.target_position_.has_value());

  // The gate keeps moving until the controller reports it idle; the stop is not repeated.
  this->uart_.tx.clear();
  this->feed_("?x=48\r");
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, StopIsSentWhenClosingReachesTarget) {
  this->send_position_(0.3f);
  this->feed_("?s=03\r");
  this->uart_.tx.clear();

  this->feed_("?x=28\r");  // 40%, short of the target
  EXPECT_TRUE(this->uart_.tx.empty());
  this->feed_("?x=1C\r");  // 28%, past the target
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
}

// A full open runs to the limit switch, so the position must not trigger a stop.
TEST_F(APCProteousCoverTest, FullOpenIsNotStoppedByPosition) {
  this->cover_.make_call().set_command_open().perform();
  this->feed_("?s=01\r");
  this->uart_.tx.clear();
  this->feed_("?x=50\r");
  this->feed_("?x=64\r");
  EXPECT_TRUE(this->uart_.tx.empty());
}

// A full open replaces an earlier partial target.
TEST_F(APCProteousCoverTest, FullOpenCancelsPartialTarget) {
  this->send_position_(0.7f);
  this->cover_.make_call().set_command_open().perform();
  EXPECT_FALSE(this->cover_.target_position_.has_value());
  this->feed_("?s=01\r");
  this->uart_.tx.clear();
  this->feed_("?x=46\r");
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, ReportedMotionAcknowledgesCommand) {
  this->cover_.make_call().set_command_open().perform();
  this->feed_("?s=03\r");  // motion in the wrong direction does not count
  EXPECT_NE(this->cover_.pending_command_, nullptr);
  this->feed_("?s=01\r");
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
  EXPECT_EQ(this->cover_.command_retries_, 0);
}

TEST_F(APCProteousCoverTest, CommandIsNotRetriedBeforeAckTimeout) {
  this->cover_.make_call().set_command_open().perform();
  this->uart_.tx.clear();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string().find("*6\r"), std::string::npos);
  EXPECT_EQ(this->cover_.command_retries_, 0);
}

TEST_F(APCProteousCoverTest, UnacknowledgedCommandIsRetried) {
  this->cover_.make_call().set_command_close().perform();
  this->uart_.tx.clear();
  this->expire_ack_window_();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string(), "*7\r");
  EXPECT_EQ(this->cover_.command_retries_, 1);
}

TEST_F(APCProteousCoverTest, RetriesStopAfterLimit) {
  this->cover_.make_call().set_command_open().perform();
  for (int i = 0; i < TestableAPCProteousCover::MAX_COMMAND_RETRIES; i++) {
    this->uart_.tx.clear();
    this->expire_ack_window_();
    this->cover_.update();
    EXPECT_EQ(this->uart_.tx_string(), "*6\r");
  }
  this->uart_.tx.clear();
  this->expire_ack_window_();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string().find("*6\r"), std::string::npos);
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
  EXPECT_EQ(this->cover_.command_retries_, 0);
}

TEST_F(APCProteousCoverTest, PartialTargetIsClearedWhenRetriesRunOut) {
  this->send_position_(0.8f);
  for (int i = 0; i <= TestableAPCProteousCover::MAX_COMMAND_RETRIES; i++) {
    this->expire_ack_window_();
    this->cover_.update();
  }
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
  EXPECT_FALSE(this->cover_.target_position_.has_value());
}

// While a reversing command is unconfirmed, the reported direction is still the old one and must
// not be compared with the target.
TEST_F(APCProteousCoverTest, ReversingPartialMoveWaitsForConfirmedDirection) {
  this->feed_("?s=01\r");
  this->send_position_(0.3f);
  EXPECT_EQ(this->uart_.tx_string(), "*7\r");
  this->uart_.tx.clear();

  this->feed_("?x=28\r");  // 40%, still reported as opening
  EXPECT_TRUE(this->uart_.tx.empty());

  this->feed_("?s=03\r");
  this->feed_("?x=1C\r");  // 28%, past the target
  EXPECT_EQ(this->uart_.tx_string(), "*1\r");
}

// The gate reports idle right after a command, before it starts moving; that must not drop the target.
TEST_F(APCProteousCoverTest, PartialTargetIsKeptWhileCommandIsUnconfirmed) {
  this->send_position_(0.8f);
  this->feed_("?s=00\r");
  EXPECT_TRUE(this->cover_.target_position_.has_value());
}

// A partial move that stops short of its target must not stop a later move started from the remote.
TEST_F(APCProteousCoverTest, PartialTargetIsClearedWhenConfirmedMoveStops) {
  this->send_position_(0.8f);
  this->feed_("?s=01\r");
  this->feed_("?s=00\r");
  EXPECT_FALSE(this->cover_.target_position_.has_value());

  this->uart_.tx.clear();
  this->feed_("?s=01\r");
  this->feed_("?x=55\r");  // 85%
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(APCProteousCoverTest, CommandIsNotRetriedOnceAtEndpoint) {
  this->cover_.make_call().set_command_open().perform();
  this->feed_("?x=64\r");
  this->uart_.tx.clear();
  this->expire_ack_window_();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string().find("*6\r"), std::string::npos);
  EXPECT_EQ(this->cover_.pending_command_, nullptr);
}

TEST_F(APCProteousCoverTest, UpdateAlternatesQueries) {
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string(), "?s\r");
  this->uart_.tx.clear();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string(), "?x\r");
  this->uart_.tx.clear();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string(), "?s\r");
}

TEST_F(APCProteousCoverTest, UpdateHoldsOffPollingAfterCommand) {
  this->cover_.make_call().set_command_open().perform();
  this->uart_.tx.clear();
  this->cover_.update();
  EXPECT_TRUE(this->uart_.tx.empty());

  this->end_quiet_period_();
  this->cover_.update();
  EXPECT_EQ(this->uart_.tx_string(), "?s\r");
}

// A command echo left without a terminator must not merge with the next response.
TEST_F(APCProteousCoverTest, UpdateDiscardsStaleEcho) {
  this->feed_("*6");
  EXPECT_NE(this->cover_.rx_len_, 0);
  this->cover_.update();
  EXPECT_EQ(this->cover_.rx_len_, 0);
  this->feed_("?s=01\r");
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_OPENING);
}

}  // namespace esphome::apc_proteous::testing
