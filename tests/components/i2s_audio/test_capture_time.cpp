#include <gtest/gtest.h>

#include "esphome/components/i2s_audio/microphone/capture_time.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace esphome::i2s_audio::testing {

// 16 kHz, 16-bit mono: 256 frame DMA buffers of 512 bytes, 16 ms each
static const audio::AudioStreamInfo STREAM_INFO(16, 1, 16000);
static constexpr uint32_t BUFFER_BYTES = 512;
static constexpr int64_t BUFFER_US = 16000;
static constexpr uint32_t DESC_NUM = 4;
static constexpr int64_t NEWEST_US = 10000000;

TEST(I2SCaptureStart, DiscardsQueuedAndInFlightAudioBeforePublishing) {
  // Three completed descriptors and one still being filled at start. The marker tone ends mid-descriptor.
  constexpr size_t bytes_per_descriptor = 8;
  std::array<uint8_t, 5 * bytes_per_descriptor> audio{};
  std::fill_n(audio.begin(), 3 * bytes_per_descriptor + 2, 0x7f);
  audio[4 * bytes_per_descriptor] = 0x42;  // First sample in the next complete descriptor
  size_t cursor = 0;
  size_t reads = 0;
  const size_t discarded = discard_capture_backlog(
      4, bytes_per_descriptor,
      [&](size_t requested) -> size_t {
        if (reads++ == 0)
          return 0;  // A timeout must not advance the capture boundary
        const size_t count = std::min<size_t>(requested, 3);
        cursor += count;
        return count;
      },
      [] { return false; });
  ASSERT_EQ(discarded, 4 * bytes_per_descriptor);
  ASSERT_EQ(cursor, discarded);
  EXPECT_EQ(audio[cursor], 0x42);
  EXPECT_EQ(std::find(audio.begin() + cursor, audio.end(), 0x7f), audio.end());
}

TEST(I2SCaptureStart, StopInterruptsTheDrainEvenWithoutAnExternalClock) {
  size_t reads = 0;
  const size_t discarded =
      discard_capture_backlog(4, 8, [&](size_t) -> size_t { return ++reads == 1 ? 2 : 0; }, [&] { return reads == 3; });
  EXPECT_EQ(discarded, 2u);
  EXPECT_EQ(reads, 3u);
}

TEST(I2SCaptureTime, CountsBackFromTheNewestBuffer) {
  // Buffers 1 to 3 received, 1 read before, reading 2; 3 is the newest
  EXPECT_EQ(dma_capture_time_us(3 * BUFFER_BYTES, 0, 0, NEWEST_US, BUFFER_BYTES, BUFFER_BYTES, DESC_NUM, BUFFER_BYTES,
                                STREAM_INFO),
            NEWEST_US - 2 * BUFFER_US);
}

TEST(I2SCaptureTime, SkipsBuffersDroppedBeforeTheRead) {
  // Buffers 1 to 5 received, 1 read before, 2 and 3 dropped, reading 4; 5 is the newest
  EXPECT_EQ(dma_capture_time_us(5 * BUFFER_BYTES, 2, 2, NEWEST_US, BUFFER_BYTES, BUFFER_BYTES, DESC_NUM, BUFFER_BYTES,
                                STREAM_INFO),
            NEWEST_US - 2 * BUFFER_US);
}

TEST(I2SCaptureTime, UnknownAfterADropDuringTheRead) {
  EXPECT_EQ(dma_capture_time_us(5 * BUFFER_BYTES, 2, 1, NEWEST_US, BUFFER_BYTES, BUFFER_BYTES, DESC_NUM, BUFFER_BYTES,
                                STREAM_INFO),
            0);
}

TEST(I2SCaptureTime, UnknownWithoutAReceivedBuffer) {
  EXPECT_EQ(
      dma_capture_time_us(3 * BUFFER_BYTES, 0, 0, 0, BUFFER_BYTES, BUFFER_BYTES, DESC_NUM, BUFFER_BYTES, STREAM_INFO),
      0);
}

TEST(I2SCaptureTime, ByteCountsWrap) {
  // As CountsBackFromTheNewestBuffer, with the byte counts having wrapped between the earlier read and now
  const uint32_t bytes_read_before = UINT32_MAX - BUFFER_BYTES + 1;
  const uint32_t bytes_received = bytes_read_before + 2 * BUFFER_BYTES;
  EXPECT_EQ(dma_capture_time_us(bytes_received, 0, 0, NEWEST_US, bytes_read_before, BUFFER_BYTES, DESC_NUM,
                                BUFFER_BYTES, STREAM_INFO),
            NEWEST_US - 2 * BUFFER_US);
}

TEST(I2SCaptureTime, DropCountWraps) {
  // As SkipsBuffersDroppedBeforeTheRead, with the drop count about to wrap
  const uint32_t drops = UINT32_MAX;
  const uint32_t bytes_received = BUFFER_BYTES + drops * BUFFER_BYTES + 2 * BUFFER_BYTES;
  EXPECT_EQ(dma_capture_time_us(bytes_received, drops, drops, NEWEST_US, BUFFER_BYTES, BUFFER_BYTES, DESC_NUM,
                                BUFFER_BYTES, STREAM_INFO),
            NEWEST_US - 2 * BUFFER_US);
}

TEST(I2SCaptureTime, CountBackIsBoundedByTheQueue) {
  // More bytes after the read than the queue can hold: count back at most the read plus DESC_NUM buffers
  EXPECT_EQ(
      dma_capture_time_us(20 * BUFFER_BYTES, 0, 0, NEWEST_US, 0, BUFFER_BYTES, DESC_NUM, BUFFER_BYTES, STREAM_INFO),
      NEWEST_US - (1 + DESC_NUM) * BUFFER_US);
}

}  // namespace esphome::i2s_audio::testing
