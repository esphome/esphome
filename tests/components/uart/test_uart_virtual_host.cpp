#include "esphome/components/uart/uart_virtual.h"

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace esphome::uart::testing {

// The base leaves writing to the class that derives from it; this one records each write.
template<size_t N> class TestVirtualUART : public VirtualUARTComponent<N> {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->writes.emplace_back(data, data + len);
    this->debug_tx_(data, len);
  }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  std::vector<std::vector<uint8_t>> writes;
};

static constexpr uint8_t FRAME[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

TEST(VirtualUART, InjectedBytesAreReadInOrder) {
  TestVirtualUART<16> uart;
  EXPECT_EQ(uart.get_rx_buffer_size(), 16u);
  EXPECT_TRUE(uart.inject_rx(FRAME, sizeof(FRAME)));
  EXPECT_EQ(uart.available(), sizeof(FRAME));
  uint8_t peeked = 0;
  EXPECT_TRUE(uart.peek_byte(&peeked));
  EXPECT_EQ(peeked, 0x01);
  uint8_t out[sizeof(FRAME)]{};
  EXPECT_TRUE(uart.read_array(out, 3));
  EXPECT_TRUE(uart.read_array(out + 3, sizeof(FRAME) - 3));
  EXPECT_EQ(std::vector<uint8_t>(out, out + sizeof(out)), std::vector<uint8_t>(FRAME, FRAME + sizeof(FRAME)));
  EXPECT_EQ(uart.available(), 0u);
}

TEST(VirtualUART, ShortReadAndEmptyPeekFailWithoutConsuming) {
  TestVirtualUART<16> uart;
  uint8_t byte = 0;
  EXPECT_FALSE(uart.peek_byte(&byte));
  uart.inject_rx(FRAME, 2);
  uint8_t out[4]{};
  EXPECT_FALSE(uart.read_array(out, 3));
  EXPECT_EQ(uart.available(), 2u);
}

TEST(VirtualUART, BlockThatDoesNotFitIsRefusedWhole) {
  TestVirtualUART<10> uart;
  EXPECT_TRUE(uart.inject_rx(FRAME, sizeof(FRAME)));
  EXPECT_FALSE(uart.inject_rx(FRAME, 3));
  EXPECT_EQ(uart.available(), sizeof(FRAME));
}

TEST(VirtualUART, DebuggerSeesReadAndWrittenBytes) {
  TestVirtualUART<16> uart;
  std::vector<std::pair<UARTDirection, uint8_t>> seen;
  uart.add_debug_callback([&seen](UARTDirection dir, uint8_t b) { seen.emplace_back(dir, b); });
  ASSERT_TRUE(uart.inject_rx(FRAME, 2));
  uint8_t out[2];
  ASSERT_TRUE(uart.read_array(out, 2));
  uart.write_array(FRAME, 1);
  ASSERT_EQ(seen.size(), 3u);
  EXPECT_EQ(seen[0], std::make_pair(UART_DIRECTION_RX, FRAME[0]));
  EXPECT_EQ(seen[1], std::make_pair(UART_DIRECTION_RX, FRAME[1]));
  EXPECT_EQ(seen[2], std::make_pair(UART_DIRECTION_TX, FRAME[0]));
}

}  // namespace esphome::uart::testing

#endif  // USE_HOST
