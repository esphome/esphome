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
}

}  // namespace esphome::mcp251xfd::testing
