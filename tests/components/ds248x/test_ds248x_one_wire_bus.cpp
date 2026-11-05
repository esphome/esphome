#include <algorithm>
#include <iterator>

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

TEST_F(DS248xOneWireBusTest, ResetReportsNoPresenceWhenResetStartsFailedRecovery) {
  this->setup_hub();
  this->bus_.stick_busy = true;
  this->bus_.nack_command = DS248X_COMMAND_RESET;
  EXPECT_EQ(this->wire_.reset_int(), 0);
  EXPECT_TRUE(this->hub_.is_recovery_pending());
}

TEST_F(DS248xOneWireBusTest, ReadsReturnWireBytesOnHealthyHub) {
  static constexpr uint8_t BYTES[8] = {0x28, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  this->setup_hub();
  std::copy(std::begin(BYTES), std::end(BYTES), this->bus_.wire_bytes);
  EXPECT_EQ(this->wire_.read8(), 0x28);
  this->bus_.byte_reads = 0;
  EXPECT_EQ(this->wire_.read64(), 0x0706050403020128ULL);
}

TEST_F(DS248xOneWireBusTest, ReadsReturnAllOnesWithoutTrafficOnFailedHub) {
  this->bus_.nack_all = true;
  this->hub_.setup();
  ASSERT_TRUE(this->hub_.is_failed());
  this->bus_.nack_all = false;

  const unsigned transfers = this->bus_.transfers;
  EXPECT_EQ(this->wire_.read8(), 0xFF);
  EXPECT_EQ(this->wire_.read64(), UINT64_MAX);
  EXPECT_EQ(this->bus_.transfers, transfers);
}

TEST_F(DS248xOneWireBusTest, ReadsReturnAllOnesWithoutTrafficDuringBackoff) {
  this->setup_hub();
  this->fail_recovery();
  AppSetupState::set_complete(true);

  const unsigned transfers = this->bus_.transfers;
  EXPECT_EQ(this->wire_.read8(), 0xFF);
  EXPECT_EQ(this->wire_.read64(), UINT64_MAX);
  EXPECT_EQ(this->bus_.transfers, transfers);
}

TEST_F(DS248xOneWireBusTest, Read8ReturnsIdleByteOnI2CFailure) {
  this->setup_hub();
  this->bus_.nack_byte_read = 0;
  EXPECT_EQ(this->wire_.read8(), 0xFF);
}

TEST_F(DS248xOneWireBusTest, Read64ReturnsAllOnesWhenALaterByteFails) {
  this->setup_hub();
  this->bus_.nack_byte_read = 3;
  EXPECT_EQ(this->wire_.read64(), UINT64_MAX);
  EXPECT_EQ(this->bus_.byte_reads, 3);
}

}  // namespace esphome::ds248x::testing
