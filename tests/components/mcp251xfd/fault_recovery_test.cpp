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

TEST(MCP251XFDFaults, ClearsFifoAndSpiFaultsWithoutClearingOtherBits) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_RX_FIFO_STA] = FIFO_RX_OVERFLOW | FIFO_NOT_FULL_NOT_EMPTY;
  spi.registers[REG_CRC + 2] = CRC_CRC_ERROR | CRC_FORMAT_ERROR | 0x80;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], FIFO_NOT_FULL_NOT_EMPTY);
  EXPECT_EQ(spi.registers[REG_CRC + 2], 0x80);
  ASSERT_EQ(spi.writes.size(), 2u);
  EXPECT_EQ(spi.writes[0].address, REG_RX_FIFO_STA);
  EXPECT_EQ(spi.writes[0].data, (std::vector<uint8_t>{static_cast<uint8_t>(~FIFO_RX_OVERFLOW)}));
  EXPECT_EQ(spi.writes[1].address, REG_CRC + 2);
  EXPECT_EQ(spi.writes[1].data, (std::vector<uint8_t>{static_cast<uint8_t>(~(CRC_CRC_ERROR | CRC_FORMAT_ERROR))}));
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
  EXPECT_EQ(controller.poll(200), 0);
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
}

TEST(MCP251XFDFaults, FailedStatusReadLeavesFaultPendingForRetry) {
  struct Case {
    uint16_t status_register;
    uint8_t fault;
    canbus::CanEventFlags events;
  };
  for (const auto &test : {Case{REG_RX_FIFO_STA, FIFO_RX_OVERFLOW, canbus::CAN_EVENT_RX_QUEUE_FULL},
                           Case{REG_CRC + 2, CRC_CRC_ERROR, canbus::CanEventFlags{}}}) {
    SCOPED_TRACE(test.status_register);
    RegisterDelegate spi;
    TestMCP251XFD controller(&spi);
    spi.registers[test.status_register] = test.fault;
    spi.corrupt_next_reads(test.status_register);

    EXPECT_EQ(controller.poll(100), test.events);
    EXPECT_EQ(spi.registers[test.status_register], test.fault);
    EXPECT_EQ(spi.writes_to(test.status_register), 0u);
    EXPECT_TRUE(controller.status_has_warning());

    EXPECT_EQ(controller.poll(200), test.events);
    EXPECT_EQ(spi.registers[test.status_register], 0);
    EXPECT_EQ(spi.writes_to(test.status_register), 1u);
    EXPECT_FALSE(controller.status_has_warning());
  }
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

TEST(MCP251XFDRecovery, RetriesModeRequestAfterIntervalAndStopsWhenRecovered) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;

  EXPECT_EQ(controller.poll(100), 0);
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_CON + 3], static_cast<uint8_t>(CanMode::CAN_MODE_NORMAL));
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_TRUE(controller.mode_fault_active());

  controller.poll(5099);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);

  spi.set_mode(CanMode::CAN_MODE_NORMAL);
  controller.poll(5200);
  EXPECT_FALSE(controller.mode_fault_active());
  const auto mode_reads = spi.reads[REG_CON + 2];
  controller.poll(10300);
  EXPECT_EQ(spi.reads[REG_CON + 2], mode_reads);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);
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
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
}

TEST(MCP251XFDRecovery, FailedModeReadLeavesFaultFlagsPending) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_SYSTEM_ERROR;
  spi.corrupt_next_reads(REG_CON + 2);

  controller.poll(100);
  EXPECT_EQ(spi.reads[REG_CON + 2], 3u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, INT_MODE_CHANGE);
  EXPECT_EQ(spi.registers[REG_INT + 1] & INT_SYSTEM_ERROR, INT_SYSTEM_ERROR);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);
  EXPECT_TRUE(controller.status_has_warning());

  controller.poll(200);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_EQ(spi.registers[REG_INT + 1] & INT_SYSTEM_ERROR, 0);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDRecovery, FailedErrorStateReadDefersModeRecoveryAndFlagClearing) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTENONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_SYSTEM_ERROR | INT_CAN_ERROR;
  spi.corrupt_next_reads(REG_TREC);

  controller.poll(100);
  EXPECT_EQ(spi.reads[REG_CON + 2], 0u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, INT_MODE_CHANGE);
  EXPECT_EQ(spi.registers[REG_INT + 1] & (INT_SYSTEM_ERROR | INT_CAN_ERROR), INT_SYSTEM_ERROR | INT_CAN_ERROR);
  EXPECT_TRUE(controller.status_has_warning());

  controller.poll(200);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_EQ(spi.registers[REG_INT + 1] & (INT_SYSTEM_ERROR | INT_CAN_ERROR), 0);
  EXPECT_FALSE(controller.status_has_warning());
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
