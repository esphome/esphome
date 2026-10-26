#include <gtest/gtest.h>

#include <array>

#include "esphome/components/bq25186/bq25186.h"
#include "esphome/components/bq25186/bq25186_switch.h"

namespace esphome::bq25186::testing {

static constexpr uint8_t ADDRESS = 0x6A;
static constexpr uint8_t SYS_REG_OTHER_BITS = 0x0F;

// The chip's registers behind an address pointer that moves on after every byte; writes can be made to fail.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size > 1 && this->fail_writes)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++)
        this->registers[this->next_()] = data[i];
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->next_()];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, BQ25186_STATUS_REGISTER_COUNT> registers{};
  bool fail_writes{false};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

class BQ25186PgGpoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.registers[BQ25186_REG_MASK_ID] = 0x01;  // the BQ25186 device id
    this->bus_.registers[BQ25186_REG_SYS_REG] = SYS_REG_OTHER_BITS;
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(ADDRESS);
    this->switch_.set_parent(&this->hub_);
    this->switch_.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    this->hub_.set_pg_gpo_switch(&this->switch_);
  }

  bool level() const { return (this->bus_.registers[BQ25186_REG_SYS_REG] & BQ25186_SYS_REG_PG_GPO_LEVEL) != 0; }
  uint8_t other_bits() const { return this->bus_.registers[BQ25186_REG_SYS_REG] & ~BQ25186_SYS_REG_PG_GPO_LEVEL; }

  RegisterBus bus_;
  BQ25186Component hub_;
  BQ25186PgGpoSwitch switch_;
};

TEST_F(BQ25186PgGpoTest, TurnOnSetsTheLevelAndPublishes) {
  this->switch_.turn_on();
  EXPECT_TRUE(this->level());
  EXPECT_EQ(this->other_bits(), SYS_REG_OTHER_BITS);
  EXPECT_TRUE(this->switch_.has_state());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, TurnOffClearsTheLevelAndPublishes) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.publish_state(true);
  this->switch_.turn_off();
  EXPECT_FALSE(this->level());
  EXPECT_EQ(this->other_bits(), SYS_REG_OTHER_BITS);
  EXPECT_FALSE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, FailedWriteLeavesTheStateUnpublished) {
  this->bus_.fail_writes = true;
  this->switch_.turn_on();
  EXPECT_FALSE(this->level());
  EXPECT_FALSE(this->switch_.has_state());
}

TEST_F(BQ25186PgGpoTest, FailedWriteKeepsThePreviousState) {
  this->switch_.publish_state(false);
  this->bus_.fail_writes = true;
  this->switch_.turn_on();
  EXPECT_FALSE(this->level());
  EXPECT_FALSE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, InvertedSwitchDrivesTheOppositeLevel) {
  this->switch_.set_inverted(true);
  this->switch_.turn_on();
  EXPECT_FALSE(this->level());
  EXPECT_TRUE(this->switch_.state);
  this->switch_.turn_off();
  EXPECT_TRUE(this->level());
  EXPECT_FALSE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWritesTheRestoredLevel) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  this->hub_.setup();
  EXPECT_TRUE(this->level());
  EXPECT_NE(this->bus_.registers[BQ25186_REG_VBAT_CTRL] & BQ25186_VBAT_CTRL_PG_MODE, 0);
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWritesTheRestoredLevelInverted) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  this->switch_.set_inverted(true);
  this->hub_.setup();
  EXPECT_FALSE(this->level());
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(BQ25186PgGpoTest, SetupWithoutRestoreTakesTheChipLevel) {
  this->bus_.registers[BQ25186_REG_SYS_REG] |= BQ25186_SYS_REG_PG_GPO_LEVEL;
  this->hub_.setup();
  EXPECT_TRUE(this->level());
  EXPECT_TRUE(this->switch_.state);
}

}  // namespace esphome::bq25186::testing
