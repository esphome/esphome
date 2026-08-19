#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "esphome/components/uart/software_serial_rx_decoder.h"

namespace esphome::uart::testing {

namespace {

// 80 MHz ESP8266 clock
constexpr uint32_t CPU_HZ = 80000000;

// Drives a SoftwareSerialRxDecoder with a simulated line: frames are turned into
// edges at their ideal cycle counts (plus optional jitter), and the main loop is
// emulated by poll(), which finalizes a pending byte and drains the buffer.
class LineSim {
 public:
  LineSim(uint32_t baud, uint8_t data_bits, bool parity, bool odd, uint8_t stop_bits, size_t buffer_size = 64)
      : data_bits_(data_bits), parity_(parity), odd_(odd), stop_bits_(stop_bits), buffer_(buffer_size) {
    this->bit_ = CPU_HZ / baud;
    this->dec_.setup(this->bit_, data_bits, parity, stop_bits, this->buffer_.data(), buffer_size);
    this->dec_.reset(this->now_, true);
  }

  uint32_t bit_cycles() const { return this->bit_; }
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

  // Emit one frame starting at cycle `start`; `jitter` is the per edge offset in cycles
  // (positive or negative) supplied by the caller. Returns the cycle at which the frame ends.
  uint32_t send(
      uint8_t value, uint32_t start, const std::function<int32_t()> &jitter = [] { return 0; }) {
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
    uint32_t t = start;
    for (bool b : bits) {
      if (b != this->line_)
        this->edge(static_cast<uint32_t>(static_cast<int64_t>(t) + jitter()), b);
      t += this->bit_;
    }
    return t;
  }

 protected:
  uint8_t data_bits_;
  bool parity_;
  bool odd_;
  uint8_t stop_bits_;
  uint32_t bit_{0};
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
    t = sim.send(b, t, [&] { return static_cast<int32_t>(jit(rng) * sim.bit_cycles()); });
    if (gap_bits != 0 && n % 3 == 2) {
      t += gap_bits * sim.bit_cycles();
      sim.poll(t);
    } else if (rng() % 4 == 0) {
      sim.poll(t);
    }
  }
  sim.poll(t + 20 * sim.bit_cycles());
  EXPECT_EQ(sim.received(), sent) << "baud " << f.baud << " jitter " << jitter_bits;
}

}  // namespace

TEST(SoftwareSerialRxDecoder, DecodesBackToBackFramesAtCommonBaudRates) {
  for (uint32_t baud : {2400U, 4800U, 9600U, 19200U, 38400U}) {
    run_stream({baud, 8, false, false, 1}, 0.0, 0, 300, baud);
  }
}

TEST(SoftwareSerialRxDecoder, ToleratesEdgeJitterUpToAQuarterBit) {
  // Consecutive edges can each be off by up to a quarter bit in either direction,
  // so the measured run length is within half a bit of the true one.
  run_stream({9600, 8, false, false, 1}, 0.24, 0, 2000, 1);
  run_stream({9600, 8, false, false, 1}, 0.24, 7, 2000, 2);
  run_stream({38400, 8, false, false, 1}, 0.24, 4, 2000, 3);
}

TEST(SoftwareSerialRxDecoder, HandlesParityDataBitsAndStopBits) {
  run_stream({9600, 8, true, false, 1}, 0.2, 3, 1000, 4);    // 8E1
  run_stream({4800, 8, true, true, 2}, 0.2, 2, 1000, 5);     // 8O2
  run_stream({19200, 7, false, false, 2}, 0.2, 5, 1000, 6);  // 7N2
  run_stream({2400, 5, false, false, 1}, 0.2, 1, 1000, 7);   // 5N1
}

TEST(SoftwareSerialRxDecoder, AllOnesByteCompletesOnlyByFinalize) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_cycles();
  uint32_t t = 5000;
  // Start bit, then the line goes high and stays there: 0xFF has no closing edge.
  sim.edge(t, false);
  sim.edge(t + bit, true);
  EXPECT_TRUE(sim.decoder().pending());
  // Eight data bits plus the stop bit must elapse after the rising edge.
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
  const uint32_t bit = sim.bit_cycles();
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
  const uint32_t bit = sim.bit_cycles();
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
  const uint32_t bit = sim.bit_cycles();
  // 0x31 = 0b00110001: start, 1, 0, 0, 0, 1, 1, 0, 0, stop. Lose the rising edge
  // of data bit 4 (ISR delayed past two edges), so the next edge arrives at the
  // same level as the last one the decoder saw.
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
  sim.poll(t + 20 * sim.bit_cycles());
  ASSERT_EQ(sim.received().size(), 7u);  // capacity is size - 1
  for (int n = 0; n < 7; n++)
    EXPECT_EQ(sim.received()[n], n);
}

TEST(SoftwareSerialRxDecoder, ResetDiscardsPartialFrame) {
  LineSim sim(9600, 8, false, false, 1);
  const uint32_t bit = sim.bit_cycles();
  sim.edge(5000, false);
  sim.edge(5000 + bit, true);
  EXPECT_TRUE(sim.decoder().pending());
  sim.decoder().reset(5000 + 2 * bit, true);
  EXPECT_FALSE(sim.decoder().pending());
  sim.poll(5000 + 30 * bit);
  EXPECT_TRUE(sim.received().empty());
}

}  // namespace esphome::uart::testing
