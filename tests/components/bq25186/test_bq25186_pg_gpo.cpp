#include "common.h"
#include "esphome/components/switch/switch.h"

namespace esphome::bq25186::testing {

static constexpr uint8_t SYS_REG_OTHER_BITS = 0x0F;

// The body code generation emits for apply= with publish="on_success"
static BQ25186Component *hub_under_test = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void apply_pg_gpo(switch_::Switch *entity, bool state) {
  if (hub_under_test->write_pg_gpo_level(state))
    entity->publish_state(state);
}

class BQ25186PgGpoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.registers[BQ25186_REG_MASK_ID] = 0x01;  // the BQ25186 device id
    this->bus_.registers[BQ25186_REG_SYS_REG] = SYS_REG_OTHER_BITS;
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(ADDRESS);
    hub_under_test = &this->hub_;
    this->switch_.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    this->hub_.set_pg_gpo_switch(&this->switch_);
  }

  bool level_() const { return (this->bus_.registers[BQ25186_REG_SYS_REG] & BQ25186_SYS_REG_PG_GPO_LEVEL) != 0; }
  uint8_t other_bits_() const { return this->bus_.registers[BQ25186_REG_SYS_REG] & ~BQ25186_SYS_REG_PG_GPO_LEVEL; }

  RegisterBus bus_;
  BQ25186Component hub_;
  switch_::ApplySwitch<apply_pg_gpo> switch_;
};

TEST_F(BQ25186PgGpoTest, TurnOnSetsTheLevelAndPublishes) {
  this->switch_.turn_on();
  EXPECT_TRUE(this->level_());
  EXPECT_EQ(this->other_bits_(), SYS_REG_OTHER_BITS);
  EXPECT_TRUE(this->switch_.has_state());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, TurnOffClearsTheLevelAndPublishes) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.publish_state(true);
  this->switch_.turn_off();
  EXPECT_FALSE(this->level_());
  EXPECT_EQ(this->other_bits_(), SYS_REG_OTHER_BITS);
  EXPECT_FALSE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, FailedWriteLeavesTheStateUnpublished) {
  this->bus_.fail_writes = true;
  this->switch_.turn_on();
  EXPECT_FALSE(this->level_());
  EXPECT_FALSE(this->switch_.has_state());
}

TEST_F(BQ25186PgGpoTest, FailedWriteKeepsThePreviousState) {
  // Start from on, so a reset to the default off state would show
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.publish_state(true);
  this->bus_.fail_writes = true;
  this->switch_.turn_off();
  EXPECT_TRUE(this->level_());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, InvertedSwitchDrivesTheOppositeLevel) {
  this->switch_.set_inverted(true);
  this->switch_.turn_on();
  EXPECT_FALSE(this->level_());
  EXPECT_TRUE(this->switch_.state);
  this->switch_.turn_off();
  EXPECT_TRUE(this->level_());
  EXPECT_FALSE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWritesTheRestoredLevel) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  this->hub_.setup();
  EXPECT_TRUE(this->level_());
  EXPECT_NE(this->bus_.registers[BQ25186_REG_VBAT_CTRL] & BQ25186_VBAT_CTRL_PG_MODE, 0);
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWritesTheRestoredLevelInverted) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  this->switch_.set_inverted(true);
  this->hub_.setup();
  EXPECT_FALSE(this->level_());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWithoutRestoreTakesTheChipLevel) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->hub_.setup();
  EXPECT_TRUE(this->level_());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWithoutRestoreTakesTheChipLevelInverted) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.set_inverted(true);
  this->hub_.setup();
  EXPECT_TRUE(this->level_());
  EXPECT_FALSE(this->switch_.state);
}

}  // namespace esphome::bq25186::testing
