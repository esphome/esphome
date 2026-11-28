#include "sx127x_test_helpers.h"
#include <cmath>

namespace esphome::sx127x::testing {

// The test radio is at 433.92 MHz, on the low frequency port
TEST(SX127xRssi, LoRaReadsTheChannelWhileListening) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.regs[REG_RSSI_VALUE] = 50;
  EXPECT_FLOAT_EQ(hw.radio.get_rssi_instant(), 50.0f - 164.0f);
}

TEST(SX127xRssi, FskReadsTheChannelWhileListening) {
  RadioHardware hw;
  hw.configure(false, true);
  hw.regs[REG_RSSI_VALUE_FSK] = 221;
  EXPECT_FLOAT_EQ(hw.radio.get_rssi_instant(), -110.5f);
}

// During a packet the reading is the packet's power, not the noise floor
TEST(SX127xRssi, NotDuringAPacket) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.regs[REG_MODEM_STAT] = MODEM_STAT_HEADER_VALID;
  EXPECT_TRUE(std::isnan(hw.radio.get_rssi_instant()));
}

TEST(SX127xRssi, NotWhileAsleepOrSending) {
  RadioHardware asleep;
  asleep.configure(true, false);
  EXPECT_TRUE(std::isnan(asleep.radio.get_rssi_instant()));
  RadioHardware hw;
  hw.configure(true, true);
  TransmitListener listener;
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
  EXPECT_TRUE(std::isnan(hw.radio.get_rssi_instant()));
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&listener));
}

}  // namespace esphome::sx127x::testing
