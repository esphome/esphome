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

}  // namespace esphome::audio::testing
