#include "esphome/components/uart/uart_virtual.h"

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome::uart::testing {

// The base leaves writing to the class that derives from it; this one records each write.
class TestVirtualUART : public VirtualUARTComponent {
 public:
  using VirtualUARTComponent::VirtualUARTComponent;

  void write_array(const uint8_t *data, size_t len) override { this->writes.emplace_back(data, data + len); }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  std::vector<std::vector<uint8_t>> writes;
};

static constexpr uint8_t FRAME[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

TEST(VirtualUART, InjectedBytesAreReadInOrder) {
  TestVirtualUART uart(16);
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
  TestVirtualUART uart(16);
  uint8_t byte = 0;
  EXPECT_FALSE(uart.peek_byte(&byte));
  uart.inject_rx(FRAME, 2);
  uint8_t out[4]{};
  EXPECT_FALSE(uart.read_array(out, 3));
  EXPECT_EQ(uart.available(), 2u);
}

TEST(VirtualUART, ReadOfNothingSucceeds) {
  TestVirtualUART uart(16);
  uint8_t byte = 0xAA;
  EXPECT_TRUE(uart.read_array(&byte, 0));
  EXPECT_EQ(byte, 0xAA);
}

TEST(VirtualUART, BlockThatDoesNotFitIsRefusedWhole) {
  TestVirtualUART uart(10);
  EXPECT_TRUE(uart.inject_rx(FRAME, sizeof(FRAME)));
  EXPECT_FALSE(uart.inject_rx(FRAME, 3));
  EXPECT_EQ(uart.available(), sizeof(FRAME));
  TestVirtualUART no_ring(0);
  EXPECT_FALSE(no_ring.inject_rx(FRAME, 1));
  EXPECT_EQ(no_ring.available(), 0u);
}

TEST(VirtualUART, RingWrapsAround) {
  TestVirtualUART uart(10);
  uint8_t out[sizeof(FRAME)]{};
  for (int round = 0; round < 5; round++) {
    ASSERT_TRUE(uart.inject_rx(FRAME, sizeof(FRAME)));
    ASSERT_TRUE(uart.read_array(out, sizeof(FRAME)));
    EXPECT_EQ(out[sizeof(FRAME) - 1], 0xCD);
  }
}

}  // namespace esphome::uart::testing

#endif  // USE_HOST
