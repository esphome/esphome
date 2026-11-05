#include "common.h"

namespace esphome::ds248x::testing {

class DS248xOneWireBusTest : public DS248xTest {
 protected:
  void SetUp() override {
    DS248xTest::SetUp();
    this->wire_.set_parent(&this->hub_);
    this->wire_.set_channel(4);
  }

  DS248xOneWireBus wire_;
};

TEST_F(DS248xOneWireBusTest, FailedHubFailsBusSetupWithoutTraffic) {
  this->bus_.nack_all = true;
  this->hub_.setup();
  ASSERT_TRUE(this->hub_.is_failed());
  this->bus_.nack_all = false;

  const unsigned transfers = this->bus_.transfers;
  this->wire_.setup();
  EXPECT_TRUE(this->wire_.is_failed());
  EXPECT_TRUE(this->wire_.status_has_error());
  EXPECT_EQ(this->wire_.reset_int(), 0);
  EXPECT_EQ(this->bus_.transfers, transfers);
}

TEST_F(DS248xOneWireBusTest, ResetReportsNoPresenceWhileRecoveryPending) {
  this->setup_hub();
  this->fail_recovery();
  AppSetupState::set_complete(true);
  EXPECT_EQ(this->wire_.reset_int(), 0);
}

TEST_F(DS248xOneWireBusTest, ResetReportsBusErrorOnPlainI2CFailure) {
  this->setup_hub();
  this->bus_.nack_command = DS248X_COMMAND_CHANNELSELECT;
  EXPECT_EQ(this->wire_.reset_int(), -1);
  EXPECT_FALSE(this->hub_.is_recovery_pending());
}

TEST_F(DS248xOneWireBusTest, ResetReportsPresenceStateOnHealthyHub) {
  this->setup_hub();
  EXPECT_EQ(this->wire_.reset_int(), 0);
  EXPECT_EQ(this->bus_.channel, 4);
}

}  // namespace esphome::ds248x::testing
