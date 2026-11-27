#include "sx126x_test_helpers.h"

namespace esphome::sx126x::testing {

// 916.4 MHz as the chip's frequency word: 916400000 * 2^25 / 32 MHz, independent of the code under test
static constexpr uint8_t FREQUENCY_WORD[4] = {0x39, 0x46, 0x66, 0x66};
static constexpr uint32_t FREQUENCY = 916400000;

static std::vector<uint8_t> set_frequency_command() {
  return {RADIO_SET_RFFREQUENCY, FREQUENCY_WORD[0], FREQUENCY_WORD[1], FREQUENCY_WORD[2], FREQUENCY_WORD[3]};
}

class TransmitListener : public SX126xTransmitListener {
 public:
  void on_transmit_complete(SX126xError result) override { this->results.push_back(result); }
  std::vector<SX126xError> results;
};

class SX126xRetune : public ::testing::TestWithParam<bool> {};

TEST_P(SX126xRetune, MovesFrequencyWithoutConfiguringAgain) {
  const bool rx = GetParam();
  RadioHardware hw;
  hw.configure(true, rx);
  ASSERT_FALSE(hw.radio.is_failed());
  hw.clear_trace();
  hw.radio.retune(FREQUENCY);
  ASSERT_GE(hw.transactions.size(), 3u);
  EXPECT_EQ(hw.transactions[0], (std::vector<uint8_t>{RADIO_SET_STANDBY, STDBY_XOSC}));
  EXPECT_EQ(hw.transactions[1], set_frequency_command());
  EXPECT_EQ(hw.transactions.back()[0], rx ? RADIO_SET_RX : RADIO_SET_SLEEP);
  // Nothing a configuration sends besides
  for (const auto &command : hw.transactions) {
    EXPECT_NE(command[0], RADIO_SET_PACKETTYPE);
    EXPECT_NE(command[0], RADIO_CALIBRATEIMAGE);
    EXPECT_NE(command[0], RADIO_SET_MODULATIONPARAMS);
  }
  EXPECT_EQ(hw.receiving, rx);
  EXPECT_FALSE(hw.radio.status_has_warning());
}

TEST_P(SX126xRetune, LaterConfigurationKeepsTheFrequency) {
  RadioHardware hw;
  hw.configure(true, GetParam());
  hw.radio.retune(FREQUENCY);
  hw.clear_trace();
  hw.radio.configure();
  EXPECT_NE(std::find(hw.transactions.begin(), hw.transactions.end(), set_frequency_command()), hw.transactions.end());
}

INSTANTIATE_TEST_SUITE_P(ReceiveOrSleep, SX126xRetune, ::testing::Bool());

TEST(SX126xRetune, IgnoredWhileSending) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1234), SX126xError::SX126X_ERROR_NONE);
  hw.clear_trace();
  hw.radio.retune(FREQUENCY);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(hw.radio.is_transmitting());
}

TEST(SX126xRetune, IgnoredBeforeSetup) {
  RadioHardware hw;
  hw.radio.retune(FREQUENCY);
  EXPECT_TRUE(hw.transactions.empty());
}

TEST(SX126xRetune, AfterColdSleepAppliedWhenTheRadioWakes) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_mode_sleep(true);
  hw.clear_trace();
  hw.radio.retune(FREQUENCY);
  EXPECT_TRUE(hw.transactions.empty());
  hw.radio.set_mode_rx();
  EXPECT_NE(std::find(hw.transactions.begin(), hw.transactions.end(), set_frequency_command()), hw.transactions.end());
  EXPECT_TRUE(hw.receiving);
}

}  // namespace esphome::sx126x::testing
