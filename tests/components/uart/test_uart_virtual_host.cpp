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

// Records each block it is handed, so a test can see the boundaries.
class BlockRecorder : public UARTSink {
 public:
  void on_block(const uint8_t *data, size_t len) override { this->blocks.emplace_back(data, data + len); }

  std::vector<std::vector<uint8_t>> blocks;
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

TEST(VirtualUART, AttachedReaderGetsBlocksAndTheRingStaysEmpty) {
  TestVirtualUART uart(16);
  BlockRecorder reader;
  uart.set_rx_sink(&reader);
  EXPECT_TRUE(uart.inject_rx(FRAME, sizeof(FRAME)));
  EXPECT_TRUE(uart.inject_rx(FRAME, 3));
  ASSERT_EQ(reader.blocks.size(), 2u);
  EXPECT_EQ(reader.blocks[0].size(), sizeof(FRAME));
  EXPECT_EQ(reader.blocks[1].size(), 3u);
  EXPECT_EQ(uart.available(), 0u);
}

// Two ends wired to each other whose readers write every block back: each write lands in the peer's inject_rx().
class LoopbackUART : public TestVirtualUART {
 public:
  LoopbackUART() : TestVirtualUART(0) {}
  void write_array(const uint8_t *data, size_t len) override {
    this->writes.emplace_back(data, data + len);
    this->peer->inject_rx(data, len);
  }
  LoopbackUART *peer{nullptr};
};

class EchoReader : public UARTSink {
 public:
  explicit EchoReader(LoopbackUART *own) : own_(own) {}
  void on_block(const uint8_t *data, size_t len) override {
    this->calls++;
    this->own_->write_array(data, len);
  }
  int calls{0};

 protected:
  LoopbackUART *own_;
};

TEST(VirtualUART, ReaderThatWritesBackIsNotHandedABlockAgain) {
  LoopbackUART x;
  LoopbackUART y;
  x.peer = &y;
  y.peer = &x;
  EchoReader x_reader(&x);
  EchoReader y_reader(&y);
  x.set_rx_sink(&x_reader);
  y.set_rx_sink(&y_reader);
  // x's reader echoes to y, y's reader echoes back to x, whose reader is still inside on_block(): refused there.
  EXPECT_TRUE(x.inject_rx(FRAME, sizeof(FRAME)));
  EXPECT_EQ(x_reader.calls, 1);
  EXPECT_EQ(y_reader.calls, 1);
  ASSERT_EQ(y.writes.size(), 1u);
  // Once on_block() has returned, the next block is handed over again.
  EXPECT_TRUE(x.inject_rx(FRAME, 3));
  EXPECT_EQ(x_reader.calls, 2);
}

}  // namespace esphome::uart::testing

#endif  // USE_HOST
