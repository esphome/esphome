#include "esphome/components/uart/bridge/uart_bridge.h"

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
  out.reserve(len);
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
  const uint8_t before = FastLoopRequests::count();
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
  // After the gap the block leaves and the fast loop is released.
  EXPECT_FALSE(pipe.poll(micros() + GAP_9600_US));
  EXPECT_EQ(b.joined(), frame(8));
  EXPECT_EQ(FastLoopRequests::count(), before);
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
  EXPECT_EQ(b.writes[1].size(), 300 - UARTBridgePipe::BLOCK_SIZE);
  EXPECT_EQ(b.joined(), frame(300));
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

TEST(UARTBridge, SourceThatIsNotALineWaitsLikeTheModbusHub) {
  // tcp_uart: bytes come in chunks, so 20 ms between two parts of one frame is normal; its baud rate is not used.
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
  const uint8_t before = FastLoopRequests::count();
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
  // The next frame leaves after its own gap and the fast loop is released.
  EXPECT_FALSE(pipe.poll(GAP_9600_US + 2 + GAP_9600_US));
  EXPECT_EQ(b.writes.back(), frame(4, 0xF0));
  EXPECT_EQ(FastLoopRequests::count(), before);
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
  b.room = UARTBridgePipe::BLOCK_SIZE;
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
  // No room while the blocks are pushed, so the first goes out on a pass at a time the test sets.
  b.room = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto first = frame(200);
  const auto second = frame(8, 0x80);
  a.inject_rx(first.data(), first.size());
  a.inject_rx(second.data(), second.size());
  b.room = SIZE_MAX;
  const uint32_t start = 1000000;
  EXPECT_TRUE(pipe.poll(start));
  ASSERT_EQ(b.writes.size(), 1u);
  // 200 characters are on the wire for 208334 us: the second block waits, at normal loop passes until 20 ms are left.
  const uint32_t hold = 208334 + GAP_9600_US;
  EXPECT_EQ(FastLoopRequests::count(), before);
  EXPECT_TRUE(pipe.poll(start + hold - 20000));
  EXPECT_EQ(FastLoopRequests::count(), before);
  EXPECT_TRUE(pipe.poll(start + hold - 19999));
  EXPECT_EQ(FastLoopRequests::count(), before + 1);
  EXPECT_FALSE(pipe.poll(start + hold));
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
  // No room while they are pushed, so the first goes out on a pass at a time the test sets.
  b.room = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto broadcast = frame(13);
  const auto request = frame(8, 0x40);
  a.inject_rx(broadcast.data(), broadcast.size());
  a.inject_rx(request.data(), request.size());
  b.room = 128;
  const uint32_t start = 1000000;
  EXPECT_TRUE(pipe.poll(start));
  ASSERT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.writes[0], broadcast);
  // 13 characters are on the wire for 13542 us.
  const uint32_t hold = 13542 + GAP_9600_US;
  EXPECT_TRUE(pipe.poll(start + hold - 1));
  EXPECT_EQ(b.writes.size(), 1u);
  EXPECT_FALSE(pipe.poll(start + hold));
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
  pipe.poll(0);
  pipe.poll(GAP_9600_US);
  ASSERT_EQ(b.source, nullptr);
  // The write took 6 ms. The three bytes that arrived meanwhile are seen on the next pass and timed from there.
  const uint32_t end = GAP_9600_US + 6000;
  pipe.poll(end);
  pipe.poll(end + GAP_9600_US - 1);
  ASSERT_EQ(b.joined(), frame(8));
  a.receive(5, 0x83);
  pipe.poll(end + GAP_9600_US - 1);
  pipe.poll(end + 2 * GAP_9600_US - 1);
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

// 115200 baud 8N1: a byte is on the wire for 86.8 us, here 87 us. Modbus RTU allows the next frame after 3.5 quiet
// characters, which is 1750 us above 19200 baud. The loop passes every 100 us while the bridge has bytes. The
// destinations below take each block in one write, so their writes show where the bridge ended each frame.
static constexpr uint32_t CHAR_115200_US = 87;
static constexpr uint32_t T35_115200_US = 1750;
static constexpr uint32_t PASS_US = 100;

// When each byte of `frames` is in at the receiving UART: one per character, each frame `gap_us` after the last.
static std::vector<uint32_t> line_times(const std::vector<std::vector<uint8_t>> &frames, uint32_t start_us,
                                        uint32_t gap_us) {
  std::vector<uint32_t> at;
  uint32_t t = start_us;
  for (const auto &f : frames) {
    for (size_t i = 0; i < f.size(); i++) {
      t += CHAR_115200_US;
      at.push_back(t);
    }
    t += gap_us;
  }
  return at;
}

// As the ESP32 driver hands the bytes over: all that are in once more than `threshold` are, or 2 quiet characters
// after the last one.
static std::vector<uint32_t> in_bursts(const std::vector<uint32_t> &at, size_t threshold) {
  std::vector<uint32_t> shown(at.size());
  size_t first = 0;
  for (size_t i = 0; i < at.size(); i++) {
    const bool full = i + 1 - first > threshold;
    const bool quiet = i + 1 == at.size() || at[i + 1] - at[i] > 2 * CHAR_115200_US;
    if (full || quiet) {
      for (size_t k = first; k <= i; k++) {
        shown[k] = full ? at[i] : at[i] + 2 * CHAR_115200_US;
      }
      first = i + 1;
    }
  }
  return shown;
}

// Hands the bytes of `frames` to `a` at the times in `shown` and polls the pipe on every loop pass until all is out.
static void play(UARTBridgePipe &pipe, WireUart &a, WireUart &b, const std::vector<std::vector<uint8_t>> &frames,
                 const std::vector<uint32_t> &shown, uint32_t pass_us = PASS_US) {
  std::vector<uint8_t> bytes;
  for (const auto &f : frames) {
    bytes.insert(bytes.end(), f.begin(), f.end());
  }
  size_t next = 0;
  // 100 ms after the last byte everything is out.
  for (uint32_t now = 0; now < shown.back() + 100000; now += pass_us) {
    for (; next < bytes.size() && shown[next] <= now; next++) {
      a.rx.push_back(bytes[next]);
    }
    b.clock_us = now;
    pipe.poll(now);
  }
}

static std::vector<size_t> sizes(const std::vector<std::vector<uint8_t>> &blocks) {
  std::vector<size_t> out;
  for (const auto &block : blocks) {
    out.push_back(block.size());
  }
  return out;
}

TEST(UARTBridge, LoneFrameArrivingByteByByteLeavesWhole) {
  // A driver that hands over each byte as it comes in (ESP8266 hardware serial, RP2040): a 64-byte frame is read over
  // many passes and leaves as one block.
  const auto reply = frame(64);
  WireUart a(115200);
  WireUart b(115200);
  b.room = UARTBridgePipe::BLOCK_SIZE;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  play(pipe, a, b, {reply}, line_times({reply}, 1000, T35_115200_US));
  EXPECT_EQ(sizes(b.writes), std::vector<size_t>{64});
  EXPECT_EQ(b.joined(), reply);
}

TEST(UARTBridge, RequestsAtTheModbusGapStayApartWhenBytesComeOneByOne) {
  // Two 8-byte requests 3.5 characters apart, from a driver that hands over each byte as it comes in. Whatever the
  // offset between the line and the loop passes, and also with a busier loop that passes every 300 us, the destination
  // gets two blocks.
  const auto first = frame(8);
  const auto second = frame(8, 0x40);
  std::vector<uint8_t> both(first);
  both.insert(both.end(), second.begin(), second.end());
  for (uint32_t pass_us : {PASS_US, 3 * PASS_US}) {
    for (uint32_t offset = 0; offset < pass_us; offset += pass_us / 10) {
      SCOPED_TRACE(::testing::Message() << "pass " << pass_us << " us, offset " << offset << " us");
      WireUart a(115200);
      WireUart b(115200);
      b.room = UARTBridgePipe::BLOCK_SIZE;
      UARTBridgePipe pipe(&a, &b);
      pipe.set_from_wire();
      pipe.set_to_wire();
      pipe.setup();
      play(pipe, a, b, {first, second}, line_times({first, second}, 1000 + offset, T35_115200_US), pass_us);
      EXPECT_EQ(sizes(b.writes), (std::vector<size_t>{8, 8}));
      EXPECT_EQ(b.joined(), both);
    }
  }
}

TEST(UARTBridge, LongRepliesAtTheModbusGapStayWholeWhenBytesComeOneByOne) {
  // Two replies of 255 bytes (125 registers) 3.5 characters apart, byte by byte as above. If the first byte of the
  // second reply went out with the first, the rest of it would wait for its own gap: a pause of more than 1.5
  // characters inside the second reply on the destination line.
  const auto first = frame(255);
  const auto second = frame(255, 0x80);
  std::vector<uint8_t> both(first);
  both.insert(both.end(), second.begin(), second.end());
  for (uint32_t offset = 0; offset < PASS_US; offset += 10) {
    SCOPED_TRACE(offset);
    WireUart a(115200);
    WireUart b(115200);
    b.room = UARTBridgePipe::BLOCK_SIZE;
    UARTBridgePipe pipe(&a, &b);
    pipe.set_from_wire();
    pipe.set_to_wire();
    pipe.setup();
    play(pipe, a, b, {first, second}, line_times({first, second}, 1000 + offset, T35_115200_US));
    EXPECT_EQ(sizes(b.writes), (std::vector<size_t>{255, 255}));
    EXPECT_EQ(b.joined(), both);
  }
}

TEST(UARTBridge, FramesAtTheModbusGapStayApartWhenTheDriverHandsOverBursts) {
  // The ESP32 driver at 115200 hands over its FIFO once more than 114 bytes are in (10 ms) or 2 quiet characters
  // after the last byte, so the next frame shows up only when it is complete or 115 bytes long.
  for (size_t len : {8u, 255u}) {
    const auto first = frame(len);
    const auto second = frame(len, 0x80);
    for (uint32_t offset = 0; offset < PASS_US; offset += 10) {
      SCOPED_TRACE(offset);
      WireUart a(115200);
      WireUart b(115200);
      b.room = UARTBridgePipe::BLOCK_SIZE;
      a.set_rx_full_threshold(114);
      a.set_rx_timeout(2);
      UARTBridgePipe pipe(&a, &b);
      pipe.set_from_wire();
      pipe.set_to_wire();
      pipe.setup();
      play(pipe, a, b, {first, second}, in_bursts(line_times({first, second}, 1000 + offset, T35_115200_US), 114));
      EXPECT_EQ(sizes(b.writes), (std::vector<size_t>{len, len}));
    }
  }
}

TEST(UARTBridge, FrameThatFillsABlockStaysApartFromTheNextOne) {
  // A 256-byte frame fills one block, so its length cannot tell whether more follows. The source then stays quiet for
  // 3.5 characters, so the next 8-byte frame is a frame of its own: on the destination it waits until the first one is
  // out, 256 characters of 87 us (22223 us), and the line was quiet for 3.5 characters.
  const auto first = frame(256);
  const auto second = frame(8, 0x40);
  for (uint32_t pass_us : {PASS_US, 3 * PASS_US}) {
    for (uint32_t offset = 0; offset < pass_us; offset += pass_us / 10) {
      SCOPED_TRACE(::testing::Message() << "pass " << pass_us << " us, offset " << offset << " us");
      WireUart a(115200);
      WireUart b(115200);
      b.room = UARTBridgePipe::BLOCK_SIZE;
      UARTBridgePipe pipe(&a, &b);
      pipe.set_from_wire();
      pipe.set_to_wire();
      pipe.setup();
      play(pipe, a, b, {first, second}, line_times({first, second}, 1000 + offset, T35_115200_US), pass_us);
      ASSERT_EQ(sizes(b.writes), (std::vector<size_t>{256, 8}));
      EXPECT_GE(b.write_us[1] - b.write_us[0], 22223 + T35_115200_US);
    }
  }
}

// A line whose UART cannot report its room, as on the RP2040: a write returns once its last byte is in the 32-byte TX
// FIFO, so while the FIFO is full it blocks and moves the clock on. With tx_fifo 0, as ESP8266 software serial, a write
// returns once its last byte is on the line. Received bytes wait in 256 bytes of buffer and the 32-byte RX FIFO; more
// are lost. Both ends of a bridge share one clock.
class Rp2040Line : public WireUart {
 public:
  static constexpr size_t RX_ROOM = 256 + 32;

  Rp2040Line(uint32_t baud, uint32_t &clock_us)
      : WireUart(baud), char_us((10 * 1000000 + baud - 1) / baud), clock_us_(clock_us) {}

  void write_array(const uint8_t *data, size_t len) override {
    WireUart::write_array(data, len);
    for (size_t i = 0; i < len; i++) {
      this->on_line_us.push_back(std::max(this->line_free_us_, this->clock_us_));
      this->line_free_us_ = this->on_line_us.back() + this->char_us;
    }
    const uint32_t fifo_us = this->tx_fifo * this->char_us;
    if (this->line_free_us_ > this->clock_us_ + fifo_us) {
      this->clock_us_ = this->line_free_us_ - fifo_us;
    }
  }
  // A byte from the other device has come in.
  void hear(uint8_t byte) {
    if (this->rx.size() < RX_ROOM) {
      this->rx.push_back(byte);
    } else {
      this->lost++;
    }
  }
  // The longest silence between two written bytes on the line.
  uint32_t longest_pause_us() const {
    uint32_t longest = 0;
    for (size_t i = 1; i < this->on_line_us.size(); i++) {
      longest = std::max(longest, this->on_line_us[i] - this->on_line_us[i - 1] - this->char_us);
    }
    return longest;
  }

  const uint32_t char_us;
  uint32_t tx_fifo{32};
  std::vector<uint32_t> on_line_us;  // when each written byte starts on the line
  size_t lost{0};

 protected:
  uint32_t &clock_us_;
  uint32_t line_free_us_{0};
};

// One loop pass: both directions are polled as UARTBridge::loop() does, then other components take pass_us.
static void loop_pass(UARTBridgePipe &ab, UARTBridgePipe *ba, uint32_t &clock_us, uint32_t pass_us = PASS_US) {
  ab.poll(clock_us);
  if (ba != nullptr) {
    ba->poll(clock_us);
  }
  clock_us += pass_us;
}

TEST(UARTBridge, LineThatCannotReportItsRoomStillRunsFull) {
  // 115200 baud with the room unknown, as on the RP2040. Each write takes what the line sent since the last one
  // returned, so no write blocks for a whole block. 4096 bytes waiting in the source keep the line busy, also when a
  // loop pass takes 1 or 2 ms (the TX FIFO lasts 32 characters, 2.8 ms): they are out in 4096 characters of 87 us,
  // within 3 %, without a pause between two bytes.
  for (uint32_t pass_us : {PASS_US, 1000u, 2000u}) {
    SCOPED_TRACE(pass_us);
    uint32_t clock_us = 1000000;
    Rp2040Line a(115200, clock_us);
    Rp2040Line b(115200, clock_us);
    UARTBridgePipe pipe(&a, &b);
    pipe.set_from_wire();
    pipe.set_to_wire();
    pipe.setup();
    a.receive(4096);
    while (b.on_line_us.size() < 4096 && clock_us < 2000000) {
      loop_pass(pipe, nullptr, clock_us, pass_us);
    }
    ASSERT_EQ(b.joined(), frame(4096));
    // More writes than the 16 blocks: none of them waited for a whole block.
    EXPECT_GT(b.writes.size(), 16u);
    EXPECT_LE(b.on_line_us.back() + b.char_us - b.on_line_us.front(), 4096 * b.char_us * 103 / 100);
    EXPECT_EQ(b.longest_pause_us(), 0u);
  }
}

TEST(UARTBridge, FrameLeavesInOneWriteToALineWithoutTxFifo) {
  // A line that cannot report its room and has no TX FIFO, as ESP8266 software serial: a pause between two writes is a
  // pause on the line. A 64-byte frame at 19200 baud with loop passes of 1 ms leaves in one write, without a pause
  // inside (1.5 characters are 781 us there).
  uint32_t clock_us = 1000000;
  WireUart a(19200);
  Rp2040Line b(19200, clock_us);
  b.tx_fifo = 0;
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_wire();
  pipe.set_to_wire();
  pipe.setup();
  a.receive(64);
  for (int pass = 0; pass < 100; pass++) {
    loop_pass(pipe, nullptr, clock_us, 1000);
  }
  ASSERT_EQ(b.joined(), frame(64));
  EXPECT_EQ(b.writes.size(), 1u);
  EXPECT_EQ(b.longest_pause_us(), 0u);
}

TEST(UARTBridge, PushedBlockGoesOutWholeToALineThatCannotReportItsRoom) {
  // A pushed block has no driver buffer to wait in. It goes out in one write, so a second push right after it fits
  // behind it and is not dropped.
  VirtualEnd a;
  WireUart b(115200);
  UARTBridgePipe pipe(&a, &b);
  pipe.set_from_virtual(&a);
  pipe.set_to_wire();
  pipe.setup();
  const auto first = frame(256);
  const auto second = frame(100, 0x80);
  a.inject_rx(first.data(), first.size());
  a.inject_rx(second.data(), second.size());
  ASSERT_FALSE(b.writes.empty());
  EXPECT_EQ(b.writes[0], first);
  // The second block waits for the line and a frame gap: 256 characters of 87 us, then 1750 us.
  for (uint32_t t = 1000000; pipe.poll(t) && t < 2000000; t += PASS_US) {
  }
  std::vector<uint8_t> both(first);
  both.insert(both.end(), second.begin(), second.end());
  EXPECT_EQ(b.joined(), both);
}

TEST(UARTBridge, BothWaysAtOnceLoseNothingOnLinesThatCannotReportTheirRoom) {
  // Two RP2040 UARTs at 9600 baud stream 2000 bytes at each other at the same time. One write of a whole 256-byte block
  // would block for 233 ms, while 224 bytes come in at the other end. Paced, both directions get every byte and finish
  // within a character of each other.
  uint32_t clock_us = 1000000;
  Rp2040Line a(9600, clock_us);
  Rp2040Line b(9600, clock_us);
  UARTBridgePipe ab(&a, &b);
  UARTBridgePipe ba(&b, &a);
  for (UARTBridgePipe *pipe : {&ab, &ba}) {
    pipe->set_from_wire();
    pipe->set_to_wire();
    pipe->setup();
  }
  const uint32_t start_us = clock_us;
  size_t heard = 0;
  // 2000 characters of 1042 us take 2.1 s; 3 s leave time for the last block.
  while (clock_us < start_us + 3000000) {
    // Each sender puts one byte per character on its line.
    for (; heard < 2000 && start_us + (heard + 1) * a.char_us <= clock_us; heard++) {
      a.hear(static_cast<uint8_t>(heard));
      b.hear(static_cast<uint8_t>(heard));
    }
    loop_pass(ab, &ba, clock_us);
  }
  EXPECT_EQ(a.lost, 0u);
  EXPECT_EQ(b.lost, 0u);
  EXPECT_EQ(b.joined(), frame(2000));
  EXPECT_EQ(a.joined(), frame(2000));
  const uint32_t a_end = a.on_line_us.back();
  const uint32_t b_end = b.on_line_us.back();
  EXPECT_LE(std::max(a_end, b_end) - std::min(a_end, b_end), a.char_us);
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
