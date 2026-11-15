#include "sx126x_test_helpers.h"

namespace esphome::sx126x::testing {

// The radio link refuses to send while is_receiving(), as a send would lose the packet
TEST(SX126xReceiving, ReportsAPacketOnTheWayOrWaiting) {
  RadioHardware hw;
  hw.configure(true, true);
  EXPECT_FALSE(hw.radio.is_receiving());
  hw.irq = IRQ_HEADER_VALID;
  EXPECT_TRUE(hw.radio.is_receiving());
  hw.irq = IRQ_SYNCWORD_VALID;
  EXPECT_TRUE(hw.radio.is_receiving());
  hw.irq = IRQ_RX_DONE;
  EXPECT_TRUE(hw.radio.is_receiving());
  // Held by a send that found it waiting
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY);
  EXPECT_TRUE(hw.radio.is_receiving());
  hw.radio.loop();
  EXPECT_FALSE(hw.radio.is_receiving());
}

TEST(SX126xReceiving, NotWhileSendingOrAsleep) {
  RadioHardware hw;
  hw.configure(true, false);
  hw.irq = IRQ_HEADER_VALID;
  hw.clear_trace();
  EXPECT_FALSE(hw.radio.is_receiving());
  EXPECT_TRUE(hw.transactions.empty());  // A sleeping radio is not woken to ask
}

// A valid header or sync word is flagged, but only the end of a packet is signalled on DIO1
TEST(SX126xReceiving, ReceiveFlagsHeaderAndSyncWithoutSignallingThem) {
  RadioHardware hw;
  hw.configure(true, true);
  const auto params = std::find_if(hw.transactions.rbegin(), hw.transactions.rend(), [](const auto &command) {
    return !command.empty() && command[0] == RADIO_SET_DIOIRQPARAMS;
  });
  ASSERT_NE(params, hw.transactions.rend());
  ASSERT_GE(params->size(), 5u);
  const uint16_t flags = encode_uint16((*params)[1], (*params)[2]);
  const uint16_t dio1 = encode_uint16((*params)[3], (*params)[4]);
  EXPECT_EQ(flags & (IRQ_HEADER_VALID | IRQ_SYNCWORD_VALID), IRQ_HEADER_VALID | IRQ_SYNCWORD_VALID);
  EXPECT_EQ(dio1 & (IRQ_HEADER_VALID | IRQ_SYNCWORD_VALID), 0);
  EXPECT_NE(dio1 & IRQ_RX_DONE, 0);
}

}  // namespace esphome::sx126x::testing
