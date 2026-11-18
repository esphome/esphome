// The rest of the test binary builds without CAN FD. As in canbus/fd_test.cpp, this file builds a CAN FD copy of
// canbus and the driver in their own namespaces, so the long payload path is tested too.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#define USE_CANBUS_FD
#define canbus mcp251xfd_canbus_fd                     // NOLINT(cppcoreguidelines-macro-usage)
#define mcp251xfd mcp251xfd_fd                         // NOLINT(cppcoreguidelines-macro-usage)
#include "esphome/components/canbus/canbus.cpp"        // NOLINT(bugprone-suspicious-include)
#include "esphome/components/mcp251xfd/mcp251xfd.cpp"  // NOLINT(bugprone-suspicious-include)
#include "register_delegate.h"

namespace esphome::mcp251xfd::testing {

TEST(MCP251XFDReceive, ReadsLongCanFdPayloadInSecondTransaction) {
  constexpr uint32_t offset = TX_FIFO_DEPTH * MESSAGE_SIZE;
  constexpr uint16_t address = RAM_START + offset;
  constexpr uint16_t rest_address = address + MESSAGE_HEADER_SIZE + 8;
  struct Case {
    uint8_t dlc;
    uint8_t length;
    std::vector<Read> expected_reads;
  };
  const std::array<Case, 4> cases{{
      {8, 8, {{address, 16}}},
      {9, 12, {{address, 16}, {rest_address, 4}}},
      {13, 32, {{address, 16}, {rest_address, 24}}},
      {15, 64, {{address, 16}, {rest_address, 56}}},
  }};
  for (const auto &test : cases) {
    SCOPED_TRACE(test.dlc);
    RegisterDelegate spi;
    TestMCP251XFD controller(&spi);
    spi.queue_rx_message(offset, MESSAGE_FDF | test.dlc, test.length);

    canbus::CanFrame frame{};
    ASSERT_EQ(controller.read_message(&frame), canbus::ERROR_OK);
    EXPECT_EQ(spi.ram_reads, test.expected_reads);
    EXPECT_TRUE(frame.fd);
    ASSERT_EQ(frame.payload_length(), test.length);
    for (size_t i = 0; i < test.length; i++)
      EXPECT_EQ(frame.data[i], i + 1);
    ASSERT_EQ(spi.writes_to(REG_RX_FIFO_CON + 1), 1u);
  }
}

TEST(MCP251XFDReceive, FailedSecondReadLeavesMessageInFifo) {
  constexpr uint32_t offset = TX_FIFO_DEPTH * MESSAGE_SIZE;
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.queue_rx_message(offset, MESSAGE_FDF | 15, 64);
  spi.corrupt_next_reads(RAM_START + offset + MESSAGE_HEADER_SIZE + 8);

  canbus::CanFrame frame{};
  EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_FAIL);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_CON + 1), 0u);
  EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_OK);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_CON + 1), 1u);
}

TEST(MCP251XFDReceive, ClassicModeUsesEightBytePayloadObjects) {
  constexpr uint32_t rx_start = TX_FIFO_DEPTH * CLASSIC_MESSAGE_SIZE;
  constexpr uint32_t last_slot = rx_start + (CLASSIC_RX_FIFO_DEPTH - 1) * CLASSIC_MESSAGE_SIZE;
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi, CanMode::CAN_MODE_CLASSIC);
  spi.queue_rx_message(last_slot, 8, 8);

  canbus::CanFrame frame{};
  ASSERT_EQ(controller.read_message(&frame), canbus::ERROR_OK);
  EXPECT_EQ(spi.ram_reads, (std::vector<Read>{{RAM_START + last_slot, 16}}));
  EXPECT_EQ(frame.payload_length(), 8);
  EXPECT_EQ(frame.data[7], 8);

  // Past the last RX object, or not on an object boundary
  for (const uint32_t offset : std::array<uint32_t, 2>{last_slot + CLASSIC_MESSAGE_SIZE, rx_start + 8}) {
    spi.queue_rx_message(offset, 8, 8);
    EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_FAIL);
  }

  // A CAN FD frame is dropped rather than left at the FIFO head
  spi.queue_rx_message(rx_start, MESSAGE_FDF | 8, 8);
  const size_t increments = spi.writes_to(REG_RX_FIFO_CON + 1);
  EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_FAIL);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_CON + 1), increments + 1);
}

TEST(MCP251XFDReceive, RemoteRequestZeroesPayloadAndKeepsDlc) {
  constexpr uint32_t offset = TX_FIFO_DEPTH * MESSAGE_SIZE;
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.queue_rx_message(offset, 8, 8);
  canbus::CanFrame frame{};
  ASSERT_EQ(controller.read_message(&frame), canbus::ERROR_OK);
  ASSERT_EQ(frame.data[0], 1);

  // The RTR object still holds bytes in RAM, which must not reach the frame either
  spi.queue_rx_message(offset, MESSAGE_RTR | 6, 8);
  ASSERT_EQ(controller.read_message(&frame), canbus::ERROR_OK);
  EXPECT_TRUE(frame.remote_transmission_request);
  EXPECT_EQ(frame.can_data_length_code, 6);
  for (size_t i = 0; i < 6; i++)
    EXPECT_EQ(frame.data[i], 0);
}

TEST(MCP251XFDTransmit, WritesHeaderAndZeroPaddedPayloadInOneTransfer) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_TX_FIFO_STA] = FIFO_NOT_FULL_NOT_EMPTY;

  canbus::CanFrame frame{};
  frame.can_id = 0x12345678 & canbus::CAN_EXTENDED_ID_MAX;
  frame.use_extended_id = true;
  frame.fd = true;
  frame.bit_rate_switch = true;
  frame.can_data_length_code = 9;
  for (size_t i = 0; i < 12; i++)
    frame.data[i] = 0xA0 + i;
  ASSERT_EQ(controller.send_message(&frame), canbus::ERROR_OK);
  ASSERT_EQ(spi.writes.size(), 2u);
  EXPECT_EQ(spi.writes[0].address, RAM_START);
  std::vector<uint8_t> expected(MESSAGE_HEADER_SIZE);
  put_le32(expected.data(), (frame.can_id >> 18) | ((frame.can_id & EXTENDED_ID_LOW_MASK) << 11));
  expected[4] = 9 | MESSAGE_IDE | MESSAGE_FDF | MESSAGE_BRS;
  for (size_t i = 0; i < 12; i++)
    expected.push_back(0xA0 + i);
  EXPECT_EQ(spi.writes[0].data, expected);
  EXPECT_EQ(spi.writes[1].address, REG_TX_FIFO_CON + 1);

  // A shorter frame pads its last word with zeros, not with bytes left from the previous frame
  frame = canbus::CanFrame{};
  frame.can_id = 0x123;
  frame.can_data_length_code = 1;
  frame.data[0] = 0x55;
  ASSERT_EQ(controller.send_message(&frame), canbus::ERROR_OK);
  EXPECT_EQ(spi.writes[2].data, (std::vector<uint8_t>{0x23, 0x01, 0, 0, 1, 0, 0, 0, 0x55, 0, 0, 0}));
}

TEST(MCP251XFDInterruptPin, EnablesReceiveInterruptAndSkipsReadsWhilePinIsHigh) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  InternalGPIOPin pin;
  controller.set_interrupt_pin(&pin);
  spi.reset();
  controller.poll(100);  // reconfigures the reset chip
  EXPECT_EQ(spi.registers[REG_INT + 2], INT_RX_ENABLE);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_CON] & FIFO_RX_NOT_EMPTY_ENABLE, FIFO_RX_NOT_EMPTY_ENABLE);

  // The host pin reads as its inverted flag; INT is active low
  canbus::CanFrame frame{};
  pin.set_inverted(true);
  EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_NOMSG);
  EXPECT_EQ(spi.reads[REG_RX_FIFO_STA], 0u);
  pin.set_inverted(false);
  EXPECT_EQ(controller.read_message(&frame), canbus::ERROR_NOMSG);
  EXPECT_EQ(spi.reads[REG_RX_FIFO_STA], 1u);
}

}  // namespace esphome::mcp251xfd::testing
