#include "sx126x_test_helpers.h"
#include "esphome/core/application.h"
#include <tuple>
#include <type_traits>
#ifdef USE_HOST
#include <string>
#include "esphome/components/logger/logger.h"
#endif
namespace esphome::sx126x::testing {
class SX126xBusySetup : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(SX126xBusySetup, StopsAtEveryConfigurationCommand) {
  const auto [lora, rx] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx);
  ASSERT_FALSE(reference.radio.is_failed());
  ASSERT_FALSE(reference.radio.status_has_warning());
  const size_t count = reference.transactions.size();
  ASSERT_GT(count, 10u);
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    hardware.fail_at = fault;
    hardware.configure(lora, rx);
    hardware.expect_aborted(fault);
    EXPECT_TRUE(hardware.radio.is_failed());
    hardware.radio.dump_config();  // Also safe if the version read never happened.
    hardware.clear_trace();
    hardware.radio.configure();
    hardware.radio.set_mode_rx();
    hardware.radio.set_mode_tx();
    hardware.radio.set_mode_sleep();
    hardware.radio.set_mode_standby(STDBY_XOSC);
    hardware.radio.run_image_cal();
    hardware.radio.loop();
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::SX126X_ERROR_NOT_READY);
    EXPECT_TRUE(hardware.transactions.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSKWithRxOrSleep, SX126xBusySetup,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool()));

class SX126xBusyTransmit : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {};

TEST_P(SX126xBusyTransmit, StopsAtEverySendAndCleanupCommandAndCanRetry) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  const size_t count = reference.transactions.size();
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    hardware.configure(lora, rx, fixed);
    hardware.clear_trace();
    hardware.fail_at = fault;
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet.data(), hardware.packet.size()),
              SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hardware.transactions[fault - 1].empty());
    EXPECT_TRUE(hardware.cs.level);
    EXPECT_FALSE(hardware.radio.is_failed());
    hardware.clear_trace();
    hardware.radio.configure();
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::NONE);
    EXPECT_FALSE(hardware.radio.status_has_warning());
    EXPECT_EQ(hardware.receiving, rx);
  }
}

TEST_P(SX126xBusyTransmit, RestoresEveryPreparationFailureWithoutAnotherTransmission) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  size_t tx_command = 0;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      tx_command = i + 1;
      break;
    }
  }
  ASSERT_NE(tx_command, 0u);
  for (size_t fault = 1; fault <= tx_command; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(lora, rx, fixed);
    hw.radio.register_listener(&listener);
    const auto configured_params = hw.packet_params;
    hw.clear_trace();
    hw.fail_at = fault;
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hw.transactions[fault - 1].empty());
    EXPECT_TRUE(hw.cs.level);
    EXPECT_EQ(hw.ends, hw.transactions.size());
    EXPECT_EQ(hw.receiving, rx);
    EXPECT_EQ(hw.packet_params, configured_params);
    EXPECT_EQ(hw.irq, 0u);
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_FALSE(hw.radio.is_failed());
    const auto sends = hw.sends;
    hw.clear_trace();
    if (rx) {
      if (!fixed) {
        hw.packet.resize(9, 0x55);  // RX must accept a different length than the attempted TX.
      }
      hw.irq = IRQ_RX_DONE;
      hw.radio.loop();
      ASSERT_EQ(listener.packets.size(), 1u);
      EXPECT_EQ(listener.packets[0], hw.packet);
    }
    EXPECT_EQ(hw.sends, sends);
  }
}

TEST_P(SX126xBusyTransmit, FailedRestorationRequiresConfigureBeforeReceiveRecovery) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  size_t restoration = 0;
  bool sent = false;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      sent = true;
    }
    if (sent && reference.transactions[i][0] == RADIO_SET_STANDBY) {
      restoration = i + 1;
      break;
    }
  }
  ASSERT_NE(restoration, 0u);
  for (size_t fault = restoration; fault <= reference.transactions.size(); fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(lora, rx, fixed);
    hw.radio.register_listener(&listener);
    const auto configured_params = hw.packet_params;
    hw.clear_trace();
    hw.fail_at = fault;
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hw.cs.level);
    EXPECT_TRUE(hw.radio.status_has_warning());
    EXPECT_FALSE(hw.radio.is_failed());
    const auto sends = hw.sends;
    hw.clear_trace();  // Clearing BUSY alone must not re-admit partial configuration.
    hw.irq = IRQ_RX_DONE;
    hw.radio.loop();
    hw.radio.set_mode_rx();
    hw.radio.set_mode_tx();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.set_mode_sleep();
    hw.radio.run_image_cal();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(listener.packets.empty());
    EXPECT_TRUE(hw.radio.status_has_warning());
    hw.irq = 0;  // Hardware reset clears IRQs; the GPIO fake does not model reset.
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.packet_params, configured_params);
    EXPECT_EQ(hw.receiving, rx);
    if (rx) {
      hw.irq = IRQ_RX_DONE;
      hw.radio.loop();
      ASSERT_EQ(listener.packets.size(), 1u);
      EXPECT_EQ(listener.packets[0], hw.packet);
    }
    EXPECT_EQ(hw.sends, sends);  // Recovery must not depend on sending another packet.
  }
}

TEST(SX126xBusy, PersistentBusyDuringPreparationAndRestorationRequiresConfigure) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(true, true);
  hw.radio.register_listener(&listener);
  const auto configured_params = hw.packet_params;
  hw.clear_trace();
  auto normal_busy = hw.busy.on_read;
  hw.busy.on_read = [&]() { return hw.transactions.size() >= 3 || normal_busy(); };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  EXPECT_FALSE(hw.receiving);
  EXPECT_TRUE(hw.radio.status_has_warning());
  EXPECT_EQ(hw.sends, 0u);
  hw.busy.on_read = normal_busy;
  hw.clear_trace();
  hw.radio.set_mode_rx();
  hw.radio.run_image_cal();
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(hw.radio.status_has_warning());
  hw.radio.configure();
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.packet_params, configured_params);
  hw.irq = IRQ_RX_DONE;
  hw.radio.loop();
  ASSERT_EQ(listener.packets.size(), 1u);
  EXPECT_EQ(listener.packets[0], hw.packet);
  EXPECT_EQ(hw.sends, 0u);
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSKWithRxOrSleepAndFixedOrVariablePayload, SX126xBusyTransmit,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()));

class SX126xBusyReceive : public ::testing::TestWithParam<bool> {};

TEST_P(SX126xBusyReceive, KeepsAPacketWhoseReadFailedAndRetries) {
  const bool lora = GetParam();
  for (size_t fault = 1; fault <= 5; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    PacketListener listener;
    hardware.configure(lora, true);
    hardware.radio.register_listener(&listener);
    hardware.clear_trace();
    hardware.irq = IRQ_RX_DONE;
    hardware.fail_at = fault;
    hardware.radio.loop();
    hardware.expect_aborted(fault);
    EXPECT_TRUE(listener.packets.empty());
    EXPECT_TRUE(hardware.radio.status_has_warning());
    EXPECT_FALSE(hardware.radio.is_failed());
    // The flags may already be cleared; the packet is still read on the retry.
    hardware.clear_trace();
    LoopBlockingGuard retry{nullptr, nullptr, App.get_loop_component_start_time() + 200};
    hardware.radio.loop();
    ASSERT_EQ(listener.packets.size(), 1u);
    EXPECT_EQ(listener.packets[0], hardware.packet);
    EXPECT_FLOAT_EQ(listener.rssi, lora ? -40 : -42);
    EXPECT_FLOAT_EQ(listener.snr, lora ? 10 : 0);
    EXPECT_FALSE(hardware.radio.status_has_warning());
  }
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSK, SX126xBusyReceive, ::testing::Bool());

// A packet whose flags were cleared before its read failed, then held by a send, survives a
// second failed read
TEST(SX126xBusy, HeldPacketSurvivesASecondFailedRead) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(true, true);
  hw.radio.register_listener(&listener);
  hw.clear_trace();
  hw.irq = IRQ_RX_DONE;
  hw.fail_at = 3;  // the first read after the flags were cleared
  hw.radio.loop();
  ASSERT_EQ(hw.irq, 0);
  ASSERT_TRUE(listener.packets.empty());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY);
  hw.clear_trace();
  hw.fail_at = 2;  // the held packet's read
  LoopBlockingGuard first{nullptr, nullptr, App.get_loop_component_start_time() + 200};
  hw.radio.loop();
  ASSERT_TRUE(listener.packets.empty());
  hw.clear_trace();
  LoopBlockingGuard second{nullptr, nullptr, App.get_loop_component_start_time() + 200};
  hw.radio.loop();
  ASSERT_EQ(listener.packets.size(), 1u);
  EXPECT_EQ(listener.packets[0], hw.packet);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}

TEST(SX126xBusy, KeepsPollingAnUnclearedInterruptAfterBusyTimeout) {
  RadioHardware hardware;
  PacketListener listener;
  hardware.configure(true, true);
  hardware.radio.register_listener(&listener);
  hardware.dio.internal = true;  // No new ISR edge while the pending IRQ stays high.
  hardware.clear_trace();
  hardware.fail_at = 1;
  hardware.irq = IRQ_RX_DONE;
  hardware.radio.call();
  EXPECT_TRUE(listener.packets.empty());
  hardware.clear_trace();
  LoopBlockingGuard retry{nullptr, nullptr, App.get_loop_component_start_time() + 200};
  hardware.radio.call();
  EXPECT_EQ(listener.packets.size(), 1u);
}

TEST(SX126xBusy, AssertsChipSelectBeforeWaitingForWakeup) {
  RadioHardware hardware;
  hardware.configure(true, true);
  hardware.clear_trace();
  hardware.busy.level = true;  // The CS callback releases BUSY only after selecting the chip.
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::NONE);
  EXPECT_TRUE(hardware.cs.level);
  EXPECT_EQ(hardware.ends, hardware.transactions.size());
}

TEST(SX126xBusy, RejectsSendBeforeSetupWithoutTouchingPins) {
  RadioHardware hardware;
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hardware.transactions.empty());
  EXPECT_EQ(hardware.busy.reads, 0u);
  EXPECT_EQ(hardware.dio.reads, 0u);
}

TEST(SX126xBusy, UnsupportedVersionFailsSetup) {
  RadioHardware hardware;
  hardware.supported_version = false;
  hardware.configure(true, true);
  EXPECT_TRUE(hardware.radio.is_failed());
}

TEST(SX126xBusy, RuntimeVersionFailureRequiresConfigureWithoutPermanentlyFailing) {
  RadioHardware hardware;
  hardware.configure(true, true);
  hardware.supported_version = false;
  hardware.radio.configure();
  EXPECT_FALSE(hardware.radio.is_failed());
  EXPECT_TRUE(hardware.radio.status_has_warning());
  hardware.clear_trace();
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hardware.transactions.empty());
  hardware.supported_version = true;
  hardware.radio.configure();
  EXPECT_FALSE(hardware.radio.status_has_warning());
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::NONE);
}

TEST(SX126xBusy, FailedReconfigurationRequiresSuccessfulConfigureBeforeOperations) {
  RadioHardware reference;
  reference.configure(true, true);
  const size_t count = reference.transactions.size();
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(true, true);
    hw.radio.register_listener(&listener);
    hw.clear_trace();
    hw.fail_at = fault;
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.is_failed());
    hw.clear_trace();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    hw.irq = IRQ_RX_DONE;
    hw.radio.loop();
    hw.radio.set_mode_rx();
    hw.radio.set_mode_tx();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.set_mode_sleep();
    hw.radio.run_image_cal();
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(listener.packets.empty());
    hw.radio.configure();
    hw.irq = 0;
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  }
}

TEST(SX126xBusy, RetryClearsStaleTxDoneBeforeWaitingForDelayedCompletion) {
  RadioHardware reference;
  reference.configure(true, true);
  reference.clear_trace();
  reference.radio.transmit_packet(reference.packet);
  size_t cleanup = 0;
  bool sent = false;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      sent = true;
    }
    if (sent && reference.transactions[i][0] == RADIO_CLR_IRQSTATUS) {
      cleanup = i + 1;
      break;
    }
  }
  ASSERT_NE(cleanup, 0u);
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  hw.fail_at = cleanup;
  ASSERT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  EXPECT_EQ(hw.irq & IRQ_TX_DONE, 0);  // Shared restoration retries the failed IRQ clear.
  hw.irq = IRQ_TX_DONE;                // A stale flag must still not finish the next TX.
  hw.clear_trace();
  hw.auto_complete = false;
  unsigned polls = 0;
  hw.dio.on_read = [&]() {
    if (++polls == 5) {
      hw.irq |= IRQ_TX_DONE;
    }
    return hw.irq != 0;
  };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  EXPECT_EQ(polls, 5u);
  EXPECT_TRUE(hw.receiving);
}

}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
template<typename T>
concept RawSpiAccessible = requires(T &device) { device.transfer_byte(0); };
template<typename T>
concept ManualSelectionAccessible = requires(T &device) {
  device.enable();
  device.disable();
};
static_assert(!RawSpiAccessible<SX126x>);
static_assert(!RawSpiAccessible<Sx126xSpiDevice>);
static_assert(!ManualSelectionAccessible<SX126x>);
static_assert(!ManualSelectionAccessible<Sx126xSpiDevice>);
// The radio runs every command sequence and state change. Without component status or
// listeners it cannot run user code mid-operation; SX126x::apply_() does that afterwards.
static_assert(!std::is_base_of_v<Component, Sx126xRadio>);
}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
TEST(SX126xBusy, FailedModeChangeCannotBeClearedByAnotherModeCommand) {
  for (size_t fault = 1; fault <= 3; fault++) {
    RadioHardware hw;
    hw.configure(true, true);
    hw.clear_trace();
    hw.fail_at = fault;
    hw.radio.set_mode_rx();
    EXPECT_TRUE(hw.radio.status_has_warning());
    hw.clear_trace();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.run_image_cal();
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_TRUE(hw.receiving);
  }
}

TEST(SX126xBusy, ConfigurationCannotReenterSendPreparationOrRestoration) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.call();  // Advance from setup into ESPHome loop scheduling.
  hw.clear_trace();
  auto write = hw.cs.on_write;
  unsigned guarded = 0;
  hw.cs.on_write = [&](bool high) {
    write(high);
    if (!high || hw.transactions.back().empty() || hw.transactions.back()[0] != RADIO_SET_STANDBY) {
      return;
    }
    guarded++;
    const auto commands = hw.transactions.size();
    hw.radio.configure();
    hw.radio.set_mode_rx();
    hw.radio.loop();
    EXPECT_TRUE(hw.radio.is_in_loop_state());
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY);
    EXPECT_EQ(hw.transactions.size(), commands);
  };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  EXPECT_EQ(guarded, 2u);
  EXPECT_TRUE(hw.radio.is_in_loop_state());
  EXPECT_TRUE(hw.receiving);
}
}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
TEST(SX126xBusy, InvalidPayloadDoesNotEnterAnOperation) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  EXPECT_EQ(hw.radio.transmit_packet(nullptr, 3), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 0), SX126xError::INVALID_PARAMS);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}

TEST(SX126xBusy, ColdSleepIsWokenByAModeCommandAndWarmSleepRetainsIt) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_mode_sleep(false);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  hw.radio.set_mode_sleep(true);
  hw.clear_trace();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  // Sleeping again leaves it asleep
  hw.radio.set_mode_sleep(false);
  EXPECT_TRUE(hw.transactions.empty());
  // Any other mode command applies the lost configuration first
  hw.radio.set_mode_rx();
  EXPECT_FALSE(hw.transactions.empty());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}

// Runs the scheduler for ms milliseconds of real time, as the main loop would
static void run_scheduler(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    App.scheduler.call(millis());
    delay(5);
  }
}

TEST(SX126xBusy, FailedModeChangeIsReconfiguredWithoutBeingAsked) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  hw.fail_at = 1;
  hw.radio.set_mode_rx();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  run_scheduler(1100);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  // Once configured, the retry stops
  hw.clear_trace();
  run_scheduler(1100);
  EXPECT_TRUE(hw.transactions.empty());
}

TEST(SX126xBusy, ColdSleepIsNotReconfiguredWithoutBeingAsked) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_mode_sleep(true);
  hw.clear_trace();
  run_scheduler(1100);
  EXPECT_TRUE(hw.transactions.empty());
}

TEST(SX126xBusy, PacketThatFailedItsCrcDoesNotHoldUpASend) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.irq = IRQ_RX_DONE | IRQ_CRC_ERROR;
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}

TEST(SX126xBusy, PacketCompletingDuringAReadIsReadNext) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(true, true);
  hw.radio.register_listener(&listener);
  bool second = false;
  hw.on_command = [&](uint8_t opcode) {
    if (opcode == RADIO_READ_BUFFER && !second) {
      second = true;
      hw.irq |= IRQ_RX_DONE;
    }
  };
  hw.irq = IRQ_RX_DONE;
  hw.radio.loop();
  EXPECT_EQ(listener.packets.size(), 1u);
  EXPECT_EQ(hw.irq & IRQ_RX_DONE, IRQ_RX_DONE);  // DIO1 stays high for the second packet
  hw.radio.loop();
  EXPECT_EQ(listener.packets.size(), 2u);
}

TEST(SX126xBusy, UnavailableLoopStopsAndConfigurationResumesReceiveWithoutAnInterrupt) {
  for (bool internal : {false, true}) {
    for (unsigned cause = 0; cause < 3; cause++) {
      SCOPED_TRACE(internal);
      SCOPED_TRACE(cause);
      RadioHardware hw;
      PacketListener received;
      hw.configure(true, true);
      hw.radio.register_listener(&received);
      // Set after setup: the fake pin cannot attach a hardware interrupt.
      hw.dio.internal = internal;
      hw.clear_trace();
      if (cause == 0) {
        hw.radio.set_mode_sleep(true);
      } else if (cause == 1) {
        hw.fail_at = 1;
        hw.radio.configure();
      } else {
        hw.fail_at = 1;
        hw.radio.set_mode_rx();
      }
      hw.clear_trace();
      hw.radio.call();
      ASSERT_TRUE(hw.radio.is_idle());
      EXPECT_TRUE(hw.transactions.empty());
      EXPECT_EQ(hw.dio.reads, 0u);
      hw.radio.configure();
      ASSERT_TRUE(hw.radio.is_in_loop_state());
      hw.irq = IRQ_RX_DONE;  // No ISR edge: configure() must resume polling itself.
      hw.radio.call();
      ASSERT_EQ(received.packets.size(), 1u);
      EXPECT_EQ(received.packets[0], hw.packet);
    }
  }
}
}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
TEST(SX126xProfile, SettersDoNotChangeAppliedPacketOrRestorationPolicy) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_payload_length(2);
  hw.radio.set_rx_start(false);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  EXPECT_TRUE(hw.receiving);
  hw.radio.configure();
  EXPECT_EQ(hw.radio.get_max_packet_size(), 2u);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 2), SX126xError::NONE);
  EXPECT_FALSE(hw.receiving);
}

TEST(SX126xProfile, RejectsOversizedSyncValueBeforeResetOrSpi) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_sync_value(std::vector<uint8_t>(9, 1));
  hw.clear_trace();
  const auto reset_writes = hw.rst.writes;
  hw.radio.configure();
  EXPECT_EQ(hw.rst.writes, reset_writes);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  hw.radio.set_sync_value({0x14, 0x24});
  hw.radio.configure();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}
}  // namespace esphome::sx126x::testing

#ifdef USE_HOST
namespace esphome::sx126x::testing {
template<typename F> static std::string capture_radio_logs(F action) {
  const auto previous_baud_rate = logger::global_logger->get_baud_rate();
  ::testing::internal::CaptureStdout();
  logger::global_logger->set_baud_rate(115200);
  action();
  logger::global_logger->set_baud_rate(previous_baud_rate);
  return ::testing::internal::GetCapturedStdout();
}

TEST(SX126xDiagnostics, BlockingRejectionsExplainWhyNoPacketWasSent) {
  RadioHardware hw;
  auto output = capture_radio_logs(
      [&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY); });
  EXPECT_NE(output.find("Packet send failed: radio not ready"), std::string::npos);
  hw.configure(true, true, true);
  hw.clear_trace();
  output = capture_radio_logs(
      [&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 2), SX126xError::INVALID_PARAMS); });
  EXPECT_NE(output.find("Packet send failed: invalid payload"), std::string::npos);
  EXPECT_TRUE(hw.transactions.empty());
  hw.irq = IRQ_RX_DONE;
  output =
      capture_radio_logs([&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY); });
  EXPECT_NE(output.find("Packet send failed: radio busy"), std::string::npos);
  EXPECT_EQ(hw.irq, IRQ_RX_DONE);
  EXPECT_EQ(hw.sends, 0u);
  hw.radio.loop();  // Deliver the held packet, which also clears its IRQ.
  EXPECT_EQ(hw.irq, 0u);
  hw.clear_trace();
  hw.fail_at = 1;
  output = capture_radio_logs(
      [&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT); });
  EXPECT_NE(output.find("Packet send failed: bus timeout"), std::string::npos);
  EXPECT_EQ(hw.sends, 0u);
}

TEST(SX126xDiagnostics, AsyncAdmissionLeavesRejectionReportingToItsCaller) {
  class Completion final : public SX126xTransmitListener {
   public:
    void on_transmit_complete(SX126xError result) override { ADD_FAILURE() << "Rejected send completed"; }
  } completion;
  RadioHardware hw;
  const auto output = capture_radio_logs([&]() {
    EXPECT_EQ(hw.radio.start_transmit_async(&completion, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    hw.configure(true, true, true);
    EXPECT_EQ(hw.radio.start_transmit_async(&completion, hw.packet.data(), 2), SX126xError::INVALID_PARAMS);
    hw.irq = IRQ_RX_DONE;
    EXPECT_EQ(hw.radio.start_transmit_async(&completion, hw.packet), SX126xError::SX126X_ERROR_BUSY);
  });
  EXPECT_EQ(output.find("Packet send failed"), std::string::npos);
  EXPECT_EQ(hw.sends, 0u);
}

TEST(SX126xDiagnostics, FailedSetupPrintsRequestedConfiguration) {
  RadioHardware hw;
  hw.fail_at = 1;
  hw.radio.set_pa_power(10);
  hw.configure(true, true, true);
  ASSERT_TRUE(hw.radio.is_failed());
  const auto output = capture_radio_logs([&]() { hw.radio.dump_config(); });
  EXPECT_NE(output.find("Ready for operation: NO"), std::string::npos);
  EXPECT_NE(output.find("Requested configuration:"), std::string::npos);
  EXPECT_NE(output.find("Frequency: 915000000 Hz"), std::string::npos);
  EXPECT_NE(output.find("PA Power: 10 dBm"), std::string::npos);
  EXPECT_NE(output.find("Payload Length: 3"), std::string::npos);
  EXPECT_NE(output.find("Sync Value: 0x1424"), std::string::npos);
}

TEST(SX126xDiagnostics, StagedConfigurationIsLabelledAndDoesNotAlterAppliedSettings) {
  RadioHardware hw;
  hw.configure(true, true, true);
  hw.radio.set_frequency(868000000);
  hw.radio.set_payload_length(2);
  auto output = capture_radio_logs([&]() { hw.radio.dump_config(); });
  EXPECT_NE(output.find("Ready for operation: YES"), std::string::npos);
  EXPECT_NE(output.find("Requested configuration:"), std::string::npos);
  EXPECT_NE(output.find("Frequency: 868000000 Hz"), std::string::npos);
  EXPECT_NE(output.find("Payload Length: 2"), std::string::npos);
  EXPECT_EQ(hw.radio.get_max_packet_size(), 3u);
  hw.clear_trace();
  hw.fail_at = 1;
  hw.radio.configure();
  output = capture_radio_logs([&]() { hw.radio.dump_config(); });
  EXPECT_NE(output.find("Ready for operation: NO"), std::string::npos);
  EXPECT_NE(output.find("Frequency: 868000000 Hz"), std::string::npos);
  EXPECT_EQ(hw.radio.get_max_packet_size(), 3u);
}
class SX126xTimeoutDiagnostics : public ::testing::TestWithParam<bool> {};

TEST_P(SX126xTimeoutDiagnostics, BlockingTimeoutLogsFailureAfterRestoration) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.auto_complete = false;
  if (GetParam()) {
    hw.dio.on_read = [&]() {
      if (hw.sends > 0) {
        hw.irq = IRQ_RX_TX_TIMEOUT;
      }
      return hw.irq != 0;
    };
  }
  const auto output =
      capture_radio_logs([&]() { EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::TIMEOUT); });
  EXPECT_NE(output.find("Packet send failed: timeout"), std::string::npos);
  EXPECT_TRUE(hw.receiving);
  EXPECT_FALSE(hw.radio.is_transmitting());
  EXPECT_FALSE(hw.radio.status_has_warning());  // Restoration itself succeeded.
}

INSTANTIATE_TEST_SUITE_P(SoftwareDeadlineAndRadioTimeout, SX126xTimeoutDiagnostics, ::testing::Bool());

TEST(SX126xDiagnostics, AsyncTimeoutLeavesCompletionReportingToItsCaller) {
  struct Completion final : SX126xTransmitListener {
    void on_transmit_complete(SX126xError result) override {
      this->count++;
      EXPECT_EQ(result, SX126xError::TIMEOUT);
    }
    unsigned count{0};
  } completion;
  RadioHardware hw;
  hw.configure(true, true);
  const auto output = capture_radio_logs([&]() {
    ASSERT_EQ(hw.radio.start_transmit_async(&completion, hw.packet), SX126xError::NONE);
    hw.irq = IRQ_RX_TX_TIMEOUT;
    hw.radio.loop();
  });
  EXPECT_EQ(completion.count, 1u);
  EXPECT_EQ(output.find("Packet send failed"), std::string::npos);
}

}  // namespace esphome::sx126x::testing
#endif
