#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "esphome/components/uart/software_serial_rx_decoder.h"

namespace esphome::uart::testing {

namespace {

// Firmware time base: micros() << 4
constexpr uint32_t TICKS_PER_SECOND = 16000000;
constexpr uint32_t TICKS_PER_US = 16;

// Simulated line: frames become edges at exact baud timing plus jitter, truncated
// to whole microseconds like micros(); poll() emulates the main loop.

class LineSim {
 public:
  LineSim(uint32_t baud, uint8_t data_bits, bool parity, bool odd, uint8_t stop_bits, size_t buffer_size = 64)
      : data_bits_(data_bits), parity_(parity), odd_(odd), stop_bits_(stop_bits), buffer_(buffer_size) {
    this->bit_ = (TICKS_PER_SECOND + baud / 2) / baud;
    this->exact_bit_ = static_cast<double>(TICKS_PER_SECOND) / baud;
    this->dec_.setup(this->bit_, data_bits, parity, stop_bits, this->buffer_.data(), buffer_size);
    this->dec_.reset(this->now_, true);
  }

  uint32_t bit_ticks() const { return this->bit_; }
  // The sender's real rate; the decoder keeps the nominal one from the constructor.
  void set_line_baud(double baud) { this->exact_bit_ = TICKS_PER_SECOND / baud; }
  SoftwareSerialRxDecoder &decoder() { return this->dec_; }
  const std::vector<uint8_t> &received() const { return this->received_; }

  void edge(uint32_t at, bool level) {
    this->now_ = at;
    this->line_ = level;
    this->dec_.on_edge(at, level);
  }

  // Main loop pass at cycle `at`.
  void poll(uint32_t at) {
    this->now_ = at;
    if (this->dec_.pending() && this->dec_.finalize_due(at))
      this->dec_.finalize(at);
    while (this->dec_.available() > 0)
      this->received_.push_back(this->dec_.read_byte());
  }

  // Emit one frame at `start` with per edge `jitter` in ticks; returns the end tick.
  uint32_t send(uint8_t value, uint32_t start, const std::function<int32_t()> &jitter = [] { return 0; }) {
    std::vector<bool> bits;
    bits.push_back(false);
    int ones = 0;
    for (int i = 0; i < this->data_bits_; i++) {
      bool b = (value >> i) & 1;
      bits.push_back(b);
      ones += b;
    }
    if (this->parity_)
      bits.push_back(this->odd_ ? !(ones & 1) : (ones & 1));
    for (int i = 0; i < this->stop_bits_; i++)
      bits.push_back(true);
    double t = start;
    for (bool b : bits) {
      if (b != this->line_) {
        const auto at = static_cast<uint32_t>(static_cast<int64_t>(t) + jitter());
        this->edge(at / TICKS_PER_US * TICKS_PER_US, b);
      }
      t += this->exact_bit_;
    }
    return static_cast<uint32_t>(t);
  }

 protected:
  uint8_t data_bits_;
  bool parity_;
  bool odd_;
  uint8_t stop_bits_;
  uint32_t bit_{0};
  double exact_bit_{0};
  uint32_t now_{1000};
  bool line_{true};
  std::vector<uint8_t> buffer_;
  std::vector<uint8_t> received_;
  SoftwareSerialRxDecoder dec_;
};

struct FrameFormat {
  uint32_t baud;
  uint8_t data_bits;
  bool parity;
  bool odd;
  uint8_t stop_bits;
};

// Random bytes, back to back or with idle gaps, with bounded edge jitter.
void run_stream(const FrameFormat &f, double jitter_bits, uint32_t gap_bits, int count, unsigned seed) {
  LineSim sim(f.baud, f.data_bits, f.parity, f.odd, f.stop_bits);
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> jit(-jitter_bits, jitter_bits);
  const uint8_t mask = static_cast<uint8_t>((1U << f.data_bits) - 1);
  std::vector<uint8_t> sent;
  uint32_t t = 5000;
  for (int n = 0; n < count; n++) {
    uint8_t b = rng() & mask;
    sent.push_back(b);
    t = sim.send(b, t, [&] { return static_cast<int32_t>(jit(rng) * sim.bit_ticks()); });
    if (gap_bits != 0 && n % 3 == 2) {
      t += gap_bits * sim.bit_ticks();
      sim.poll(t);
    } else if (rng() % 4 == 0) {
      sim.poll(t);
    }
  }
  sim.poll(t + 20 * sim.bit_ticks());
  EXPECT_EQ(sim.received(), sent) << "baud " << f.baud << " jitter " << jitter_bits;
}

}  // namespace

TEST(SoftwareSerialRxDecoder, DecodesBackToBackFramesAtCommonBaudRates) {
  for (uint32_t baud : {2400U, 4800U, 9600U, 19200U, 38400U}) {
    run_stream({baud, 8, false, false, 1}, 0.0, 0, 300, baud);
  }
}

TEST(SoftwareSerialRxDecoder, ToleratesEdgeJitterUpToAQuarterBit) {
  // A quarter bit per edge keeps each run within the half bit rounding budget.
  run_stream({9600, 8, false, false, 1}, 0.24, 0, 2000, 1);
  run_stream({9600, 8, false, false, 1}, 0.24, 7, 2000, 2);
  run_stream({19200, 8, false, false, 1}, 0.24, 4, 2000, 3);  // edge decoder limit
}

TEST(SoftwareSerialRxDecoder, HandlesParityDataBitsAndStopBits) {
  run_stream({9600, 8, true, false, 1}, 0.2, 3, 1000, 4);    // 8E1
  run_stream({4800, 8, true, true, 2}, 0.2, 2, 1000, 5);     // 8O2
  run_stream({19200, 7, false, false, 2}, 0.2, 5, 1000, 6);  // 7N2
  run_stream({2400, 5, false, false, 1}, 0.2, 1, 1000, 7);   // 5N1
}

TEST(SoftwareSerialRxDecoder, AllOnesByteCompletesOnlyByFinalize) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_ticks();
  uint32_t t = 5000;
  // 0xFF: start bit, then the line stays high with no closing edge.
  sim.edge(t, false);
  sim.edge(t + bit, true);
  EXPECT_TRUE(sim.decoder().pending());
  sim.poll(t + bit + 8 * bit);
  EXPECT_TRUE(sim.received().empty());
  EXPECT_FALSE(sim.decoder().finalize_due(t + bit + 8 * bit));
  sim.poll(t + bit + 10 * bit);
  ASSERT_EQ(sim.received().size(), 1u);
  EXPECT_EQ(sim.received()[0], 0xFF);
  EXPECT_FALSE(sim.decoder().pending());
}

TEST(SoftwareSerialRxDecoder, LastByteOfBurstIsFinalizedThenNextFrameDecodes) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_ticks();
  uint32_t t = sim.send(0xA5, 5000);
  t = sim.send(0xF0, t);  // ends high, needs finalize
  EXPECT_TRUE(sim.received().empty());
  sim.poll(t + 2 * bit);
  ASSERT_EQ(sim.received().size(), 2u);
  EXPECT_EQ(sim.received()[0], 0xA5);
  EXPECT_EQ(sim.received()[1], 0xF0);
  // The stale last edge must not confuse the next start bit.
  t = sim.send(0x3C, t + 50 * bit);
  sim.poll(t + 2 * bit);
  ASSERT_EQ(sim.received().size(), 3u);
  EXPECT_EQ(sim.received()[2], 0x3C);
}

TEST(SoftwareSerialRxDecoder, BreakConditionIsDroppedAndResyncs) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_ticks();
  uint32_t t = 5000;
  sim.edge(t, false);
  t += 25 * bit;  // line held low for far longer than a frame
  sim.edge(t, true);
  t += 3 * bit;
  sim.poll(t);
  EXPECT_TRUE(sim.received().empty());
  t = sim.send(0xA5, t);
  sim.poll(t + 2 * bit);
  ASSERT_EQ(sim.received().size(), 1u);
  EXPECT_EQ(sim.received()[0], 0xA5);
}

TEST(SoftwareSerialRxDecoder, CollapsedEdgeIsIgnoredAndStreamRealignsAtIdle) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_ticks();
  // 0x31: lose the rising edge of bit 4, so the next edge repeats the last level.
  uint32_t t = 5000;
  sim.edge(t, false);                         // start
  sim.edge(t + 1 * bit, true);                // bit0 = 1
  sim.edge(t + 2 * bit, false);               // bits1..3 = 0
  sim.decoder().on_edge(t + 7 * bit, false);  // should have been bit6 falling edge; level still low
  sim.edge(t + 9 * bit, true);                // stop
  t += 10 * bit;
  // Next frame decodes correctly once the line has idled.
  t = sim.send(0x5A, t + 12 * bit);
  sim.poll(t + 2 * bit);
  ASSERT_FALSE(sim.received().empty());
  EXPECT_EQ(sim.received().back(), 0x5A);
}

TEST(SoftwareSerialRxDecoder, DropsBytesWhenBufferIsFullAndKeepsOldest) {
  LineSim sim(9600, 8, false, false, 1, 8);
  uint32_t t = 5000;
  for (int n = 0; n < 20; n++)
    t = sim.send(static_cast<uint8_t>(n), t);
  sim.poll(t + 20 * sim.bit_ticks());
  ASSERT_EQ(sim.received().size(), 7u);  // capacity is size - 1
  for (int n = 0; n < 7; n++)
    EXPECT_EQ(sim.received()[n], n);
}

TEST(SoftwareSerialRxDecoder, SetupAgainDropsStaleStateAndUsesNewBufferAndFraming) {
  // Mirrors load_settings(): buffered 8N1 bytes and an open frame, then setup() as 5E2.
  LineSim sim(9600, 8, false, false, 1);
  uint32_t t = 5000;
  for (int n = 0; n < 10; n++)
    t = sim.send(static_cast<uint8_t>(0x40 + n), t);
  sim.edge(t + 8 * sim.bit_ticks(), false);  // open a frame, never closed
  SoftwareSerialRxDecoder &dec = sim.decoder();
  ASSERT_GE(dec.available(), 9u);

  std::vector<uint8_t> small(4, 0xEE);
  const uint32_t bit = TICKS_PER_SECOND / 2400;
  dec.setup(bit, 5, true, 2, small.data(), small.size());
  EXPECT_EQ(dec.available(), 0u);
  EXPECT_FALSE(dec.pending());
  EXPECT_EQ(dec.read_byte(), 0);

  // Capacity of the 4 byte buffer is 3; the spare slot must stay untouched.
  auto send_5e2 = [&](uint8_t value, uint32_t start) {
    bool line = true;
    uint32_t at = start;
    auto put = [&](bool b) {
      if (b != line) {
        dec.on_edge(at, b);
        line = b;
      }
      at += bit;
    };
    put(false);
    int ones = 0;
    for (int i = 0; i < 5; i++) {
      bool b = (value >> i) & 1;
      ones += b;
      put(b);
    }
    put(ones & 1);
    put(true);
    put(true);
    return at;
  };
  uint32_t t2 = 5000;
  for (int n = 1; n <= 6; n++)
    t2 = send_5e2(static_cast<uint8_t>(n), t2);
  dec.finalize(t2 + 20 * bit);
  ASSERT_EQ(dec.available(), 3u);
  EXPECT_EQ(dec.read_byte(), 1);
  EXPECT_EQ(dec.read_byte(), 2);
  EXPECT_EQ(dec.read_byte(), 3);
  EXPECT_EQ(small[3], 0xEE);  // capacity slot is never written
}

TEST(SoftwareSerialRxDecoder, ResetDiscardsPartialFrame) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_ticks();
  sim.edge(5000, false);
  sim.edge(5000 + bit, true);
  EXPECT_TRUE(sim.decoder().pending());
  sim.decoder().reset(5000 + 2 * bit, true);
  EXPECT_FALSE(sim.decoder().pending());
  sim.poll(5000 + 30 * bit);
  EXPECT_TRUE(sim.received().empty());
}

#ifdef USE_UART_AUTO_BAUD
// Far enough after reset() for the line to count as idle, so the first frame is a sync candidate
constexpr uint32_t IDLE_START = 1000000;

// Packets of sync then mixed bytes, each after an idle gap and at its own rate; returns what was sent.
std::vector<uint8_t> send_packets(LineSim &sim, const std::vector<double> &line_bauds, uint8_t sync, unsigned seed) {
  std::vector<uint8_t> sent;
  std::mt19937 rng(seed);
  uint32_t t = IDLE_START;
  for (double baud : line_bauds) {
    sim.set_line_baud(baud);
    sent.push_back(sync);
    t = sim.send(sync, t);
    for (int n = 0; n < 18; n++) {
      // Long runs of zeros and ones are what a wrong bit time miscounts
      uint8_t b = (n % 3 == 0) ? 0x00 : (n % 3 == 1) ? 0xFF : static_cast<uint8_t>(rng());
      sent.push_back(b);
      t = sim.send(b, t);
    }
    t += 40 * sim.bit_ticks();
    sim.poll(t);
  }
  return sent;
}

// The unit test build sets UART_AUTO_BAUD_SYNC_BYTE to 0x55 (tests/components/uart/__init__.py).

TEST(SoftwareSerialRxDecoder, AutoBaudPacketWithoutSyncStaysNominalAndA7PercentFastLineCorrupts) {
  LineSim sim(9600, 8, false, false, 1);
  const auto sent = send_packets(sim, {9600 * 1.07}, 0x01, 11);
  EXPECT_NE(sim.received(), sent);
}

TEST(SoftwareSerialRxDecoder, AutoBaudRetimesEachPacketFromTheSyncByte) {
  LineSim sim(9600, 8, false, false, 1);
  // Each packet at its own rate: fast, slow, nominal, fast again
  const auto sent = send_packets(sim, {9600 * 1.07, 9600 * 0.93, 9600, 9600 * 1.10}, 0x55, 12);
  EXPECT_EQ(sim.received(), sent);
  // Measured from the last packet, 10% fast
  EXPECT_NEAR(sim.decoder().bit_ticks(), TICKS_PER_SECOND / (9600 * 1.10), 2.0);
}

TEST(SoftwareSerialRxDecoder, AutoBaudWithEvenParityAt1200) {
  LineSim sim(1200, 8, true, false, 1);
  const auto sent = send_packets(sim, {1200 * 0.94, 1200 * 1.06}, 0x55, 13);
  EXPECT_EQ(sim.received(), sent);
}

TEST(SoftwareSerialRxDecoder, AutoBaudLoneSyncByteIsTimedWhenFinalized) {
  LineSim sim(9600, 8, false, false, 1);
  sim.set_line_baud(9600 * 1.05);
  uint32_t t = sim.send(0x55, IDLE_START);
  // No closing edge: finalize() ends the frame
  sim.poll(t + 20 * sim.bit_ticks());
  ASSERT_EQ(sim.received().size(), 1u);
  EXPECT_NEAR(sim.decoder().bit_ticks(), TICKS_PER_SECOND / (9600 * 1.05), 2.0);
}

TEST(SoftwareSerialRxDecoder, AutoBaudKeepsNominalWhenThePacketDoesNotStartWithSync) {
  LineSim sim(9600, 8, false, false, 1);
  sim.set_line_baud(9600 * 1.05);
  uint32_t t = sim.send(0x55, IDLE_START);
  t = sim.send(0x01, t);
  sim.poll(t + 40 * sim.bit_ticks());
  ASSERT_NE(sim.decoder().bit_ticks(), sim.bit_ticks());
  // Next packet starts with another byte: back to the nominal bit time
  t = sim.send(0xAA, t + 40 * sim.bit_ticks());
  sim.poll(t + 40 * sim.bit_ticks());
  EXPECT_EQ(sim.decoder().bit_ticks(), sim.bit_ticks());
}

TEST(SoftwareSerialRxDecoder, AutoBaudIgnoresASyncFrameWithALowStopBit) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = TICKS_PER_SECOND / (9600 * 105 / 100);
  uint32_t t = IDLE_START;
  // 0x55 with the stop bit held low (framing error), then the line idles
  bool level = false;
  for (int i = 0; i < 9; i++) {
    sim.edge(t + i * bit, level);
    level = !level;
  }
  sim.edge(t + 12 * bit, true);
  sim.poll(t + 40 * bit);
  EXPECT_TRUE(sim.received().empty());
  EXPECT_EQ(sim.decoder().bit_ticks(), sim.bit_ticks());
}

TEST(SoftwareSerialRxDecoder, AutoBaudIdleAfterASlowPacketIsMeasuredAtTheNominalRate) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t nominal = sim.bit_ticks();
  sim.set_line_baud(9600 * 0.93);
  uint32_t t = sim.send(0x55, IDLE_START);
  t = sim.send(0x00, t);
  // With the stop bit, the line stays high about 11.3 nominal bits: over a frame (11 bits) at the nominal
  // rate, under one at the slow measured rate
  t += 10 * nominal + nominal / 4;
  sim.poll(t);
  sim.set_line_baud(9600);
  t = sim.send(0x55, t);
  t = sim.send(0x00, t);
  sim.poll(t + 40 * nominal);
  EXPECT_EQ(sim.received(), (std::vector<uint8_t>{0x55, 0x00, 0x55, 0x00}));
  EXPECT_NEAR(sim.decoder().bit_ticks(), nominal, 2.0);
}

#endif  // USE_UART_AUTO_BAUD

}  // namespace esphome::uart::testing
