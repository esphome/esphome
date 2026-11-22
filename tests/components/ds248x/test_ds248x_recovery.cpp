#include "common.h"

namespace esphome::ds248x::testing {

TEST_F(DS248xTest, BusyTimeoutRecoveryReselectsChannel) {
  this->setup_hub();
  ASSERT_TRUE(this->hub_.select_channel(2));
  const unsigned selects = this->bus_.channel_selects;
  this->hub_.status_set_warning();

  this->bus_.stick_busy = true;
  bool presence = false;
  EXPECT_FALSE(this->hub_.ow_reset(presence));

  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_FALSE(this->hub_.is_recovery_pending());
  EXPECT_FALSE(this->hub_.status_has_warning());
  // The device reset moved the chip back to channel 0, so the cached channel must not be trusted
  EXPECT_TRUE(this->hub_.select_channel(2));
  EXPECT_EQ(this->bus_.channel_selects, selects + 1);
  EXPECT_EQ(this->bus_.channel, 2);
}

TEST_F(DS248xTest, FailedRecoveryKeepsHubRunning) {
  this->setup_hub();
  this->fail_recovery();

  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_TRUE(this->hub_.is_recovery_pending());
  EXPECT_TRUE(this->hub_.status_has_warning());
}

TEST_F(DS248xTest, PendingRecoveryBacksOffAfterSetup) {
  this->setup_hub();
  this->fail_recovery();
  AppSetupState::set_complete(true);

  unsigned transfers = this->bus_.transfers;
  EXPECT_FALSE(this->hub_.select_channel(2));
  EXPECT_EQ(this->bus_.transfers, transfers);

  // A retry after the window that fails again starts a new window
  this->bus_.nack_command = DS248X_COMMAND_RESET;
  this->hub_.expire_backoff();
  EXPECT_FALSE(this->hub_.select_channel(2));
  EXPECT_GT(this->bus_.transfers, transfers);
  EXPECT_TRUE(this->hub_.is_recovery_pending());
  transfers = this->bus_.transfers;
  EXPECT_FALSE(this->hub_.select_channel(2));
  EXPECT_EQ(this->bus_.transfers, transfers);

  this->bus_.nack_command = NO_COMMAND;
  this->hub_.expire_backoff();
  EXPECT_TRUE(this->hub_.select_channel(2));
  EXPECT_FALSE(this->hub_.is_recovery_pending());
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_EQ(this->bus_.channel, 2);
}

TEST_F(DS248xTest, PendingRecoveryRetriesImmediatelyDuringSetup) {
  this->setup_hub();
  this->fail_recovery();

  // A sibling bus set up right after the failure must not wait out the retry window
  DS248xOneWireBus sibling;
  sibling.set_parent(&this->hub_);
  sibling.set_channel(1);
  sibling.setup();

  EXPECT_FALSE(sibling.is_failed());
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_FALSE(this->hub_.is_recovery_pending());
  EXPECT_EQ(this->bus_.channel, 1);
}

TEST_F(DS248xTest, ChannelStaysCachedWhenReadPointerRestoreFails) {
  this->setup_hub();
  this->bus_.nack_command = DS248X_COMMAND_SETREADPTR;
  const unsigned selects = this->bus_.channel_selects;
  EXPECT_FALSE(this->hub_.select_channel(3));
  EXPECT_EQ(this->bus_.channel_selects, selects + 1);
  EXPECT_EQ(this->bus_.channel, 3);

  this->bus_.nack_command = NO_COMMAND;
  EXPECT_TRUE(this->hub_.select_channel(3));
  EXPECT_EQ(this->bus_.channel_selects, selects + 1);
}

TEST_F(DS248xTest, FailedConfigureKeepsRecoveryPending) {
  this->hub_.set_active_pullup(true);
  this->setup_hub();
  ASSERT_EQ(this->bus_.config(), DS248X_CONFIG_ACTIVE_PULLUP);
  DS248xOneWireBus wire;
  wire.set_parent(&this->hub_);
  wire.set_channel(2);

  // The device reset is acknowledged, but the chip refuses its configuration and keeps the reset defaults
  this->bus_.stick_busy = true;
  this->bus_.nack_command = DS248X_COMMAND_WRITECONFIG;
  bool presence = false;
  EXPECT_FALSE(this->hub_.ow_reset(presence));
  EXPECT_TRUE(this->hub_.is_recovery_pending());
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->bus_.config(), 0);
  this->bus_.nack_command = NO_COMMAND;
  AppSetupState::set_complete(true);

  const unsigned commands = this->bus_.one_wire_commands;
  EXPECT_EQ(wire.reset_int(), 0);
  EXPECT_EQ(this->bus_.one_wire_commands, commands);

  this->hub_.expire_backoff();
  EXPECT_EQ(wire.reset_int(), 0);
  EXPECT_EQ(this->bus_.one_wire_commands, commands + 1);
  EXPECT_EQ(this->bus_.config(), DS248X_CONFIG_ACTIVE_PULLUP);
  EXPECT_EQ(this->bus_.channel, 2);
  EXPECT_FALSE(this->hub_.is_recovery_pending());
  EXPECT_FALSE(this->hub_.status_has_warning());
}

}  // namespace esphome::ds248x::testing
