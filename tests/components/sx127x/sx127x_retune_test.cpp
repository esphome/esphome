#include "sx127x_test_helpers.h"
#include <tuple>

namespace esphome::sx127x::testing {

// 434.5 MHz as the chip's frequency registers: 434500000 * 2^19 / 32 MHz, independent of the code under test
static constexpr uint32_t FRF = 0x6CA000;
static constexpr uint32_t FREQUENCY = 434500000;
static constexpr uint32_t OTHER_FREQUENCY = 434700000;
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

class SX127xRetuneModulation : public ::testing::TestWithParam<bool> {};

TEST_P(SX127xRetuneModulation, KeepsAReceivedPacketForTheLoop) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(GetParam(), true);
  hw.radio.register_listener(&listener);
  hw.receive({0x44, 0x55, 0x66});
  hw.radio.retune(FREQUENCY);
  EXPECT_EQ(frf(hw), FRF);
  EXPECT_TRUE(listener.packets.empty());
  hw.radio.loop();
  hw.radio.loop();
  EXPECT_EQ(listener.packets, (std::vector<std::vector<uint8_t>>{{0x44, 0x55, 0x66}}));
}

TEST_P(SX127xRetuneModulation, FailureWarnsAndTheRetryUsesTheNewFrequency) {
  RadioHardware hw;
  hw.configure(GetParam(), true);
  hw.stuck = true;
  hw.radio.retune(FREQUENCY);
  EXPECT_TRUE(hw.radio.status_has_warning());
  hw.stuck = false;
  hw.radio.configure();
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_EQ(frf(hw), FRF);
}

TEST_P(SX127xRetuneModulation, ASecondRetuneBeforeTheLoopWaitsForTheHeldPacket) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(GetParam(), true);
  hw.radio.register_listener(&listener);
  hw.receive({0x01});
  hw.radio.retune(FREQUENCY);
  hw.receive({0x02});
  hw.radio.retune(OTHER_FREQUENCY);
  EXPECT_EQ(frf(hw), FRF);
  hw.radio.loop();
  hw.radio.loop();
  EXPECT_EQ(listener.packets, (std::vector<std::vector<uint8_t>>{{0x01}, {0x02}}));
}

TEST_P(SX127xRetuneModulation, ListenersKeepTheirPacketWhenOneRetunes) {
  RadioHardware hw;
  PacketListener first, second;
  hw.configure(GetParam(), true);
  hw.radio.register_listener(&first);
  hw.radio.register_listener(&second);
  first.on_receive = [&]() {
    if (first.packets.size() == 1) {
      hw.receive({0x02});
      hw.radio.retune(FREQUENCY);
    }
  };
  hw.receive({0x01});
  hw.radio.loop();
  hw.radio.loop();
  hw.radio.loop();
  const std::vector<std::vector<uint8_t>> both{{0x01}, {0x02}};
  EXPECT_EQ(first.packets, both);
  EXPECT_EQ(second.packets, both);
  EXPECT_EQ(frf(hw), FRF);
}

TEST_P(SX127xRetuneModulation, ConfigureDiscardsAHeldPacket) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(GetParam(), true);
  hw.radio.register_listener(&listener);
  hw.receive({0x01});
  hw.radio.retune(FREQUENCY);
  hw.radio.configure();
  hw.radio.loop();
  EXPECT_TRUE(listener.packets.empty());
}

INSTANTIATE_TEST_SUITE_P(LoraOrFsk, SX127xRetuneModulation, ::testing::Bool());

TEST(SX127xRetune, IgnoresAFrequencyOutOfRange) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.retune(1024000000);
  hw.radio.retune(136000000);
  EXPECT_EQ(frf(hw), FRF_DEFAULT);
}

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
