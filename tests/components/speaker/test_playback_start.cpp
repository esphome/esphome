#include <gtest/gtest.h>

#include "esphome/components/speaker/media_player/playback_start.h"

namespace esphome::speaker::testing {

TEST(PlaybackStart, AnnouncementUsesQuarterOfAdjustedFlacThreshold) {
  constexpr size_t media_initial_bytes = 48000;
  EXPECT_FALSE(playback_start_ready(true, 0, media_initial_bytes));
  EXPECT_FALSE(playback_start_ready(true, 11999, media_initial_bytes));
  EXPECT_TRUE(playback_start_ready(true, 12000, media_initial_bytes));
  EXPECT_TRUE(playback_start_ready(true, 42799, media_initial_bytes));
}

TEST(PlaybackStart, MediaRetainsFullRawThreshold) {
  constexpr size_t media_initial_bytes = 48000;
  EXPECT_FALSE(playback_start_ready(false, 12000, media_initial_bytes));
  EXPECT_FALSE(playback_start_ready(false, 47999, media_initial_bytes));
  EXPECT_TRUE(playback_start_ready(false, 48000, media_initial_bytes));
  EXPECT_TRUE(playback_start_ready(false, 0, 0));
}

TEST(PlaybackStart, AnnouncementThresholdScalesWithFormat) {
  // These inputs are already adjusted for codec compression and ring capacity by the pipeline.
  EXPECT_FALSE(playback_start_ready(true, 23999, 96000));  // WAV
  EXPECT_TRUE(playback_start_ready(true, 24000, 96000));
  EXPECT_FALSE(playback_start_ready(true, 2999, 12000));  // MP3 or Opus
  EXPECT_TRUE(playback_start_ready(true, 3000, 12000));
  EXPECT_FALSE(playback_start_ready(true, 2249, 9000));  // Ring-capacity-limited estimate
  EXPECT_TRUE(playback_start_ready(true, 2250, 9000));
}

}  // namespace esphome::speaker::testing
