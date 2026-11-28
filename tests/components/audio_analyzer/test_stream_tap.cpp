#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "esphome/components/audio_analyzer/stream_tap.h"
#include "fake_sample_buffer.h"
#include "test_helpers.h"

namespace esphome::audio_analyzer::testing {

/// Calls ``hook`` before each write to the buffer, to land a restart in the middle of a producer's write.
class HookedBuffer final : public SampleBuffer {
 public:
  explicit HookedBuffer(std::unique_ptr<SampleBuffer> buffer) : buffer_(std::move(buffer)) {}
  bool write(const void *data, size_t length) override {
    ++this->writes;
    if (this->hook) {
      this->hook();
    }
    return this->buffer_->write(data, length);
  }
  size_t read(void *data, size_t length) override { return this->buffer_->read(data, length); }
  size_t available() const override { return this->buffer_->available(); }
  void reset() override { this->buffer_->reset(); }

  std::function<void()> hook;
  int writes{0};

 protected:
  std::unique_ptr<SampleBuffer> buffer_;
};

class TestTap : public StreamTap {
 public:
  HookedBuffer *hook_buffer() {
    auto hooked = std::make_unique<HookedBuffer>(std::move(this->buffer_));
    HookedBuffer *raw = hooked.get();
    this->buffer_ = std::move(hooked);
    return raw;
  }
};

static std::vector<int16_t> drain(StreamTap &tap) {
  std::vector<int16_t> out;
  tap.consume(tap.available(), [&out](int16_t s) { out.push_back(s); });
  return out;
}

// 16-bit frames of the given values, ``channels`` identical channels
static std::vector<uint8_t> frames_of(const std::vector<int16_t> &values, uint8_t channels = 1) {
  std::vector<uint8_t> bytes;
  for (int16_t v : values) {
    for (uint8_t c = 0; c < channels; ++c) {
      bytes.push_back(static_cast<uint8_t>(v & 0xFF));
      bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
  }
  return bytes;
}

static void write(StreamTap &tap, const std::vector<int16_t> &values) {
  const std::vector<uint8_t> bytes = frames_of(values);
  tap.write(bytes.data(), values.size(), 2, 1);
}

static std::vector<int16_t> counting(int16_t first, uint32_t count) {
  std::vector<int16_t> values(count);
  for (uint32_t i = 0; i < count; ++i) {
    values[i] = static_cast<int16_t>(first + i);
  }
  return values;
}

TEST(AudioAnalyzerTap, AllocationFailureLeavesItUnallocated) {
  StreamTap tap;
  FakeSampleBuffer::fail_next_create = true;
  EXPECT_FALSE(tap.allocate(8));
  EXPECT_FALSE(tap.is_allocated());
  EXPECT_TRUE(tap.allocate(8));
  EXPECT_TRUE(tap.is_allocated());
}

TEST(AudioAnalyzerTap, CopiesOnlyAfterTheFirstDrain) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(8));
  write(tap, {1, 2});
  EXPECT_EQ(tap.available(), 0u);
  EXPECT_TRUE(tap.drain_if_restarting());
  EXPECT_FALSE(tap.drain_if_restarting());
  write(tap, {3, 4});
  EXPECT_EQ(drain(tap), (std::vector<int16_t>{3, 4}));
}

TEST(AudioAnalyzerTap, OverflowRestartsFromTheNewestAudio) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(4));
  tap.drain_if_restarting();
  write(tap, {1, 2});
  write(tap, {3, 4, 5});  // doesn't fit: nothing of it is written
  EXPECT_TRUE(tap.take_overflow());
  EXPECT_FALSE(tap.take_overflow());
  EXPECT_EQ(tap.available(), 0u);  // what is buffered waits for the drain
  write(tap, {6});                 // not copied until loop() drained
  EXPECT_TRUE(tap.drain_if_restarting());
  write(tap, {7, 8});
  EXPECT_EQ(drain(tap), (std::vector<int16_t>{7, 8}));
}

TEST(AudioAnalyzerTap, RestartWaitsForTheProducer) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(8));
  tap.drain_if_restarting();
  write(tap, {1, 2});
  tap.restart();
  EXPECT_EQ(tap.available(), 0u);
  // The producer hasn't acknowledged yet: loop() can't drain
  EXPECT_FALSE(tap.drain_if_restarting());
  write(tap, {3});  // acknowledges, not copied
  EXPECT_TRUE(tap.drain_if_restarting());
  write(tap, {4});
  EXPECT_EQ(drain(tap), (std::vector<int16_t>{4}));
}

TEST(AudioAnalyzerTap, RestartRightAfterADrainKeepsTheCopy) {
  StreamTap tap;
  ASSERT_TRUE(tap.allocate(8));
  tap.drain_if_restarting();
  tap.restart();  // nothing old is left
  write(tap, {1});
  EXPECT_EQ(drain(tap), (std::vector<int16_t>{1}));
}

TEST(AudioAnalyzerTap, RestartDuringAWriteLeavesNothingStale) {
  // A restart from another task while the producer is in the middle of a long write
  TestTap tap;
  ASSERT_TRUE(tap.allocate(1024));
  HookedBuffer *buffer = tap.hook_buffer();
  tap.drain_if_restarting();
  buffer->hook = [&tap, buffer]() {
    if (buffer->writes == 2) {
      tap.restart();
      // The write in progress still lands; loop() must not drain before the producer stopped
      EXPECT_FALSE(tap.drain_if_restarting());
    }
  };
  write(tap, counting(0, 500));  // 128-sample writes: the restart lands during the second
  EXPECT_EQ(buffer->writes, 2);
  EXPECT_TRUE(tap.drain_if_restarting());
  EXPECT_EQ(tap.available(), 0u);
  write(tap, counting(1000, 3));
  EXPECT_EQ(drain(tap), counting(1000, 3));
}

TEST(AudioAnalyzerTap, OverflowDuringAWriteStopsIt) {
  TestTap tap;
  ASSERT_TRUE(tap.allocate(200));
  HookedBuffer *buffer = tap.hook_buffer();
  tap.drain_if_restarting();
  write(tap, counting(0, 500));
  EXPECT_EQ(buffer->writes, 2);  // the second didn't fit; the rest were never tried
  EXPECT_TRUE(tap.take_overflow());
  EXPECT_TRUE(tap.drain_if_restarting());
}

TEST(AudioAnalyzerTap, CopyAsIsMatchesTheConvertedPath) {
  // 16-bit mono at the analysis rate is copied as is; stereo of the same values goes through the conversion
  const std::vector<int16_t> values{-32768, -12345, -1, 0, 1, 255, 256, 12345, 32767};
  for (uint8_t channels : {1, 2}) {
    StreamTap tap;
    ASSERT_TRUE(tap.allocate(64));
    tap.drain_if_restarting();
    const std::vector<uint8_t> bytes = frames_of(values, channels);
    tap.write(bytes.data(), values.size(), 2, channels);
    EXPECT_EQ(drain(tap), values);
  }
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
  const std::vector<int16_t> out = drain(tap);
  ASSERT_EQ(out.size(), 32u);
  EXPECT_NEAR(out.back(), 8192, 40);
}

TEST(AudioAnalyzerTap, ConcurrentRestartsLeaveNothingStale) {
  // A producer thread writes a counting sequence while loop() reads, restarts and falls behind. Between two drains,
  // what loop() reads counts up without a gap or a repeat, and after a restart only audio from writes that began
  // after it is read.
  TestTap tap;
  ASSERT_TRUE(tap.allocate(4096));
  HookedBuffer *buffer = tap.hook_buffer();
  // Producer only: now and then it is preempted between checking for a restart and writing
  buffer->hook = [buffer]() {
    if (buffer->writes % 4 == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  };
  std::atomic<bool> done{false};
  // Count of the producer's next write; the samples are this count, wrapped to 16 bits
  std::atomic<int32_t> next_write{0};
  std::thread producer([&tap, &done, &next_write]() {
    int32_t next = 0;
    uint32_t size = 1;
    while (!done.load()) {
      size = size % 300 + 37;
      const std::vector<int16_t> values = counting(static_cast<int16_t>(next), size);
      next += static_cast<int32_t>(size);
      next_write.store(next);
      write(tap, values);
      // Paced like an audio task, so loop() keeps up except when it stalls
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
  });

  bool have_last = false;
  int16_t last = 0;
  bool restarted = false;
  int32_t restart_boundary = 0;
  bool check_boundary = false;
  int32_t boundary = 0;
  uint32_t samples = 0;
  uint32_t drains = 0;
  uint32_t errors = 0;
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
  for (uint32_t pass = 0; std::chrono::steady_clock::now() < end; ++pass) {
    if (tap.drain_if_restarting()) {
      have_last = false;
      check_boundary = restarted;
      boundary = restart_boundary;
      restarted = false;
      ++drains;
    }
    const uint32_t count = tap.available();
    // Every sample counted is from a write that started before this
    const int32_t head = next_write.load();
    tap.consume(count, [&](int16_t sample) {
      if (have_last && sample != static_cast<int16_t>(last + 1)) {
        ++errors;
      }
      // The rest of the piece follows from its first sample
      const int32_t position =
          head - static_cast<uint16_t>(static_cast<uint16_t>(head) - static_cast<uint16_t>(sample));
      if (!have_last && check_boundary && position < boundary) {
        ++errors;  // written by a write that began before the restart
      }
      last = sample;
      have_last = true;
      ++samples;
      // Audio without a drain: the restart came right after a drain, when nothing old was left
      restarted = false;
    });
    if (pass % 20 == 7) {
      restart_boundary = next_write.load();
      restarted = true;
      tap.restart();
    }
    // A loop pass, and now and then a stall that overflows the buffer
    std::this_thread::sleep_for(std::chrono::microseconds(pass % 100 == 30 ? 5000 : 50));
  }
  done = true;
  producer.join();
  EXPECT_EQ(errors, 0u);
  EXPECT_GT(samples, 1000u);
  EXPECT_GT(drains, 5u);
}

}  // namespace esphome::audio_analyzer::testing
