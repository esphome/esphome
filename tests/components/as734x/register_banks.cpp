// Reaching a register through the wrong CFG0 bank is not reported as an error by either chip: the
// transfer is acknowledged and reads back the value just written, while the register that was meant
// to change keeps its own. Nothing at runtime can notice that, so these tests put a bus in front of
// the drivers that answers strictly as the datasheets describe and refuses out-of-bank addresses.

#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "esphome/components/as734x/as7341.h"
#include "esphome/components/as734x/as7343.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::as734x::testing {

namespace {

// DS000504 (AS7341) and DS001046 (AS7343), CFG0 bit 4 REG_BANK:
//   0 - register access to register 0x80 and above
//   1 - register access to the low window (0x60 to 0x74 / 0x20 to 0x7F)
class StrictBankBus : public i2c::I2CBus {
 public:
  StrictBankBus(uint8_t cfg0, uint8_t low_min, uint8_t low_max) : cfg0_(cfg0), low_min_(low_min), low_max_(low_max) {}

  std::vector<uint8_t> rejected;         // addresses reached through the wrong bank
  std::map<uint8_t, uint8_t> registers;  // values the chip actually holds

  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    const uint8_t reg = write_buffer[0];
    if (!this->reachable_(reg)) {
      this->rejected.push_back(reg);
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (read_count > 0) {
      for (size_t i = 0; i < read_count; i++) {
        const auto it = this->registers.find(static_cast<uint8_t>(reg + i));
        read_buffer[i] = it == this->registers.end() ? 0 : it->second;
      }
      return i2c::ERROR_OK;
    }
    for (size_t i = 1; i < write_count; i++) {
      this->registers[static_cast<uint8_t>(reg + i - 1)] = write_buffer[i];
    }
    return i2c::ERROR_OK;
  }

  bool low_bank_selected() const { return (this->value_of(this->cfg0_) & 0x10) != 0; }

  uint8_t value_of(uint8_t reg) const {
    const auto it = this->registers.find(reg);
    return it == this->registers.end() ? 0 : it->second;
  }

 protected:
  // CFG0 has to answer from either bank, otherwise the bank could never be switched back. The
  // AS7341 SMUX configuration RAM below 0x20 sits outside both windows and is always reachable.
  bool reachable_(uint8_t reg) const {
    if (reg == this->cfg0_ || reg < 0x20) {
      return true;
    }
    return this->low_bank_selected() ? (reg >= this->low_min_ && reg <= this->low_max_) : (reg >= 0x80);
  }

  uint8_t cfg0_;
  uint8_t low_min_;
  uint8_t low_max_;
};

constexpr uint8_t ATIME = 0x81;

}  // namespace

// AS7341: CONFIG 0x70 is only reachable from the low bank, ATIME/ASTEP/CFG1 only from the high one,
// and setup() writes them one after the other.
TEST(RegisterBanks, As7341ConfiguresEveryRegisterFromTheRightBank) {
  StrictBankBus bus(0xA9, 0x60, 0x74);
  i2c::I2CDevice dev;
  dev.set_i2c_bus(&bus);
  dev.set_i2c_address(0x39);
  AS7341 chip(&dev);

  chip.verify_device_id();
  EXPECT_TRUE(chip.write_default_config());
  EXPECT_TRUE(chip.write_atime(120));
  EXPECT_TRUE(chip.write_astep(99));
  EXPECT_TRUE(chip.write_gain(GAIN_8X));
  EXPECT_TRUE(chip.prepare_for_smux_step(0));

  EXPECT_EQ(bus.rejected.size(), 0u) << "a register was reached through the wrong bank";
  EXPECT_EQ(bus.value_of(ATIME), 120);
  EXPECT_EQ(bus.value_of(0xAA), GAIN_8X) << "CFG1 holds the gain";
  EXPECT_FALSE(bus.low_bank_selected()) << "the low bank is left selected after a low register";
}

// AS7343: ID 0x5A and CFG10 0x65 are only reachable from the low bank, everything else in the
// start-up chain only from the high one.
TEST(RegisterBanks, As7343ConfiguresEveryRegisterFromTheRightBank) {
  StrictBankBus bus(0xBF, 0x20, 0x7F);
  i2c::I2CDevice dev;
  dev.set_i2c_bus(&bus);
  dev.set_i2c_address(0x39);
  AS7343 chip(&dev);

  chip.verify_device_id();
  EXPECT_TRUE(chip.write_default_config());
  EXPECT_TRUE(chip.write_atime(120));
  EXPECT_TRUE(chip.write_astep(99));
  EXPECT_TRUE(chip.write_gain(GAIN_8X));

  EXPECT_EQ(bus.rejected.size(), 0u) << "a register was reached through the wrong bank";
  EXPECT_EQ(bus.value_of(0x65), 0xF2) << "CFG10 sits in the low bank";
  EXPECT_EQ(bus.value_of(0xD6), 0x62) << "CFG20 selects all 18 channels over three SMUX cycles";
  EXPECT_EQ(bus.value_of(ATIME), 120);
  EXPECT_FALSE(bus.low_bank_selected()) << "the low bank is left selected after a low register";
}

// Powering the chip up and starting a measurement only touches ENABLE, which is outside the low
// window, so none of it should cost a bank switch.
TEST(RegisterBanks, EnableBitsNeedNoBankSwitch) {
  StrictBankBus bus(0xBF, 0x20, 0x7F);
  i2c::I2CDevice dev;
  dev.set_i2c_bus(&bus);
  dev.set_i2c_address(0x39);
  AS7343 chip(&dev);

  EXPECT_TRUE(chip.enable_power(true));
  EXPECT_TRUE(chip.enable_spectral_measurement(true));
  EXPECT_EQ(bus.value_of(0x80), 0x03) << "PON and SP_EN";

  EXPECT_EQ(bus.rejected.size(), 0u);
  EXPECT_FALSE(bus.low_bank_selected());
  EXPECT_EQ(bus.registers.count(0xBF), 0u) << "CFG0 was written for a register that did not need it";
}

}  // namespace esphome::as734x::testing
