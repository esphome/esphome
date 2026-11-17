#include "sx127x_test_helpers.h"

namespace esphome::sx127x::testing {

// The radio link refuses to send while is_receiving(), as a send would lose the packet
TEST(SX127xReceiving, LoRaReportsAPacketOnTheWayOrWaiting) {
  RadioHardware hw;
  hw.configure(true, true);
  EXPECT_FALSE(hw.radio.is_receiving());
  for (const uint8_t stat : {MODEM_STAT_SIGNAL_DETECTED, MODEM_STAT_SIGNAL_SYNCHRONIZED, MODEM_STAT_HEADER_VALID}) {
    hw.regs[REG_MODEM_STAT] = stat;
    EXPECT_TRUE(hw.radio.is_receiving()) << int(stat);
  }
  hw.regs[REG_MODEM_STAT] = 0;
  hw.receive({0x01, 0x02});
  EXPECT_TRUE(hw.radio.is_receiving());
  // Held by a send that found it waiting
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_BUSY);
  EXPECT_TRUE(hw.radio.is_receiving());
  hw.radio.loop();
  EXPECT_FALSE(hw.radio.is_receiving());
}

TEST(SX127xReceiving, FskReportsAMatchedSyncWord) {
  RadioHardware hw;
  hw.configure(false, true);
  EXPECT_FALSE(hw.radio.is_receiving());
  hw.regs[REG_IRQ_FLAGS_1] = IRQ_FLAGS_1_PREAMBLE_DETECT;
  EXPECT_FALSE(hw.radio.is_receiving());
  hw.regs[REG_IRQ_FLAGS_1] = IRQ_FLAGS_1_SYNC_ADDRESS_MATCH;
  EXPECT_TRUE(hw.radio.is_receiving());
  hw.regs[REG_IRQ_FLAGS_1] = 0;
  hw.receive({0x01, 0x02});
  EXPECT_TRUE(hw.radio.is_receiving());
}

TEST(SX127xReceiving, NotAfterRawTxOrWhileSendingOrAsleep) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_mode_tx();  // Its TX_DONE raises DIO0, which is not a packet
  EXPECT_TRUE(hw.dio0.digital_read());
  EXPECT_FALSE(hw.radio.is_receiving());
  hw.radio.set_mode_rx();
  TransmitListener listener;
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
  hw.regs[REG_MODEM_STAT] = MODEM_STAT_HEADER_VALID;
  EXPECT_FALSE(hw.radio.is_receiving());
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&listener));

  RadioHardware asleep;
  asleep.configure(true, false);
  asleep.regs[REG_MODEM_STAT] = MODEM_STAT_HEADER_VALID;
  EXPECT_FALSE(asleep.radio.is_receiving());
}

TEST(SX127xReceiving, FskFifoHoldsTheLengthByte) {
  RadioHardware hw;
  hw.configure(false, true);
  EXPECT_EQ(hw.radio.get_max_packet_size(), 63u);
  EXPECT_EQ(hw.regs[REG_PAYLOAD_LENGTH_LSB], 63);
}

}  // namespace esphome::sx127x::testing
