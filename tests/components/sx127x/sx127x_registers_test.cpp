#include "sx127x_test_helpers.h"

namespace esphome::sx127x::testing {

// Values from the datasheet formulas for the RadioHardware defaults (433.92 MHz, 4800 bps,
// 5 kHz deviation, 8 symbol preamble), independent of the code under test
static constexpr uint32_t FRF = 0x6C7AE1;    // 433920000 * 2^19 / 32 MHz
static constexpr uint16_t BITRATE = 0x1A0B;  // 32 MHz / 4800, rounded
static constexpr uint16_t FDEV = 0x0051;     // 5000 * 2^19 / 32 MHz
static constexpr uint16_t PREAMBLE = 8;

static uint32_t reg16(const RadioHardware &hw, uint8_t msb) { return (hw.regs[msb] << 8) | hw.regs[msb + 1]; }

TEST(SX127xRegisters, FskWritesEachByteOfTheMultiByteSettings) {
  RadioHardware hw;
  hw.configure(false, false);
  EXPECT_EQ((hw.regs[REG_FRF_MSB] << 16) | reg16(hw, REG_FRF_MID), FRF);
  EXPECT_EQ(reg16(hw, REG_BITRATE_MSB), BITRATE);
  EXPECT_EQ(reg16(hw, REG_FDEV_MSB), FDEV);
  EXPECT_EQ(reg16(hw, REG_PREAMBLE_SIZE_MSB), PREAMBLE);
}

TEST(SX127xRegisters, LoraWritesEachByteOfTheMultiByteSettings) {
  RadioHardware hw;
  hw.configure(true, false);
  EXPECT_EQ((hw.regs[REG_FRF_MSB] << 16) | reg16(hw, REG_FRF_MID), FRF);
  EXPECT_EQ(reg16(hw, REG_PREAMBLE_LEN_MSB), PREAMBLE);
}

}  // namespace esphome::sx127x::testing
