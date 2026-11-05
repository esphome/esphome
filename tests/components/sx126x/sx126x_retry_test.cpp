#include "sx126x_test_helpers.h"
#include "esphome/core/application.h"
#include <tuple>

namespace esphome::sx126x::testing {

struct RejectedCompletion final : SX126xTransmitListener {
  void on_transmit_complete(SX126xError result) override { ADD_FAILURE() << "Rejected transmission completed"; }
};

class SX126xReceiveRetry : public ::testing::TestWithParam<std::tuple<bool, unsigned, bool>> {};

TEST_P(SX126xReceiveRetry, PersistentBusyIsRateLimitedAndRecoversWithoutNewEdge) {
  const auto [held_receive, failed_command, rollover] = GetParam();
  RadioHardware hw;
  PacketListener listener;
  RejectedCompletion tx;
  hw.configure(true, true);
  hw.radio.register_listener(&listener);
  hw.radio.call();
  hw.dio.internal = true;
  if (held_receive) {
    auto write = hw.cs.on_write;
    hw.cs.on_write = [&](bool high) {
      write(high);
      if (high && !hw.transactions.back().empty() && hw.transactions.back()[0] == RADIO_SET_STANDBY) {
        hw.irq = IRQ_RX_DONE;
      }
    };
    EXPECT_EQ(hw.radio.start_transmit_async(&tx, hw.packet), SX126xError::SX126X_ERROR_BUSY);
    hw.cs.on_write = write;
  }
  hw.clear_trace();
  hw.irq = IRQ_RX_DONE;
  hw.fail_at = failed_command;
  const uint32_t start = rollover ? 0xFFFFFFF0u : 1000;
  {
    LoopBlockingGuard clock{nullptr, nullptr, start};
    hw.radio.call();
  }
  hw.expect_aborted(failed_command);
  EXPECT_EQ(hw.irq, IRQ_RX_DONE);
  const auto normal_busy = hw.busy.on_read;
  hw.busy.on_read = []() { return true; };
  for (uint32_t elapsed = 1; elapsed < 200; elapsed++) {
    LoopBlockingGuard clock{nullptr, nullptr, start + elapsed};
    hw.radio.call();
  }
  EXPECT_EQ(hw.transactions.size(), failed_command);  // No SPI/BUSY waits inside the retry interval.
  {
    LoopBlockingGuard clock{nullptr, nullptr, start + 200};
    hw.radio.call();
  }
  ASSERT_EQ(hw.transactions.size(), failed_command + 1);
  EXPECT_TRUE(hw.transactions.back().empty());
  EXPECT_TRUE(hw.radio.is_in_loop_state());
  EXPECT_TRUE(hw.cs.level);
  EXPECT_EQ(hw.irq, IRQ_RX_DONE);
  EXPECT_TRUE(listener.packets.empty());
  hw.busy.on_read = normal_busy;
  hw.fail_at = 0;
  {
    LoopBlockingGuard clock{nullptr, nullptr, start + 399};
    hw.radio.call();
    EXPECT_EQ(hw.transactions.size(), failed_command + 1);
  }
  {
    LoopBlockingGuard clock{nullptr, nullptr, start + 400};
    hw.radio.call();
  }
  ASSERT_EQ(listener.packets.size(), 1u);
  EXPECT_EQ(listener.packets.front(), hw.packet);
  EXPECT_FALSE(hw.radio.status_has_warning());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.sends, 0u);
  hw.radio.call();
  EXPECT_EQ(listener.packets.size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(IdleAndHeldRxWithIrqAndPacketFailuresAcrossRollover, SX126xReceiveRetry,
                         ::testing::Combine(::testing::Bool(), ::testing::Values(1u, 2u), ::testing::Bool()));

TEST(SX126xReceiveRetry, SuccessfulConfigurationClearsBackoffAndOtherRadiosRemainIndependent) {
  RadioHardware failed, other;
  PacketListener recovered, unaffected;
  failed.configure(true, true);
  other.configure(true, true);
  failed.radio.register_listener(&recovered);
  other.radio.register_listener(&unaffected);
  failed.clear_trace();
  failed.fail_at = 1;
  failed.irq = IRQ_RX_DONE;
  LoopBlockingGuard clock{nullptr, nullptr, 1000};
  failed.radio.loop();
  other.irq = IRQ_RX_DONE;
  other.radio.loop();
  ASSERT_EQ(unaffected.packets.size(), 1u);
  EXPECT_TRUE(recovered.packets.empty());
  failed.clear_trace();
  failed.radio.configure();
  failed.irq = IRQ_RX_DONE;
  failed.radio.loop();
  ASSERT_EQ(recovered.packets.size(), 1u);  // Explicit recovery does not wait for the old retry deadline.
}

TEST(SX126xReceiveRetry, ReceiveBackoffDoesNotDelayTransmitCompletion) {
  struct Completion final : SX126xTransmitListener {
    void on_transmit_complete(SX126xError result) override { this->results.push_back(result); }
    std::vector<SX126xError> results;
  } tx;
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  hw.fail_at = 1;
  hw.irq = IRQ_TX_DONE;  // Failed read of an unrelated stale IRQ starts the RX retry gate.
  LoopBlockingGuard clock{nullptr, nullptr, millis()};
  hw.radio.loop();
  EXPECT_TRUE(hw.radio.status_has_warning());
  hw.clear_trace();
  hw.irq = 0;
  ASSERT_EQ(hw.radio.start_transmit_async(&tx, hw.packet), SX126xError::NONE);
  hw.radio.loop();  // Same cached tick, before the RX retry gate opens.
  EXPECT_EQ(tx.results, (std::vector<SX126xError>{SX126xError::NONE}));
  EXPECT_TRUE(hw.receiving);
}

}  // namespace esphome::sx126x::testing
