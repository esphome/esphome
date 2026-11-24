#include <gtest/gtest.h>

#include "esphome/components/audio/audio.h"

namespace esphome::audio::testing {

TEST(AudioStreamInfo, FramesToMicroseconds) {
  AudioStreamInfo info(16, 2, 48000);
  EXPECT_EQ(info.frames_to_microseconds(48), 1000U);
  EXPECT_EQ(info.frames_to_microseconds(1), 21U);  // 20.83 us, rounded
}

TEST(AudioStreamInfo, FramesToMicrosecondsAboveFourThousandFrames) {
  // frames * 1000000 no longer fits 32 bits above 4294 frames, about 90 ms at 48 kHz.
  AudioStreamInfo info(24, 2, 48000);
  EXPECT_EQ(info.frames_to_microseconds(4800), 100000U);
  EXPECT_EQ(info.frames_to_microseconds(48000), 1000000U);
}

TEST(AudioStreamInfo, MsToBytesAboveTheThirtyTwoBitProduct) {
  // ms * 8 bytes * 96000 Hz no longer fits 32 bits above 5592 ms.
  AudioStreamInfo info(32, 2, 96000);
  EXPECT_EQ(info.ms_to_bytes(10000), 7680000U);
}

TEST(AudioStreamInfo, MsToSamplesAboveTheThirtyTwoBitProduct) {
  // ms * 2 channels * 48000 Hz no longer fits 32 bits above 44739 ms; sound_level allows 60 s.
  AudioStreamInfo info(16, 2, 48000);
  EXPECT_EQ(info.ms_to_samples(60000), 5760000U);
}

TEST(AudioStreamInfo, MsToFramesAboveTheThirtyTwoBitProduct) {
  // ms * 96000 Hz no longer fits 32 bits above 44739 ms.
  AudioStreamInfo info(16, 2, 96000);
  EXPECT_EQ(info.ms_to_frames(60000), 5760000U);
}

TEST(AudioStreamInfo, BytesToMsAboveFourMegabytes) {
  // bytes * 1000 no longer fits 32 bits above 4294967 bytes.
  AudioStreamInfo info(16, 2, 48000);
  EXPECT_EQ(info.bytes_to_ms(19200000), 100000U);
}

TEST(AudioStreamInfo, FramesToMillisecondsAboveFourMillionFrames) {
  // frames * 1000 no longer fits 32 bits above 4294967 frames, about 89 s at 48 kHz.
  AudioStreamInfo info(16, 2, 48000);
  uint32_t frames = 4800000 + 7;
  EXPECT_EQ(info.frames_to_milliseconds_with_remainder(&frames), 100000U);
  EXPECT_EQ(frames, 7U);
}

}  // namespace esphome::audio::testing
