#include "sx127x_test_helpers.h"
#include <tuple>

namespace esphome::sx127x::testing {

// 434.5 MHz as the chip's frequency registers: 434500000 * 2^19 / 32 MHz, independent of the code under test
static constexpr uint32_t FRF = 0x6CA000;
static constexpr uint32_t FREQUENCY = 434500000;
// The RadioHardware default, 433.92 MHz
static constexpr uint32_t FRF_DEFAULT = 0x6C7AE1;

static uint32_t frf(const RadioHardware &hw) {
  return (hw.regs[REG_FRF_MSB] << 16) | (hw.regs[REG_FRF_MID] << 8) | hw.regs[REG_FRF_LSB];
}

class SX127xRetune : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(SX127xRetune, MovesFrequencyWithoutConfiguringAgain) {
  const auto [lora, rx] = GetParam();
  RadioHardware hw;
  hw.configure(lora, rx);
  ASSERT_FALSE(hw.radio.is_failed());
  // A register only configuration writes: a reset would clear it
  const uint8_t pa_config = hw.regs[REG_PA_CONFIG];
  ASSERT_NE(pa_config, 0);
  hw.radio.retune(FREQUENCY);
  EXPECT_EQ(frf(hw), FRF);
  EXPECT_EQ(hw.regs[REG_PA_CONFIG], pa_config);
  EXPECT_EQ(hw.lora(), lora);
  EXPECT_EQ(hw.mode(), rx ? MODE_RX : MODE_SLEEP);
  EXPECT_FALSE(hw.radio.status_has_warning());
}

TEST_P(SX127xRetune, LaterConfigurationKeepsTheFrequency) {
  const auto [lora, rx] = GetParam();
  RadioHardware hw;
  hw.configure(lora, rx);
  hw.radio.retune(FREQUENCY);
  hw.radio.configure();
  EXPECT_EQ(frf(hw), FRF);
}

INSTANTIATE_TEST_SUITE_P(ModulationAndMode, SX127xRetune, ::testing::Combine(::testing::Bool(), ::testing::Bool()));

TEST(SX127xRetune, IgnoredWhileSending) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1234), SX127xError::SX127X_ERROR_NONE);
  hw.radio.retune(FREQUENCY);
  EXPECT_EQ(frf(hw), FRF_DEFAULT);
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.mode(), MODE_TX);
}

TEST(SX127xRetune, IgnoredBeforeSetup) {
  RadioHardware hw;
  hw.radio.retune(FREQUENCY);
  EXPECT_EQ(frf(hw), 0u);
}

}  // namespace esphome::sx127x::testing
