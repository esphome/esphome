#include "sx126x_test_helpers.h"
#include "esphome/core/application.h"
#include <tuple>

namespace esphome::sx126x::testing {

class TransmitListener : public SX126xTransmitListener {
 public:
  void on_transmit_complete(SX126xError result) override {
    this->results.push_back(result);
    if (this->on_complete) {
      this->on_complete(result);
    }
  }
  std::vector<SX126xError> results;
  std::function<void(SX126xError)> on_complete;
};

class ReceiveCallback : public SX126xListener {
 public:
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override { this->on_packet_callback(); }
  std::function<void()> on_packet_callback;
};

class SX126xAsyncModes : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {};

TEST_P(SX126xAsyncModes, ReturnsBeforeCompletionAndRestoresMode) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(lora, rx, fixed);
  hw.auto_complete = false;
  hw.clear_trace();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1234), SX126xError::NONE);
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_TRUE(listener.results.empty());
  EXPECT_EQ(hw.sends, 1u);
  const auto &tx = hw.transactions.back();
  EXPECT_EQ(tx, (std::vector<uint8_t>{RADIO_SET_TX, 0x01, 0x34, 0x80}));
  const auto fifo = std::find_if(hw.transactions.begin(), hw.transactions.end(),
                                 [](const auto &cmd) { return cmd[0] == RADIO_WRITE_BUFFER; });
  ASSERT_NE(fifo, hw.transactions.end());
  EXPECT_EQ(*fifo, (std::vector<uint8_t>{RADIO_WRITE_BUFFER, 0, 0x11, 0x22, 0x33}));
  hw.packet.assign(3, 0x99);  // The driver must not retain the caller's payload.
  hw.irq = IRQ_TX_DONE;
  listener.on_complete = [&](SX126xError result) {
    EXPECT_FALSE(hw.radio.is_transmitting());
    EXPECT_EQ(hw.receiving, rx);
    EXPECT_TRUE(hw.cs.level);
  };
  hw.radio.loop();
  hw.radio.loop();
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

TEST_P(SX126xAsyncModes, RejectsEveryFailedStartWithoutCallbackAndCanRetry) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  TransmitListener listener;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.start_transmit_async(&listener, reference.packet), SX126xError::NONE);
  const size_t count = reference.transactions.size();
  reference.radio.cancel_transmit_async(&listener);
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    TransmitListener result;
    hw.configure(lora, rx, fixed);
    hw.clear_trace();
    hw.fail_at = fault;
    EXPECT_EQ(hw.radio.start_transmit_async(&result, hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(result.results.empty());
    EXPECT_FALSE(hw.radio.is_transmitting());
    EXPECT_TRUE(hw.cs.level);
    EXPECT_TRUE(hw.transactions[fault - 1].empty());
    EXPECT_EQ(hw.sends, 0u);
    EXPECT_EQ(hw.receiving, rx);  // One-shot fault permits failed-start cleanup.
    hw.clear_trace();
    hw.radio.configure();
    ASSERT_EQ(hw.radio.start_transmit_async(&result, hw.packet), SX126xError::NONE);
    hw.radio.loop();
    EXPECT_EQ(result.results, (std::vector<SX126xError>{SX126xError::NONE}));
  }
}

TEST_P(SX126xAsyncModes, ReportsEveryCompletionAndCleanupFailureOnce) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  TransmitListener listener;
  reference.configure(lora, rx, fixed);
  ASSERT_EQ(reference.radio.start_transmit_async(&listener, reference.packet), SX126xError::NONE);
  reference.clear_trace();
  reference.radio.loop();
  const size_t count = reference.transactions.size();
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    TransmitListener result;
    hw.configure(lora, rx, fixed);
    ASSERT_EQ(hw.radio.start_transmit_async(&result, hw.packet), SX126xError::NONE);
    hw.clear_trace();
    hw.fail_at = fault;
    hw.radio.loop();
    EXPECT_EQ(result.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_BUSY_TIMEOUT}));
    EXPECT_TRUE(hw.transactions[fault - 1].empty());
    EXPECT_TRUE(hw.cs.level);
    EXPECT_FALSE(hw.radio.is_transmitting());
    hw.radio.loop();
    EXPECT_EQ(result.results.size(), 1u);
    hw.clear_trace();
    hw.radio.configure();
    ASSERT_EQ(hw.radio.start_transmit_async(&result, hw.packet), SX126xError::NONE);
    hw.radio.loop();
    ASSERT_EQ(result.results.size(), 2u);
    EXPECT_EQ(result.results.back(), SX126xError::NONE);
    EXPECT_EQ(hw.receiving, rx);
  }
}

TEST_P(SX126xAsyncModes, BlockingAndAsyncUseTheSameCommandSequence) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware blocking, async;
  TransmitListener listener;
  blocking.configure(lora, rx, fixed);
  async.configure(lora, rx, fixed);
  blocking.clear_trace();
  async.clear_trace();
  ASSERT_EQ(blocking.radio.transmit_packet(blocking.packet), SX126xError::NONE);
  ASSERT_EQ(async.radio.start_transmit_async(&listener, async.packet), SX126xError::NONE);
  async.radio.loop();
  EXPECT_EQ(blocking.transactions, async.transactions);
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::NONE}));
  EXPECT_EQ(blocking.receiving, async.receiving);
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSKWithRxOrSleepAndFixedOrVariablePayload, SX126xAsyncModes,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()));

TEST(SX126xAsync, SoftwareDeadlineWithoutDioAndCachedTickBeforeStart) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  hw.dio.internal = true;
  const uint32_t before = millis();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 50), SX126xError::NONE);
  const uint32_t after = millis();
  {
    LoopBlockingGuard clock{nullptr, nullptr, before - 1};
    hw.radio.call();
    EXPECT_TRUE(listener.results.empty());  // No unsigned underflow on the admitting loop tick.
  }
  {
    LoopBlockingGuard clock{nullptr, nullptr, after + 51};
    hw.radio.call();  // Still enabled despite no IRQ or ISR edge.
  }
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::TIMEOUT}));
  EXPECT_TRUE(hw.receiving);
  EXPECT_FALSE(hw.radio.is_transmitting());
}

TEST(SX126xAsync, UnrelatedIrqIsNotSuccessAndTimeoutWinsOverTxDone) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  hw.irq = IRQ_RX_DONE;
  {
    LoopBlockingGuard clock{nullptr, nullptr, millis()};
    hw.radio.loop();
  }
  EXPECT_TRUE(listener.results.empty());
  hw.irq = IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT;
  hw.radio.loop();
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::TIMEOUT}));
}

TEST(SX126xAsync, CancelRequiresOwnerAndCompletesSynchronouslyOnce) {
  RadioHardware hw;
  TransmitListener listener, other;
  hw.configure(true, true);
  hw.auto_complete = false;
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&listener));
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  EXPECT_FALSE(hw.radio.cancel_transmit_async(nullptr));
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&other));
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&listener));
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_CANCELLED}));
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&listener));
  hw.irq = IRQ_TX_DONE;  // A late IRQ cannot deliver a second completion.
  hw.radio.loop();
  EXPECT_EQ(listener.results.size(), 1u);
  EXPECT_TRUE(hw.receiving);
  hw.irq = IRQ_TX_DONE;  // Also cleared before a new send starts.
  ASSERT_EQ(hw.radio.start_transmit_async(&other, hw.packet), SX126xError::NONE);
  {
    LoopBlockingGuard clock{nullptr, nullptr, millis()};
    hw.radio.loop();
  }
  EXPECT_TRUE(other.results.empty());
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&other));
}

TEST(SX126xAsync, CompletionCallbackCanStartNextSend) {
  RadioHardware hw;
  TransmitListener first, second;
  hw.configure(true, true);
  hw.auto_complete = false;
  first.on_complete = [&](SX126xError result) {
    EXPECT_EQ(hw.radio.start_transmit_async(&second, hw.packet), SX126xError::NONE);
  };
  ASSERT_EQ(hw.radio.start_transmit_async(&first, hw.packet), SX126xError::NONE);
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  EXPECT_EQ(first.results.size(), 1u);
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_TRUE(second.results.empty());
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  EXPECT_EQ(second.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

static void expect_conflicting_calls_rejected(RadioHardware &hw, TransmitListener &listener) {
  const size_t count = hw.transactions.size();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_BUSY);
  hw.radio.set_mode_rx();
  hw.radio.set_mode_tx();
  hw.radio.set_mode_standby(STDBY_XOSC);
  hw.radio.set_mode_sleep(true);
  hw.radio.configure();
  hw.radio.run_image_cal();
  EXPECT_EQ(hw.transactions.size(), count);
}

TEST(SX126xAsync, PendingReceiveRejectsAdmissionWithoutDispatchOrAcknowledgment) {
  RadioHardware hw;
  TransmitListener listener;
  PacketListener receiver;
  hw.configure(true, true);
  hw.radio.register_listener(&receiver);
  hw.irq = IRQ_RX_DONE;
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_BUSY);
  EXPECT_TRUE(receiver.packets.empty());
  EXPECT_TRUE(listener.results.empty());
  EXPECT_EQ(hw.irq, IRQ_RX_DONE);
  hw.radio.loop();
  EXPECT_EQ(receiver.packets.size(), 1u);
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  expect_conflicting_calls_rejected(hw, listener);
  hw.radio.loop();
  EXPECT_EQ(listener.results.size(), 1u);
}

TEST(SX126xAsync, ValidatesReadinessPayloadAndTimeoutWithoutSpi) {
  RadioHardware hw;
  TransmitListener listener;
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hw.transactions.empty());
  hw.configure(true, true, true);
  hw.clear_trace();
  EXPECT_EQ(hw.radio.start_transmit_async(nullptr, hw.packet), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, nullptr, 3), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet.data(), 0), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet.data(), 2), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet.data(), 256), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 0), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 262144), SX126xError::INVALID_PARAMS);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(listener.results.empty());
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 262143), SX126xError::NONE);
  EXPECT_EQ(hw.transactions.back(), (std::vector<uint8_t>{RADIO_SET_TX, 0xFF, 0xFF, 0xC0}));
  hw.radio.loop();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet, 1), SX126xError::NONE);
  EXPECT_EQ(hw.transactions.back(), (std::vector<uint8_t>{RADIO_SET_TX, 0, 0, 64}));
  hw.radio.loop();
}

TEST(SX126xAsync, PersistentBusyReportsFailedCleanupAndAllowsLaterRecovery) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.auto_complete = false;
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  hw.busy.on_read = []() { return true; };
  ASSERT_TRUE(hw.radio.cancel_transmit_async(&listener));
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_BUSY_TIMEOUT}));
  EXPECT_FALSE(hw.radio.is_transmitting());
  EXPECT_TRUE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.cs.level);
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_EQ(listener.results.size(), 1u);  // Failed admission never invokes completion.
  hw.busy.on_read = []() { return false; };
  hw.radio.configure();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  ASSERT_EQ(listener.results.size(), 2u);
  EXPECT_EQ(listener.results.back(), SX126xError::NONE);
  EXPECT_TRUE(hw.receiving);
  EXPECT_FALSE(hw.radio.status_has_warning());
}

TEST(SX126xAsync, CancellationCallbackMayStartAnotherSend) {
  RadioHardware hw;
  TransmitListener first, second;
  hw.configure(true, true);
  hw.auto_complete = false;
  first.on_complete = [&](SX126xError result) {
    EXPECT_EQ(result, SX126xError::SX126X_ERROR_CANCELLED);
    EXPECT_EQ(hw.radio.start_transmit_async(&second, hw.packet), SX126xError::NONE);
  };
  ASSERT_EQ(hw.radio.start_transmit_async(&first, hw.packet), SX126xError::NONE);
  EXPECT_TRUE(hw.radio.cancel_transmit_async(&first));
  EXPECT_TRUE(hw.radio.is_transmitting());
  EXPECT_FALSE(hw.radio.cancel_transmit_async(&first));
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  EXPECT_EQ(second.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

TEST(SX126xAsync, RejectsAfterFailedSetupWithoutBusAccess) {
  RadioHardware hw;
  TransmitListener listener;
  hw.fail_at = 1;
  hw.configure(true, true);
  ASSERT_TRUE(hw.radio.is_failed());
  hw.clear_trace();
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(listener.results.empty());
}

TEST(SX126xAsync, TwoRadiosHaveIndependentOwnersAndCompletion) {
  RadioHardware a, b;
  TransmitListener first, second;
  a.configure(true, true);
  b.configure(false, false);
  a.auto_complete = b.auto_complete = false;
  ASSERT_EQ(a.radio.start_transmit_async(&first, a.packet), SX126xError::NONE);
  ASSERT_EQ(b.radio.start_transmit_async(&second, b.packet), SX126xError::NONE);
  a.radio.cancel_transmit_async(&first);
  EXPECT_TRUE(b.radio.is_transmitting());
  EXPECT_TRUE(second.results.empty());
  b.irq = IRQ_TX_DONE;
  b.radio.loop();
  EXPECT_EQ(first.results, (std::vector<SX126xError>{SX126xError::SX126X_ERROR_CANCELLED}));
  EXPECT_EQ(second.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

TEST(SX126xAsync, BackToBackReceiveEchoPreservesBorrowedPacketForAllListeners) {
  class EchoListener : public SX126xListener {
   public:
    std::function<void(const std::vector<uint8_t> &)> callback;
    void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override { callback(packet); }
  } echo;
  RadioHardware hw;
  TransmitListener tx;
  PacketListener observer;
  hw.configure(true, true);
  hw.auto_complete = false;
  hw.radio.register_listener(&echo);
  hw.radio.register_listener(&observer);
  const auto first = hw.packet;
  const std::vector<uint8_t> second(200, 0xA5);
  unsigned calls = 0;
  echo.callback = [&](const std::vector<uint8_t> &packet) {
    if (++calls == 1) {
      hw.packet = second;  // Larger back-to-back RX could reallocate the driver's packet_.
      hw.irq = IRQ_RX_DONE;
      const auto count = hw.transactions.size();
      hw.radio.loop();  // Explicit loop reentry must not overwrite the borrowed RX buffer either.
      EXPECT_EQ(hw.radio.start_transmit_async(&tx, packet), SX126xError::SX126X_ERROR_BUSY);
      EXPECT_EQ(hw.transactions.size(), count + 1);
      EXPECT_EQ(hw.transactions.back()[0], RADIO_GET_IRQSTATUS);
      EXPECT_EQ(packet, first);
    } else {
      EXPECT_EQ(packet, second);
      EXPECT_EQ(hw.radio.start_transmit_async(&tx, packet), SX126xError::NONE);
    }
  };
  hw.irq = IRQ_RX_DONE;
  hw.radio.loop();
  ASSERT_EQ(observer.packets.size(), 1u);
  EXPECT_EQ(observer.packets[0], first);
  EXPECT_TRUE(tx.results.empty());
  EXPECT_FALSE(hw.radio.is_transmitting());
  hw.radio.loop();
  ASSERT_EQ(observer.packets.size(), 2u);
  EXPECT_EQ(observer.packets[1], second);
  ASSERT_EQ(calls, 2u);
  auto fifo = std::find_if(hw.transactions.rbegin(), hw.transactions.rend(),
                           [](const auto &cmd) { return !cmd.empty() && cmd[0] == RADIO_WRITE_BUFFER; });
  ASSERT_NE(fifo, hw.transactions.rend());
  EXPECT_EQ(std::vector<uint8_t>(fifo->begin() + 2, fifo->end()), second);
  hw.irq = IRQ_TX_DONE;
  hw.radio.loop();
  EXPECT_EQ(tx.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

TEST(SX126xAsync, FailedReconfigurationRejectsAsyncUntilReconfigured) {
  RadioHardware hw;
  TransmitListener listener;
  hw.configure(true, true);
  hw.clear_trace();
  hw.fail_at = 3;
  hw.radio.configure();
  hw.clear_trace();
  EXPECT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(listener.results.empty());
  hw.radio.configure();
  ASSERT_EQ(hw.radio.start_transmit_async(&listener, hw.packet), SX126xError::NONE);
  hw.radio.loop();
  EXPECT_EQ(listener.results, (std::vector<SX126xError>{SX126xError::NONE}));
}

TEST(SX126xAsync, BlockingSendUsesIrqResultInsteadOfTreatingAnyDioAsSuccess) {
  RadioHardware hw;
  hw.configure(true, true);
  auto write = hw.cs.on_write;
  hw.cs.on_write = [&](bool high) {
    write(high);
    if (high && !hw.transactions.back().empty() && hw.transactions.back()[0] == RADIO_SET_TX) {
      hw.irq = IRQ_RX_TX_TIMEOUT;
    }
  };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::TIMEOUT);
  EXPECT_TRUE(hw.receiving);
}

TEST(SX126xAsync, FailedAdmissionRestoresReceivePollingWithoutAnotherTransmission) {
  RadioHardware hw;
  PacketListener rx;
  TransmitListener tx;
  hw.configure(true, true);
  hw.radio.register_listener(&rx);
  const auto configured_params = hw.packet_params;
  hw.dio.internal = true;
  hw.radio.call();
  ASSERT_TRUE(hw.radio.is_idle());
  hw.clear_trace();
  hw.fail_at = 3;
  EXPECT_EQ(hw.radio.start_transmit_async(&tx, hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  EXPECT_TRUE(tx.results.empty());
  EXPECT_TRUE(hw.radio.is_in_loop_state());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.packet_params, configured_params);
  hw.clear_trace();
  hw.irq = IRQ_RX_DONE;  // No simulated ISR edge: restoration must re-enable polling.
  hw.radio.call();
  ASSERT_EQ(rx.packets.size(), 1u);
  EXPECT_EQ(rx.packets[0], hw.packet);
  EXPECT_EQ(hw.sends, 0u);
}

}  // namespace esphome::sx126x::testing
