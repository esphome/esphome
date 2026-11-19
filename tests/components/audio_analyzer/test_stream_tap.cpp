#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/stream_tap.h"
#include "test_helpers.h"

namespace esphome::audio_analyzer::testing {

static std::vector<int16_t> drain(SampleFifo &fifo) {
  std::vector<int16_t> out;
  fifo.consume(fifo.available(), [&out](int16_t s) { out.push_back(s); });
  return out;
}

TEST(AudioAnalyzerFifo, KeepsOrderAcrossTheWrap) {
  SampleFifo fifo;
  ASSERT_TRUE(fifo.allocate(4));
  for (int16_t round = 0; round < 3; ++round) {
    for (int16_t i = 0; i < 3; ++i) {
      EXPECT_TRUE(fifo.push(round * 10 + i));
    }
    EXPECT_EQ(fifo.available(), 0u);  // not published yet
    fifo.publish();
    EXPECT_EQ(drain(fifo), (std::vector<int16_t>{static_cast<int16_t>(round * 10), static_cast<int16_t>(round * 10 + 1),
                                                 static_cast<int16_t>(round * 10 + 2)}));
  }
}

TEST(AudioAnalyzerFifo, FullRefusesUntilTheConsumerFrees) {
  SampleFifo fifo;
  ASSERT_TRUE(fifo.allocate(2));
  EXPECT_TRUE(fifo.push(1));
  EXPECT_TRUE(fifo.push(2));
  EXPECT_FALSE(fifo.push(3));
  fifo.publish();
  fifo.skip(1);
  EXPECT_TRUE(fifo.push(4));
  fifo.publish();
  EXPECT_EQ(drain(fifo), (std::vector<int16_t>{2, 4}));
  fifo.push(5);
  fifo.publish();
  fifo.clear();
  EXPECT_EQ(fifo.available(), 0u);
}

// 16-bit mono frames of constant values
static std::vector<uint8_t> frames_of(std::initializer_list<int16_t> values) {
  std::vector<uint8_t> bytes;
  for (int16_t v : values) {
    bytes.push_back(static_cast<uint8_t>(v & 0xFF));
    bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  }
  return bytes;
}

static void write(StreamTap &tap, std::initializer_list<int16_t> values) {
  const std::vector<uint8_t> bytes = frames_of(values);
  tap.write(bytes.data(), bytes.size() / 2, 2, 1);
}

TEST(AudioAnalyzerTap, CopiesOnlyAfterTheFirstDrain) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(8));
  write(tap, {1, 2});
  EXPECT_EQ(tap.get_fifo().available(), 0u);
  EXPECT_TRUE(tap.drain_if_restarting());
  EXPECT_FALSE(tap.drain_if_restarting());
  write(tap, {3, 4});
  EXPECT_EQ(drain(tap.get_fifo()), (std::vector<int16_t>{3, 4}));
}

TEST(AudioAnalyzerTap, OverflowRestartsFromTheNewestAudio) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(4));
  tap.drain_if_restarting();
  write(tap, {1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(tap.take_overflow());
  EXPECT_FALSE(tap.take_overflow());
  // What fitted is published, but loop() drops it on the restart
  write(tap, {7});
  EXPECT_TRUE(tap.drain_if_restarting());
  EXPECT_EQ(tap.get_fifo().available(), 0u);
  write(tap, {8, 9});
  EXPECT_EQ(drain(tap.get_fifo()), (std::vector<int16_t>{8, 9}));
}

TEST(AudioAnalyzerTap, RestartFromLoopDropsTheBufferedAudio) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(8));
  tap.drain_if_restarting();
  write(tap, {1, 2});
  tap.restart();
  write(tap, {3});  // not copied while restarting
  EXPECT_TRUE(tap.drain_if_restarting());
  write(tap, {4});
  EXPECT_EQ(drain(tap.get_fifo()), (std::vector<int16_t>{4}));
}

TEST(AudioAnalyzerTap, DecimatesAndDownmixes) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(64));
  tap.get_decimator().configure(2, reference_sections(2));
  tap.drain_if_restarting();
  // 64 stereo frames of DC at 0.5 on the right only: 32 samples settling towards 0.25
  std::vector<uint8_t> bytes;
  for (int i = 0; i < 64; ++i) {
    for (int16_t v : {int16_t{0}, int16_t{16384}}) {
      bytes.push_back(static_cast<uint8_t>(v & 0xFF));
      bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
  }
  tap.write(bytes.data(), 64, 2, 2);
  const std::vector<int16_t> out = drain(tap.get_fifo());
  ASSERT_EQ(out.size(), 32u);
  EXPECT_NEAR(out.back(), 8192, 40);
}

}  // namespace esphome::audio_analyzer::testing
