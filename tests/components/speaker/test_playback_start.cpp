#include <gtest/gtest.h>

#include "esphome/components/speaker/media_player/playback_start.h"

namespace esphome::speaker::testing {

static constexpr size_t INITIAL_BYTES = 48000;

TEST(PlaybackStart, FullThresholdStartsAnyStream) {
  PlaybackStartGate media;
  EXPECT_FALSE(media.ready(false, 47999, INITIAL_BYTES, 0));
  EXPECT_TRUE(media.ready(false, 48000, INITIAL_BYTES, 1));
  PlaybackStartGate announcement;
  EXPECT_TRUE(announcement.ready(true, 48000, INITIAL_BYTES, 0));
}

TEST(PlaybackStart, StalledAnnouncementStartsBelowThreshold) {
  PlaybackStartGate gate;
  EXPECT_FALSE(gate.ready(true, 42799, INITIAL_BYTES, 1000));
  EXPECT_FALSE(gate.ready(true, 42799, INITIAL_BYTES, 1000 + ANNOUNCEMENT_STALL_START_MS - 1));
  EXPECT_TRUE(gate.ready(true, 42799, INITIAL_BYTES, 1000 + ANNOUNCEMENT_STALL_START_MS));
}

TEST(PlaybackStart, ArrivingDataKeepsTheFullReserve) {
  PlaybackStartGate gate;
  uint32_t now = 0;
  for (size_t bytes = 1000; bytes < INITIAL_BYTES; bytes += 1000) {
    EXPECT_FALSE(gate.ready(true, bytes, INITIAL_BYTES, now));
    now += ANNOUNCEMENT_STALL_START_MS - 1;
  }
}

TEST(PlaybackStart, StalledMediaWaitsForTheThreshold) {
  PlaybackStartGate gate;
  EXPECT_FALSE(gate.ready(false, 42799, INITIAL_BYTES, 0));
  EXPECT_FALSE(gate.ready(false, 42799, INITIAL_BYTES, 10000));
}

TEST(PlaybackStart, EmptyBufferNeverStartsEarly) {
  PlaybackStartGate gate;
  EXPECT_FALSE(gate.ready(true, 0, INITIAL_BYTES, 0));
  EXPECT_FALSE(gate.ready(true, 0, INITIAL_BYTES, 10000));
}

}  // namespace esphome::speaker::testing
