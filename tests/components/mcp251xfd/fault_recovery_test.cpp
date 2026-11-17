#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "register_delegate.h"

namespace esphome::mcp251xfd::testing {

TEST(MCP251XFDFaults, RetriesCrcFailuresAndPreservesLastStatusWhenReadsFail) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_TREC] = 4;
  spi.registers[REG_TREC + 1] = 9;
  spi.corrupt_next_reads(REG_TREC, 2);

  auto status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 4);
  EXPECT_EQ(status.tx_error_counter, 9);
  EXPECT_EQ(spi.reads[REG_TREC], 3u);
  EXPECT_FALSE(controller.status_has_warning());

  spi.registers[REG_TREC] = 7;
  spi.corrupt_next_reads(REG_TREC);
  status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 4);
  EXPECT_EQ(status.tx_error_counter, 9);
  EXPECT_EQ(spi.reads[REG_TREC], 6u);
  EXPECT_TRUE(controller.status_has_warning());

  status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 7);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, ClearsFifoOverflowWithoutClearingOtherBits) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_RX_FIFO_STA] = FIFO_RX_OVERFLOW | FIFO_NOT_FULL_NOT_EMPTY;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], FIFO_NOT_FULL_NOT_EMPTY);
  ASSERT_EQ(spi.writes.size(), 1u);
  EXPECT_EQ(spi.writes[0].address, REG_RX_FIFO_STA);
  EXPECT_EQ(spi.writes[0].data, (std::vector<uint8_t>{static_cast<uint8_t>(~FIFO_RX_OVERFLOW)}));
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
  EXPECT_EQ(controller.poll(200), 0);
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
}

TEST(MCP251XFDFaults, UnhandledFlagDoesNotForceErrorStateReads) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_INT + 1] = 0x40;  // WAKIF, which the driver never clears

  controller.poll(100);
  controller.poll(200);
  EXPECT_EQ(spi.reads[REG_TREC], 0u);
  EXPECT_EQ(spi.registers[REG_INT + 1], 0x40);
}

TEST(MCP251XFDFaults, FailedStatusReadLeavesFaultPendingForRetry) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  // Run the first mode check now; its successful read would clear the warning under test.
  controller.poll(0);
  spi.registers[REG_RX_FIFO_STA] = FIFO_RX_OVERFLOW;
  spi.corrupt_next_reads(REG_RX_FIFO_STA);

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], FIFO_RX_OVERFLOW);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_STA), 0u);
  EXPECT_TRUE(controller.status_has_warning());

  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], 0);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_STA), 1u);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, ReportsErrorStateTransitionsAndPreservesUnrelatedInterrupts) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_TREC] = 3;
  spi.registers[REG_TREC + 1] = 128;
  spi.set_error_flags(ERROR_TX_PASSIVE);
  constexpr uint8_t unrelated_interrupt = 0x40;
  spi.registers[REG_INT + 1] = INT_CAN_ERROR | INT_INVALID_MESSAGE | unrelated_interrupt;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_PASSIVE);
  EXPECT_EQ(spi.registers[REG_INT + 1], unrelated_interrupt);
  const auto status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 3);
  EXPECT_EQ(status.tx_error_counter, 128);
  EXPECT_EQ(status.bus_error_count, 1u);

  spi.set_error_flags(0);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_ACTIVE);
  EXPECT_EQ(controller.get_status().bus_error_count, 1u);
}

TEST(MCP251XFDRecovery, RetriesModeRequestAfterIntervalAndKeepsCheckingWhenRecovered) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;

  EXPECT_EQ(controller.poll(100), 0);
  // The first attempt requests the configured mode directly, without Configuration mode resetting the FIFOs
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.writes[0].data, (std::vector<uint8_t>{static_cast<uint8_t>(CanMode::CAN_MODE_NORMAL)}));
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_TRUE(controller.mode_fault_active());

  // A retry goes through Configuration mode
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  controller.poll(5099);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 3u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);

  controller.poll(10100);
  EXPECT_FALSE(controller.mode_fault_active());
  const auto mode_reads = spi.reads[REG_CON + 2];
  controller.poll(15099);
  EXPECT_EQ(spi.reads[REG_CON + 2], mode_reads);
  controller.poll(15100);
  EXPECT_EQ(spi.reads[REG_CON + 2], mode_reads + 1);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 3u);
}

TEST(MCP251XFDRecovery, FirstModeFaultRecoversBeforeCheckIsDue) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  controller.poll(100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);

  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  controller.poll(200);
  EXPECT_TRUE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);

  // While the fault is active, the interval from that attempt limits the next one
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  controller.poll(300);
  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  controller.poll(5200);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 3u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
}

TEST(MCP251XFDRecovery, RefusedDirectModeChangeRecoversThroughConfiguration) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi, CanMode::CAN_MODE_CLASSIC);
  spi.set_mode(CanMode::CAN_MODE_LOOPBACK);
  spi.request_mode(CanMode::CAN_MODE_CLASSIC);
  ASSERT_EQ(spi.mode(), CanMode::CAN_MODE_LOOPBACK);

  controller.poll(100);
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_LOOPBACK);

  controller.poll(5100);
  // Only the mode requests, without a reconfiguration
  ASSERT_EQ(spi.writes.size(), 3u);
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 3u);
  EXPECT_EQ(spi.writes[1].data, (std::vector<uint8_t>{static_cast<uint8_t>(CanMode::CAN_MODE_CONFIGURATION)}));
  EXPECT_EQ(spi.writes[2].data, (std::vector<uint8_t>{static_cast<uint8_t>(CanMode::CAN_MODE_CLASSIC)}));
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_CLASSIC);

  controller.poll(10100);
  EXPECT_FALSE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 3u);
}

TEST(MCP251XFDRecovery, ChipResetReconfiguresWithoutReregisteringSpi) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  // NBTCFG's reset value matches the configured timing, so only the mode shows the reset
  spi.reset();
  ASSERT_EQ(encode_value_le<uint32_t>(spi.registers.data() + REG_NBTCFG), TEST_NOMINAL_BIT_TIMING);

  controller.poll(100);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
  EXPECT_EQ(spi.writes_to(REG_CON), 1u);
  EXPECT_EQ(spi.writes_to(REG_FLTCON), 1u);
  EXPECT_EQ(spi.registers[REG_FLTCON], FILTER_ENABLE_RX_FIFO);
  EXPECT_EQ(controller.spi_setups, 0u);
  EXPECT_EQ(controller.spi_teardowns, 0u);

  controller.poll(5100);
  EXPECT_FALSE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON), 1u);
}

TEST(MCP251XFDRecovery, ChipResetDuringBusErrorsIsNotHeldBackByBusOff) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  // Earlier bus errors keep TREC polled, and a reset chip reports bus-off until it is configured
  spi.set_error_flags(ERROR_TX_PASSIVE);
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;
  controller.poll(100);
  spi.reset();

  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON), 1u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
}

TEST(MCP251XFDRecovery, PendingExitFromConfigurationIsNotAChipReset) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_CON + 2] = static_cast<uint8_t>(CanMode::CAN_MODE_CONFIGURATION) << CON_MODE_SHIFT;

  controller.poll(100);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
  EXPECT_EQ(spi.writes_to(REG_CON), 0u);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
}

TEST(MCP251XFDRecovery, ChipResetWith4MHzOscillatorReconfiguresAtSlowSpiRate) {
  RegisterDelegate spi;
  spi.oscillator_4mhz = true;
  TestMCP251XFD controller(&spi, CanMode::CAN_MODE_NORMAL, CanClock::CAN_CLOCK_4MHZ);
  const uint32_t operating_rate = spi.data_rate;
  ASSERT_GT(operating_rate, 1000000u);
  spi.registers[REG_OSC] = OSC_PLL_ENABLE;
  controller.poll(100);
  ASSERT_FALSE(controller.status_has_warning());

  // With the PLL off every read at the operating rate fails, the mode read included
  spi.reset();
  controller.poll(200);
  EXPECT_TRUE(controller.status_has_warning());
  EXPECT_EQ(spi.writes_to(REG_CON), 0u);

  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON), 1u);
  EXPECT_EQ(spi.registers[REG_OSC] & OSC_PLL_ENABLE, OSC_PLL_ENABLE);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
  EXPECT_EQ(spi.data_rate, operating_rate);
  EXPECT_EQ(controller.spi_setups, 2u);
  EXPECT_EQ(controller.spi_teardowns, 2u);

  controller.poll(5200);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDRecovery, FailedFifoResetLeavesTheChipInConfigurationForRetry) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.reset();
  // The FIFO reset read fails after the operating mode was requested
  spi.corrupt_next_reads(REG_TX_FIFO_CON + 1, 4);
  controller.poll(100);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_CONFIGURATION);

  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON), 2u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
}

TEST(MCP251XFDRecovery, OneFailedReadDoesNotResetA4MHzChip) {
  RegisterDelegate spi;
  spi.oscillator_4mhz = true;
  TestMCP251XFD controller(&spi, CanMode::CAN_MODE_NORMAL, CanClock::CAN_CLOCK_4MHZ);
  spi.registers[REG_OSC] = OSC_PLL_ENABLE;
  controller.poll(100);

  // A burst of noise on the due poll fails the INT read and its retries, with no earlier fault
  spi.corrupt_next_reads(REG_INT);
  controller.poll(5100);
  EXPECT_TRUE(controller.status_has_warning());
  EXPECT_EQ(spi.writes_to(REG_CON), 0u);
  EXPECT_EQ(controller.spi_teardowns, 0u);

  controller.poll(5200);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDRecovery, PeriodicCheckFixesStrayModeRequest) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  controller.poll(100);
  EXPECT_EQ(spi.reads[REG_CON + 2], 1u);
  EXPECT_FALSE(controller.mode_fault_active());

  // A refused request stays in REQOP without changing OPMOD or raising MODIF
  spi.registers[REG_CON + 3] = static_cast<uint8_t>(CanMode::CAN_MODE_LISTENONLY);
  controller.poll(200);
  EXPECT_EQ(spi.reads[REG_CON + 2], 1u);
  controller.poll(5100);
  EXPECT_TRUE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_CON + 3], static_cast<uint8_t>(CanMode::CAN_MODE_NORMAL));
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);

  controller.poll(10100);
  EXPECT_FALSE(controller.mode_fault_active());
}

TEST(MCP251XFDRecovery, DefersModeRequestWhileBusOff) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.set_error_flags(ERROR_BUS_OFF);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_TRUE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);

  spi.set_error_flags(0);
  EXPECT_EQ(controller.poll(300), 0);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);
}

TEST(MCP251XFDRecovery, FaultFirstSeenDuringBusOffRecoversWhenTheBusRecovers) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  controller.poll(100);  // a healthy periodic check, so the next one is not due for 5 s

  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.set_error_flags(ERROR_BUS_OFF);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);

  spi.registers[REG_INT] = 0;
  spi.set_error_flags(0);
  controller.poll(300);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);
  EXPECT_EQ(spi.mode(), CanMode::CAN_MODE_NORMAL);
}

TEST(MCP251XFDRecovery, FailedReadDefersModeRecoveryAndFlagClearing) {
  struct Case {
    uint16_t failed_register;
    uint8_t controller_flags;
    unsigned mode_reads;
  };
  // A failed mode read, and a failed error-state read that skips the mode check
  for (const auto &test :
       {Case{REG_CON + 2, INT_SYSTEM_ERROR, 3}, Case{REG_TREC, INT_SYSTEM_ERROR | INT_CAN_ERROR, 0}}) {
    SCOPED_TRACE(test.failed_register);
    RegisterDelegate spi;
    TestMCP251XFD controller(&spi);
    spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
    spi.registers[REG_INT] = INT_MODE_CHANGE;
    spi.registers[REG_INT + 1] = test.controller_flags;
    spi.corrupt_next_reads(test.failed_register);

    controller.poll(100);
    EXPECT_EQ(spi.reads[REG_CON + 2], test.mode_reads);
    EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, INT_MODE_CHANGE);
    EXPECT_EQ(spi.registers[REG_INT + 1], test.controller_flags);
    EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);
    EXPECT_TRUE(controller.status_has_warning());

    controller.poll(200);
    EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
    EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
    EXPECT_EQ(spi.registers[REG_INT + 1], 0);
    EXPECT_FALSE(controller.status_has_warning());
  }
}

TEST(MCP251XFDRecovery, SpiRecoveryDoesNotClearBusOffWarning) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_error_flags(ERROR_BUS_OFF);
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;
  auto events = controller.poll(100);
  ASSERT_EQ(events, canbus::CAN_EVENT_BUS_OFF);
  controller.log_events(events, 100);
  ASSERT_TRUE(controller.status_has_warning());

  spi.corrupt_next_reads(REG_INT);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_EQ(spi.reads[REG_INT], 4u);
  EXPECT_TRUE(controller.status_has_warning());
  EXPECT_EQ(controller.poll(300), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_TRUE(controller.status_has_warning());

  spi.set_error_flags(0);
  events = controller.poll(1400);
  EXPECT_EQ(events, 0);
  controller.log_events(events, 1400);
  EXPECT_FALSE(controller.status_has_warning());
}

}  // namespace esphome::mcp251xfd::testing
