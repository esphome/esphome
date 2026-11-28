#include "sx126x_test_helpers.h"
#include <cmath>

namespace esphome::sx126x::testing {

TEST(SX126xRssi, ReadsTheChannelWhileListening) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.rssi_inst = 221;
  EXPECT_FLOAT_EQ(hw.radio.get_rssi_instant(), -110.5f);
}

// During a packet the reading is the packet's power, not the noise floor
TEST(SX126xRssi, NotDuringAPacket) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.rssi_inst = 100;
  hw.irq = IRQ_HEADER_VALID;
  EXPECT_TRUE(std::isnan(hw.radio.get_rssi_instant()));
}

TEST(SX126xRssi, NotWhileAsleep) {
  RadioHardware hw;
  hw.configure(true, false);
  hw.clear_trace();
  EXPECT_TRUE(std::isnan(hw.radio.get_rssi_instant()));
  EXPECT_TRUE(hw.transactions.empty());  // A sleeping radio is not woken to ask
}

TEST(SX126xRssi, FailedReadIsNotANumber) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.rssi_inst = 200;
  hw.clear_trace();
  hw.fail_at = 2;  // BUSY stays high for the RSSI read, after the IRQ read
  EXPECT_TRUE(std::isnan(hw.radio.get_rssi_instant()));
  hw.clear_trace();
  EXPECT_FLOAT_EQ(hw.radio.get_rssi_instant(), -100.0f);
}

}  // namespace esphome::sx126x::testing
