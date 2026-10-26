#include <gtest/gtest.h>

#include <array>

#include "esphome/components/pcm5122/pcm5122.h"
#include "esphome/components/switch/switch.h"

namespace esphome::pcm5122::testing {

static constexpr uint8_t ADDRESS = 0x4D;

// The bodies codegen emits for power_mode: standby and power_mode: powerdown.
static PCM5122 *dac = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void standby(switch_::Switch *entity, bool state) { dac->write_power_switch(entity, state, true); }
static void powerdown(switch_::Switch *entity, bool state) { dac->write_power_switch(entity, state, false); }

// Page 0 registers written as register/value pairs; failing writes are not acknowledged.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t * /*out*/,
                             size_t /*out_size*/) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 2)
      this->registers[data[0]] = data[1];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 256> registers{};
  bool fail{false};
};

class PCM5122PowerSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->dac_.set_i2c_bus(&this->bus_);
    this->dac_.set_i2c_address(ADDRESS);
    dac = &this->dac_;
    this->standby_.add_on_state_callback([this](bool) { ++this->publishes_; });
    this->powerdown_.add_on_state_callback([this](bool) { ++this->publishes_; });
  }

  uint8_t power_control_() const { return this->bus_.registers[PCM5122_REG_POWER_CONTROL]; }

  RegisterBus bus_;
  PCM5122 dac_;
  switch_::ApplySwitch<standby> standby_;
  switch_::ApplySwitch<powerdown> powerdown_;
  unsigned publishes_{0};
};

TEST_F(PCM5122PowerSwitchTest, StandbySwitchRequestsStandbyAndPublishes) {
  this->standby_.turn_on();
  EXPECT_EQ(this->power_control_(), PCM5122_POWER_CONTROL_RQST);
  EXPECT_TRUE(this->standby_.state);
  EXPECT_EQ(this->publishes_, 1u);

  this->standby_.turn_off();
  EXPECT_EQ(this->power_control_(), 0);
  EXPECT_FALSE(this->standby_.state);
  EXPECT_EQ(this->publishes_, 2u);
}

TEST_F(PCM5122PowerSwitchTest, PowerdownSwitchRequestsPowerdownAndPublishes) {
  this->powerdown_.turn_on();
  EXPECT_EQ(this->power_control_(), PCM5122_POWER_CONTROL_RQPD);
  EXPECT_TRUE(this->powerdown_.state);
  EXPECT_EQ(this->publishes_, 1u);

  this->powerdown_.turn_off();
  EXPECT_EQ(this->power_control_(), 0);
  EXPECT_FALSE(this->powerdown_.state);
  EXPECT_EQ(this->publishes_, 2u);
}

TEST_F(PCM5122PowerSwitchTest, FailedWriteLeavesTheSwitchUnpublished) {
  this->bus_.fail = true;
  this->standby_.turn_on();
  this->powerdown_.turn_on();
  EXPECT_EQ(this->publishes_, 0u);
  EXPECT_FALSE(this->standby_.has_state());
  EXPECT_FALSE(this->powerdown_.has_state());
}

TEST_F(PCM5122PowerSwitchTest, FailedWriteDoesNotLeakIntoTheNextRequest) {
  this->bus_.fail = true;
  this->standby_.turn_on();
  this->bus_.fail = false;
  this->powerdown_.turn_on();
  EXPECT_EQ(this->power_control_(), PCM5122_POWER_CONTROL_RQPD);
  EXPECT_FALSE(this->standby_.has_state());
  EXPECT_TRUE(this->powerdown_.state);
}

}  // namespace esphome::pcm5122::testing
