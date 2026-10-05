#include "esphome/components/uart/bridge/uart_bridge.h"

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace esphome::uart::testing {

// A UART with a wire: bytes wait until read, every write is recorded as one block with the time it was made.
class WireUart : public UARTComponent {
 public:
  explicit WireUart(uint32_t baud) { this->set_baud_rate(baud); }

  void write_array(const uint8_t *data, size_t len) override {
    this->writes.emplace_back(data, data + len);
    this->write_us.push_back(this->clock_us);
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty()) {
      return false;
    }
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return this->room; }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  bool is_connected() override { return this->connected; }
  void set_rx_full_threshold(size_t threshold) override { this->rx_full_threshold_ = threshold; }
  void set_rx_timeout(size_t timeout) override { this->rx_timeout_ = timeout; }

  void receive(size_t len, uint8_t first = 0) {
    for (size_t i = 0; i < len; i++) {
      this->rx.push_back(static_cast<uint8_t>(first + i));
    }
  }
  std::vector<uint8_t> joined() const {
    std::vector<uint8_t> out;
    for (const auto &block : this->writes) {
      out.insert(out.end(), block.begin(), block.end());
    }
    return out;
  }

  std::vector<uint8_t> rx;
  std::vector<std::vector<uint8_t>> writes;
  std::vector<uint32_t> write_us;
  size_t room{SIZE_MAX};
  uint32_t clock_us{0};
  bool connected{true};

 protected:
  void check_logger_conflict() override {}
};

// A virtual end that records each write as one block.
class VirtualEnd : public VirtualUARTComponent {
 public:
  VirtualEnd() : VirtualUARTComponent(0) {}

  void write_array(const uint8_t *data, size_t len) override { this->writes.emplace_back(data, data + len); }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  std::vector<std::vector<uint8_t>> writes;
};

// Reads how many requests for full-speed loop passes are open.
struct FastLoopRequests : public HighFrequencyLoopRequester {
  static uint8_t count() { return num_requests; }
};

static std::vector<uint8_t> frame(size_t len, uint8_t first = 0) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i < len; i++) {
    out.push_back(static_cast<uint8_t>(first + i));
  }
  return out;
}

// 9600 baud 8N1: one character is 1042 us (rounded up), 3.5 characters 3647 us; 8 characters are on the wire for
// 8334 us.
static constexpr uint32_t GAP_9600_US = 3647;
static constexpr uint32_t WIRE_8_AT_9600_US = 8334;
static constexpr uint32_t MIN_GAP_US = 1750;
static constexpr uint32_t CHUNKED_GAP_US = 50000;

TEST(UARTBridge, BlockLeavesOnlyAfterTheGap) {
  WireUart a(9600);
  WireUart b(9600);
  b.room = 128;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  EXPECT_EQ(pipe.frame_gap_us(), GAP_9600_US);
  a.receive(8);
  EXPECT_TRUE(pipe.poll(1000));
  EXPECT_TRUE(pipe.poll(1000 + GAP_9600_US - 1));
  EXPECT_TRUE(b.writes.empty());
  EXPECT_FALSE(pipe.poll(1000 + GAP_9600_US));
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(8));
}

TEST(UARTBridge, IdlePassOnlyAsksForBytes) {
  WireUart a(9600);
  WireUart b(9600);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  EXPECT_FALSE(pipe.poll());
  a.receive(8);
  EXPECT_TRUE(pipe.poll());
  EXPECT_EQ(a.available(), 0u);
  EXPECT_TRUE(b.writes.empty());
}

TEST(UARTBridge, PiecesInsideTheGapFormOneBlock) {
  WireUart a(9600);
  WireUart b(9600);
  b.room = 128;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  a.receive(5);
  pipe.poll(0);
  a.receive(3, 5);
  pipe.poll(3000);
  pipe.poll(3000 + GAP_9600_US - 1);
  EXPECT_TRUE(b.writes.empty());
  pipe.poll(3000 + GAP_9600_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(8));
}

TEST(UARTBridge, GapIsAtLeast1750usAbove19200Baud) {
  // At 115200 a pause of 500 us inside a frame is allowed (1.5 characters are 750 us there).
  WireUart a(115200);
  VirtualEnd b;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_virtual();
  pipe.setup();
  EXPECT_EQ(pipe.frame_gap_us(), MIN_GAP_US);
  a.receive(50);
  pipe.poll(0);
  pipe.poll(500);
  a.receive(159, 50);
  pipe.poll(500);
  pipe.poll(500 + MIN_GAP_US - 1);
  EXPECT_TRUE(b.writes.empty());
  pipe.poll(500 + MIN_GAP_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(209));
}

TEST(UARTBridge, FullBlockLeavesWithoutWaiting) {
  WireUart a(9600);
  WireUart b(9600);
  b.room = 1024;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  a.receive(300);
  pipe.poll(0);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0].size(), UARTBridgePipe::BLOCK_SIZE);
  pipe.poll(1);
  pipe.poll(1 + GAP_9600_US);
  ASSERT_EQ(b.writes.size(), 2u);
  EXPECT_EQ(b.writes[1], frame(300 - UARTBridgePipe::BLOCK_SIZE, UARTBridgePipe::BLOCK_SIZE));
}

TEST(UARTBridge, FullDriverBatchWaitsForTheRestOfTheFrame) {
  // ESP32 at 9600 with rx_full_threshold 8 and rx_timeout 2: the driver hands over 9 bytes once more than 8 are in.
  // The last 8 bytes of a 17-byte reply show up after 8 characters plus the rx timeout (about 2.4 characters on the
  // ESP32) and some interrupt latency: 10937 us later.
  WireUart a(9600);
  VirtualEnd b;
  a.set_rx_full_threshold(8);
  a.set_rx_timeout(2);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_virtual();
  pipe.setup();
  a.receive(9);
  uint32_t t = 0;
  for (; t < 10937; t += 100) {
    pipe.poll(t);
  }
  EXPECT_TRUE(b.writes.empty());
  a.receive(8, 9);
  while (pipe.poll(t)) {
    t += 100;
  }
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(17));
}

TEST(UARTBridge, SourceThatIsNotAHardwareLineWaitsLikeTheModbusHub) {
  // tcp_uart, USB: bytes come in chunks, so 20 ms between two parts of one frame is normal.
  WireUart a(9600);
  VirtualEnd b;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_to_virtual();
  pipe.setup();
  EXPECT_EQ(pipe.frame_gap_us(), CHUNKED_GAP_US);
  a.receive(10);
  pipe.poll(0);
  pipe.poll(20000);
  a.receive(10, 10);
  pipe.poll(20000);
  pipe.poll(20000 + CHUNKED_GAP_US - 1);
  EXPECT_TRUE(b.writes.empty());
  pipe.poll(20000 + CHUNKED_GAP_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(20));
}

TEST(UARTBridge, SourceWithoutBaudRateUsesTheChunkGap) {
  // ble_nus, or usb_cdc_acm before the host sets the line.
  WireUart a(0);
  WireUart b(9600);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  EXPECT_EQ(pipe.frame_gap_us(), CHUNKED_GAP_US);
  a.receive(10);
  pipe.poll(0);
  pipe.poll(CHUNKED_GAP_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(10));
}

TEST(UARTBridge, DestinationWithoutBaudRateGetsTheWholeBlock) {
  WireUart a(9600);
  WireUart b(0);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  a.receive(20);
  pipe.poll(0);
  pipe.poll(GAP_9600_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(20));
  // Nothing to time on it, so the next block does not wait.
  a.receive(4, 20);
  pipe.poll(GAP_9600_US + 1);
  pipe.poll(2 * GAP_9600_US + 1);
  EXPECT_EQ(b.writes.size(), 2u);
}

TEST(UARTBridge, UnknownRoomTakesTheWholeBlockInOneWrite) {
  WireUart a(115200);
  WireUart b(115200);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  a.receive(200);
  pipe.poll(0);
  EXPECT_FALSE(pipe.poll(MIN_GAP_US));
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(200));
}

TEST(UARTBridge, SmallRoomFeedsTheRestOnLaterPasses) {
  WireUart a(9600);
  WireUart b(9600);
  b.room = 100;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  a.receive(200);
  pipe.poll(0);
  pipe.poll(GAP_9600_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0].size(), 100u);
  // The next frame stays in the source until this one is out.
  a.receive(4, 0xF0);
  b.room = 0;
  EXPECT_TRUE(pipe.poll(GAP_9600_US + 1));
  EXPECT_EQ(a.available(), 4u);
  b.room = 100;
  pipe.poll(GAP_9600_US + 2);
  EXPECT_EQ(b.joined(), frame(200));
  EXPECT_EQ(a.available(), 0u);
}

TEST(UARTBridge, HardwareDestinationKeepsAFrameGapBetweenBlocks) {
  // 115200 in, 9600 out with a 128-byte FIFO: the second frame waits until the first has been on the 9600 wire for
  // 8 characters, plus 3.5 characters of silence.
  WireUart a(115200);
  WireUart b(9600);
  b.room = 128;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  uint32_t t = 0;
  for (int f = 0; f < 2; f++) {
    a.receive(8, static_cast<uint8_t>(f * 8));
    for (int k = 0; k < 30; k++) {
      t += 100;
      b.clock_us = t;
      pipe.poll(t);
    }
  }
  while (pipe.poll(t)) {
    t += 100;
    b.clock_us = t;
  }
  ASSERT_EQ(b.writes.size(), 2u);
  EXPECT_GE(b.write_us[1] - b.write_us[0], WIRE_8_AT_9600_US + GAP_9600_US);
  EXPECT_EQ(b.joined(), frame(16));
}

TEST(UARTBridge, StreamCutIntoBlocksKeepsUpWithTheLine) {
  // 768 bytes at the line rate of 115200 baud: blocks cut at 256 bytes follow each other without a gap, so the
  // destination at the same rate never falls behind.
  WireUart a(115200);
  WireUart b(115200);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  // 256 characters of 10 bits at 115200 baud.
  const uint32_t block_us = 22223;
  uint32_t t = 1000000;
  for (int k = 0; k < 3; k++) {
    a.receive(UARTBridgePipe::BLOCK_SIZE, static_cast<uint8_t>(k));
    b.clock_us = t;
    pipe.poll(t);
    ASSERT_EQ(b.writes.size(), static_cast<size_t>(k + 1));
    EXPECT_EQ(b.write_us[k], t);
    t += block_us;
  }
}

TEST(UARTBridge, LongWaitForTheLineUsesNormalPasses) {
  const uint8_t before = FastLoopRequests::count();
  VirtualEnd a;
  WireUart b(9600);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto first = frame(200);
  const auto second = frame(8, 0x80);
  const uint32_t start = micros();
  a.inject_rx(first.data(), first.size());
  a.inject_rx(second.data(), second.size());
  // 200 characters are on the wire for 208 ms: the second block waits, at normal loop passes until 20 ms are left.
  EXPECT_TRUE(pipe.poll(start + 1000));
  EXPECT_EQ(FastLoopRequests::count(), before);
  EXPECT_TRUE(pipe.poll(start + 208334 + GAP_9600_US - 15000));
  EXPECT_EQ(FastLoopRequests::count(), before + 1);
  EXPECT_FALSE(pipe.poll(start + 208334 + GAP_9600_US + 1000));
  EXPECT_EQ(FastLoopRequests::count(), before);
  ASSERT_EQ(b.writes.size(), 2u);
}

TEST(UARTBridge, VirtualSourcePushesTheWholeBlockAtOnce) {
  VirtualEnd a;
  WireUart b(9600);
  b.room = 128;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto request = frame(8);
  a.inject_rx(request.data(), request.size());
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], request);
  EXPECT_EQ(pipe.frame_gap_us(), 0u);
}

TEST(UARTBridge, SecondPushIsItsOwnBlockAfterTheGap) {
  // A broadcast and the next request pushed in one call keep 3.5 characters of silence between them.
  VirtualEnd a;
  WireUart b(9600);
  b.room = 128;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto broadcast = frame(13);
  const auto request = frame(8, 0x40);
  const uint32_t start = micros();
  a.inject_rx(broadcast.data(), broadcast.size());
  a.inject_rx(request.data(), request.size());
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], broadcast);
  // 13 characters are on the wire for 13542 us.
  const uint32_t hold = 13542 + GAP_9600_US;
  EXPECT_TRUE(pipe.poll(start + hold - 1000));
  EXPECT_EQ(b.writes.size(), 1u);
  EXPECT_FALSE(pipe.poll(start + hold + 1000));
  ASSERT_EQ(b.writes.size(), 2u);
  EXPECT_EQ(b.writes[1], request);
}

TEST(UARTBridge, PushThatDoesNotFitIsDroppedWhole) {
  VirtualEnd a;
  WireUart b(9600);
  b.room = 10;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.setup();
  const auto first = frame(200);
  const auto second = frame(100, 0x80);
  const auto third = frame(50, 0xC0);
  a.inject_rx(first.data(), first.size());
  a.inject_rx(second.data(), second.size());
  a.inject_rx(third.data(), third.size());
  // A fourth one while the third waits behind the first.
  a.inject_rx(second.data(), 1);
  b.room = SIZE_MAX;
  for (uint32_t t = 0; pipe.poll(t); t++) {
  }
  std::vector<uint8_t> want(first);
  want.insert(want.end(), third.begin(), third.end());
  EXPECT_EQ(b.joined(), want);
}

TEST(UARTBridge, PushThatFinishesThePendingBlockReleasesTheFastLoop) {
  const uint8_t before = FastLoopRequests::count();
  VirtualEnd a;
  WireUart b(9600);
  b.room = 4;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.setup();
  const auto first = frame(10);
  a.inject_rx(first.data(), first.size());
  EXPECT_EQ(FastLoopRequests::count(), before + 1);
  b.room = 1000;
  const auto second = frame(5, 10);
  a.inject_rx(second.data(), second.size());
  for (int i = 0; i < 5; i++) {
    pipe.poll();
  }
  EXPECT_EQ(b.joined(), frame(15));
  EXPECT_EQ(FastLoopRequests::count(), before);
}

TEST(UARTBridge, DestinationNotConnectedDropsAndReleasesTheLoop) {
  // tcp_uart in server role while no client is connected.
  const uint8_t before = FastLoopRequests::count();
  WireUart a(9600);
  WireUart b(9600);
  b.connected = false;
  b.room = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  a.receive(8);
  for (uint32_t t = 0; t < 100000; t += 1000) {
    pipe.poll(t);
  }
  EXPECT_EQ(a.available(), 0u);
  EXPECT_TRUE(b.writes.empty());
  EXPECT_EQ(FastLoopRequests::count(), before);
  // The next client gets only what arrives after it connected.
  b.connected = true;
  b.room = SIZE_MAX;
  a.receive(4, 0x40);
  pipe.poll(100000);
  pipe.poll(100000 + GAP_9600_US);
  EXPECT_EQ(b.joined(), frame(4, 0x40));
}

TEST(UARTBridge, PushForADestinationThatIsNotConnectedIsDropped) {
  const uint8_t before = FastLoopRequests::count();
  VirtualEnd a;
  WireUart b(9600);
  b.connected = false;
  b.room = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.setup();
  const auto request = frame(8);
  a.inject_rx(request.data(), request.size());
  EXPECT_FALSE(pipe.poll(0));
  EXPECT_EQ(FastLoopRequests::count(), before);
  b.connected = true;
  b.room = SIZE_MAX;
  EXPECT_FALSE(pipe.poll(1));
  EXPECT_TRUE(b.writes.empty());
}

TEST(UARTBridge, DestinationThatTakesNothingIsDroppedAfterASecond) {
  // A connected destination that never has room, e.g. an output that only reads, or a UART whose driver failed.
  const uint8_t before = FastLoopRequests::count();
  WireUart a(9600);
  WireUart b(9600);
  b.room = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  a.receive(8);
  pipe.poll(0);
  // A FIFO that is full for a moment keeps the fast passes; after 20 ms without room they stop.
  EXPECT_TRUE(pipe.poll(CHUNKED_GAP_US));
  EXPECT_EQ(FastLoopRequests::count(), before + 1);
  EXPECT_TRUE(pipe.poll(CHUNKED_GAP_US + 20000));
  EXPECT_EQ(FastLoopRequests::count(), before);
  a.receive(4, 0x40);
  EXPECT_TRUE(pipe.poll(CHUNKED_GAP_US + 999999));
  EXPECT_EQ(a.available(), 4u);
  EXPECT_FALSE(pipe.poll(CHUNKED_GAP_US + 1000000));
  // The source is read again; the dropped block never goes out.
  b.room = SIZE_MAX;
  pipe.poll(2000000);
  pipe.poll(2000000 + CHUNKED_GAP_US);
  EXPECT_EQ(b.joined(), frame(4, 0x40));
}

// A destination whose write blocks, as one that cannot report its room does; the next frame starts arriving
// meanwhile.
class BlockingWire : public WireUart {
 public:
  using WireUart::WireUart;

  void write_array(const uint8_t *data, size_t len) override {
    WireUart::write_array(data, len);
    if (this->source != nullptr && data[len - 1] == this->last_byte) {
      this->source->receive(3, 0x80);
      this->source = nullptr;
      std::this_thread::sleep_for(std::chrono::milliseconds(6));
    }
  }

  WireUart *source{nullptr};
  uint8_t last_byte{0};
};

TEST(UARTBridge, BytesThatArriveDuringABlockingWriteAreTimedFromItsEnd) {
  WireUart a(9600);
  BlockingWire b(9600);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.setup();
  a.receive(8);
  b.source = &a;
  b.last_byte = 7;
  pipe.poll();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  for (int i = 0; i < 20 && b.source != nullptr; i++) {
    pipe.poll();
  }
  ASSERT_EQ(b.source, nullptr);
  pipe.poll();
  pipe.poll();
  // The three bytes arrived up to 6 ms ago, but were seen only after the write: no block yet.
  ASSERT_EQ(b.joined(), frame(8));
  a.receive(5, 0x83);
  pipe.poll();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  pipe.poll();
  EXPECT_EQ(b.writes.back(), frame(8, 0x80));
}

TEST(UARTBridge, PolledSourceGivesAVirtualDestinationTheWholeBlock) {
  WireUart a(9600);
  VirtualEnd b;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_virtual();
  pipe.setup();
  a.receive(209);
  pipe.poll(0);
  pipe.poll(GAP_9600_US);
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], frame(209));
}

TEST(UARTBridge, TwoVirtualEndsNeedNoLoop) {
  VirtualEnd a;
  VirtualEnd b;
  UARTBridge bridge(&a, &b);
  bridge.set_virtual_a(&a);
  bridge.set_virtual_b(&b);
  bridge.setup();
  EXPECT_TRUE(bridge.is_idle());
  const auto request = frame(8);
  a.inject_rx(request.data(), request.size());
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], request);
  const auto reply = frame(255, 0x40);
  b.inject_rx(reply.data(), reply.size());
  ASSERT_EQ(a.writes.size(), 1u);
  EXPECT_EQ(a.writes[0], reply);
}

TEST(UARTBridge, HardwareEndKeepsTheLoop) {
  WireUart a(9600);
  VirtualEnd b;
  UARTBridge bridge(&a, &b);
  bridge.set_wire_a();
  bridge.set_virtual_b(&b);
  bridge.setup();
  EXPECT_FALSE(bridge.is_idle());
}

}  // namespace esphome::uart::testing

#endif  // USE_HOST
