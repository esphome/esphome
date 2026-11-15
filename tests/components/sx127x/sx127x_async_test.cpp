#include "sx127x_test_helpers.h"
#include <cmath>
#include <tuple>

namespace esphome::sx127x::testing {

static std::vector<uint8_t> expected_fifo(const RadioHardware &hw, bool lora, bool fixed) {
  std::vector<uint8_t> fifo;
  if (!lora && !fixed) {
    fifo.push_back(hw.packet.size());  // A variable length FSK packet starts with its length
  }
  fifo.insert(fifo.end(), hw.packet.begin(), hw.packet.end());
  return fifo;
}

class SX127xAsyncModes : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {};

TEST_P(SX127xAsyncModes, ReturnsBeforeCompletionAndRestoresMode) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(lora, rx, fixed);
  ASSERT_FALSE(hw.radio.is_failed());
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1234), SX127xError::SX127X_ERROR_NONE);
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.mode(), MODE_TX);
  ASSERT_EQ(hw.sent.size(), 1u);
  EXPECT_EQ(hw.sent[0], expected_fifo(hw, lora, fixed));
  hw.radio.loop();
  EXPECT_TRUE(listener.results.empty());
  hw.complete_send();
  hw.radio.loop();
  EXPECT_EQ(listener.results, std::vector<SX127xError>{SX127xError::SX127X_ERROR_NONE});
  EXPECT_FALSE(hw.radio.is_transmitting());
  EXPECT_EQ(hw.mode(), rx ? MODE_RX : MODE_SLEEP);
  if (lora) {
    EXPECT_EQ(hw.irq, 0);  // TX_DONE is cleared, so DIO0 is low again
  }
}

TEST_P(SX127xAsyncModes, BlockingSendWaitsForCompletion) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware hw;
  hw.configure(lora, rx, fixed);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
  EXPECT_FALSE(hw.radio.is_transmitting());
  ASSERT_EQ(hw.sent.size(), 1u);
  EXPECT_EQ(hw.sent[0], expected_fifo(hw, lora, fixed));
  EXPECT_EQ(hw.mode(), rx ? MODE_RX : MODE_SLEEP);
}

INSTANTIATE_TEST_SUITE_P(Modes, SX127xAsyncModes,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()));

// Modulation and fixed length; only a receiving radio has a packet waiting
class SX127xHeldPacket : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(SX127xHeldPacket, PacketWaitingWhenASendStartsIsDeliveredFirst) {
  const auto [lora, fixed] = GetParam();
  const bool rx = true;
  RadioHardware hw;
  PacketListener packets;
  TransmitListener listener;
  hw.radio.register_listener(&packets);
  hw.configure(lora, rx, fixed);
  hw.receive({0x44, 0x55, 0x66});
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_BUSY);
  EXPECT_TRUE(hw.sent.empty());
  hw.radio.set_mode_sleep();  // Ignored while the packet is held
  EXPECT_EQ(hw.mode(), MODE_STDBY);
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], (std::vector<uint8_t>{0x44, 0x55, 0x66}));
  EXPECT_TRUE(hw.receiving());
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
  hw.radio.loop();
  EXPECT_EQ(listener.results, std::vector<SX127xError>{SX127xError::SX127X_ERROR_NONE});
  EXPECT_EQ(hw.sent.size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(Modes, SX127xHeldPacket, ::testing::Combine(::testing::Bool(), ::testing::Bool()));

TEST(SX127xAsync, OneSendAtATime) {
  RadioHardware hw;
  TransmitListener first, second;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&first, hw.packet), SX127xError::SX127X_ERROR_NONE);
  EXPECT_EQ(hw.radio.start_transmit_async(&second, hw.packet), SX127xError::SX127X_ERROR_BUSY);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_BUSY);
  hw.radio.set_mode_rx();
  hw.radio.run_image_cal();
  hw.radio.configure();
  EXPECT_EQ(hw.mode(), MODE_TX);
  EXPECT_EQ(hw.sent.size(), 1u);
  hw.complete_send();
  hw.radio.loop();
  EXPECT_EQ(first.results, std::vector<SX127xError>{SX127xError::SX127X_ERROR_NONE});
  EXPECT_TRUE(second.results.empty());
}

TEST(SX127xAsync, SendWithoutCompletionTimesOut) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  const uint32_t start = millis();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1000), SX127xError::SX127X_ERROR_NONE);
  {
    LoopBlockingGuard clock{nullptr, nullptr, start + 500};
    hw.radio.loop();
  }
  EXPECT_TRUE(listener.results.empty());
  {
    LoopBlockingGuard clock{nullptr, nullptr, start + 1100};
    hw.radio.loop();
  }
  EXPECT_EQ(listener.results, std::vector<SX127xError>{SX127xError::SX127X_ERROR_TIMEOUT});
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving());
}

// The chip leaves a send that is still going out for standby or sleep, but not for RX
class SX127xCancel : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(SX127xCancel, CancelEndsOnlyTheListenersOwnSend) {
  const auto [lora, rx] = GetParam();
  RadioHardware hw;
  TransmitListener owner, other;
  hw.configure(lora, rx);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&owner, hw.packet), SX127xError::SX127X_ERROR_NONE);
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&other));
  EXPECT_FALSE(hw.radio.cancel_transmit_async(nullptr));
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&owner));
  EXPECT_EQ(owner.results, std::vector<SX127xError>{SX127xError::SX127X_ERROR_CANCELLED});
  EXPECT_FALSE(hw.radio.is_transmitting());
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_EQ(hw.mode(), rx ? MODE_RX : MODE_SLEEP);
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&owner));
  hw.radio.loop();
  EXPECT_EQ(owner.results.size(), 1u);
  EXPECT_TRUE(other.results.empty());
}

INSTANTIATE_TEST_SUITE_P(Modes, SX127xCancel, ::testing::Combine(::testing::Bool(), ::testing::Bool()));

TEST(SX127xAsync, CompletionMayStartTheNextSend) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  listener.on_complete = [&](SX127xError result) {
    if (listener.results.size() == 1) {
      EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
    }
  };
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
  hw.radio.loop();
  EXPECT_TRUE(hw.radio.is_transmitting());
  hw.radio.loop();
  EXPECT_EQ(listener.results.size(), 2u);
  EXPECT_EQ(hw.sent.size(), 2u);
  EXPECT_TRUE(hw.receiving());
}

TEST(SX127xAsync, PacketListenerMaySend) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  hw.configure(true, true);
  packets.on_receive = [&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE); };
  hw.receive({0x01, 0x02});
  hw.radio.loop();
  EXPECT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(hw.sent.size(), 1u);
  EXPECT_TRUE(hw.receiving());
}

TEST(SX127xAsync, InvalidRequestsAreRefused) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true, true);
  EXPECT_EQ(hw.radio.start_transmit_async(nullptr, hw.packet), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 0), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 262144), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, nullptr, 3), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 0), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 2), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_TRUE(hw.sent.empty());
  EXPECT_TRUE(listener.results.empty());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
}

TEST(SX127xAsync, SendWithoutDio0IsRefused) {
  RadioHardware hw;
  hw.radio.set_dio0_pin(nullptr);
  hw.configure(true, true);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_INVALID_PARAMS);
  EXPECT_TRUE(hw.sent.empty());
}

TEST(SX127xReceive, LoRaPacketHasRssiAndSnr) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  hw.configure(true, true);
  hw.receive({0x01, 0x02, 0x03, 0x04});
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}));
  EXPECT_FLOAT_EQ(packets.rssi, 100.0f - 164.0f);
  EXPECT_FLOAT_EQ(packets.snr, 5.0f);
  EXPECT_EQ(hw.irq, 0);
}

TEST(SX127xReceive, FskPacketHasNoSnr) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  hw.configure(false, true);
  hw.receive({0x01, 0x02, 0x03, 0x04});
  hw.radio.loop();
  ASSERT_EQ(packets.packets.size(), 1u);
  EXPECT_EQ(packets.packets[0], (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}));
  EXPECT_TRUE(std::isnan(packets.snr));
}

TEST(SX127xReceive, PacketThatFailedItsCrcIsDroppedAndDoesNotHoldUpASend) {
  RadioHardware hw;
  PacketListener packets;
  hw.radio.register_listener(&packets);
  hw.configure(true, true);
  hw.receive({0x01}, true);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
  hw.radio.loop();
  EXPECT_TRUE(packets.packets.empty());
}

// The TX mode action sends whatever the FIFO holds without tracking it; its completion
// is not taken for a received packet
TEST(SX127xReceive, RawTxModeIsNotAPacket) {
  for (const bool lora : {true, false}) {
    RadioHardware hw;
    PacketListener packets;
    hw.radio.register_listener(&packets);
    hw.configure(lora, true);
    hw.radio.set_mode_tx();
    EXPECT_EQ(hw.mode(), MODE_TX);
    EXPECT_TRUE(hw.dio0.digital_read());
    hw.radio.loop();
    EXPECT_TRUE(packets.packets.empty()) << (lora ? "LoRa" : "FSK");
    hw.radio.set_mode_rx();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
  }
}

TEST(SX127xRecovery, FailedModeChangeIsReconfiguredWithoutBeingAsked) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.stuck = true;
  hw.radio.set_mode_standby();
  EXPECT_FALSE(hw.radio.is_failed());
  EXPECT_TRUE(hw.radio.status_has_warning());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NOT_READY);
  hw.radio.set_mode_sleep();  // Ignored until configured again
  hw.stuck = false;
  EXPECT_EQ(hw.mode(), MODE_RX);
  run_scheduler(1100);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NONE);
  // Once configured, the retry stops
  hw.radio.set_mode_sleep();
  run_scheduler(1100);
  EXPECT_EQ(hw.mode(), MODE_SLEEP);
}

TEST(SX127xRecovery, FailedSendStartIsReconfiguredWithoutBeingAsked) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.stuck = true;
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NOT_READY);
  EXPECT_FALSE(hw.radio.is_transmitting());
  EXPECT_TRUE(listener.results.empty());
  hw.stuck = false;
  run_scheduler(1100);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX127xError::SX127X_ERROR_NONE);
}

TEST(SX127xRecovery, UnsupportedChipFailsSetup) {
  RadioHardware hw;
  hw.version = 0x22;
  hw.configure(true, true);
  EXPECT_TRUE(hw.radio.is_failed());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NOT_READY);
}

TEST(SX127xRecovery, NothingHappensBeforeSetup) {
  RadioHardware hw;
  unsigned resets = 0;
  hw.rst.on_write = [&](bool high) { resets += high ? 0 : 1; };
  hw.radio.configure();
  hw.radio.set_mode_rx();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX127xError::SX127X_ERROR_NOT_READY);
  EXPECT_EQ(resets, 0u);
  EXPECT_EQ(hw.mode(), MODE_STDBY);
}

}  // namespace esphome::sx127x::testing
