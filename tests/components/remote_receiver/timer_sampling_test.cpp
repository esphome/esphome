#include <gtest/gtest.h>
#include <array>
#include <vector>
#include "esphome/components/remote_receiver/remote_receiver.h"

// remote_receiver.h only defines the timer store when the backend is enabled, so an ESP32 build
// compiles this file to nothing
#ifdef USE_REMOTE_RECEIVER_TIMER_SAMPLING

namespace esphome::remote_receiver::testing {

static constexpr uint32_t SAMPLE_US = 50;
static constexpr uint32_t FILTER_US = 100;
static constexpr uint32_t IDLE_US = 10000;
static constexpr uint16_t FRAME_ENTRIES = 16;

// Feeds the sampler one level per sample period and collects the frames it publishes
class Sampler {
 public:
  Sampler() {
    this->store_.buffer = this->buffer_.data();
    this->store_.capture = this->buffer_.data();
    this->store_.frame_entries = FRAME_ENTRIES;
    this->store_.filter_ticks = FILTER_US * TIMER_TICKS_PER_US;
    this->store_.idle_ticks = IDLE_US * TIMER_TICKS_PER_US;
  }
  // true is a mark, as digital_read() returns it with the pin's inverted: applied
  void feed(bool level, uint32_t us) {
    for (uint32_t t = 0; t < us; t += SAMPLE_US) {
      if (this->store_.sample(level, SAMPLE_US * TIMER_TICKS_PER_US) && !this->hold)
        this->collect();
    }
  }
  // Takes the pending frame, as loop() does once it has dispatched it
  void collect() {
    if (this->store_.ready == nullptr)
      return;
    this->frames.emplace_back(this->store_.ready, this->store_.ready + this->store_.ready_count);
    this->store_.ready = nullptr;
  }
  // Leave a published frame pending instead of collecting it
  bool hold{false};
  RemoteReceiverComponentStore &store() { return this->store_; }
  std::vector<std::vector<int32_t>> frames;

 protected:
  RemoteReceiverComponentStore store_;
  std::array<int32_t, FRAME_ENTRIES * 2> buffer_{};
};

TEST(RemoteReceiverTimerSampling, MarkIsPositive) {
  Sampler s;
  s.feed(false, 1000);
  s.feed(true, 9000);
  s.feed(false, 4500);
  s.feed(true, 600);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -4500, 600, -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, GlitchInSpaceDoesNotRestartIdle) {
  Sampler s;
  s.feed(true, 9000);
  s.feed(false, 5000);
  // shorter than the filter, folded into the space
  s.feed(true, 50);
  s.feed(false, 6000);
  s.feed(true, 2000);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 2u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US)}));
  EXPECT_EQ(s.frames[1], (std::vector<int32_t>{2000, -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, MarkAtIdleStartsNextFrame) {
  Sampler s;
  s.feed(true, 9000);
  // the next mark arrives on the sample where the space reaches idle
  s.feed(false, IDLE_US);
  s.feed(true, 2000);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 2u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US)}));
  EXPECT_EQ(s.frames[1], (std::vector<int32_t>{2000, -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, SpaceBelowIdleKeepsOneFrame) {
  Sampler s;
  s.feed(true, 9000);
  s.feed(false, IDLE_US - SAMPLE_US);
  s.feed(true, 2000);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US - SAMPLE_US), 2000,
                                               -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, LoneGlitchPublishesNothing) {
  Sampler s;
  s.feed(true, 50);
  s.feed(false, 20000);
  EXPECT_TRUE(s.frames.empty());
}

TEST(RemoteReceiverTimerSampling, PendingFrameSurvivesABusyDrop) {
  Sampler s;
  s.hold = true;
  s.feed(true, 9000);
  s.feed(false, 20000);  // published and left pending, as if the loop has not run yet
  const int32_t *pending = s.store().ready;
  ASSERT_NE(pending, nullptr);
  const uint16_t pending_count = s.store().ready_count;

  // a second frame finishes while the first is still pending, so it is dropped
  s.feed(true, 2000);
  s.feed(false, 20000);
  EXPECT_TRUE(s.store().busy);
  const int32_t *after = s.store().ready;
  const uint16_t after_count = s.store().ready_count;
  EXPECT_EQ(after, pending);
  EXPECT_EQ(after_count, pending_count);

  // the loop releases the first frame, then reception recovers
  s.hold = false;
  s.collect();
  s.feed(true, 3000);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 2u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US)}));
  EXPECT_EQ(s.frames[1], (std::vector<int32_t>{3000, -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, OversizedFrameDropsAndRecovers) {
  Sampler s;
  // more runs than one frame half holds
  s.feed(true, 200);
  for (uint16_t i = 0; i < FRAME_ENTRIES; i++) {
    s.feed(false, 200);
    s.feed(true, 200);
  }
  s.feed(false, 20000);
  EXPECT_TRUE(s.store().overflow);
  EXPECT_TRUE(s.frames.empty());

  // the next frame is captured normally
  s.feed(true, 9000);
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US)}));
}

TEST(RemoteReceiverTimerSampling, FilteredLeadingMarkIsDroppedBeforeAFrame) {
  Sampler s;
  s.feed(true, 50);     // shorter than the filter
  s.feed(false, 2000);  // the space that follows it does not start a frame
  s.feed(true, 9000);   // a valid frame follows
  s.feed(false, 20000);
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0], (std::vector<int32_t>{9000, -static_cast<int32_t>(IDLE_US)}));
}

}  // namespace esphome::remote_receiver::testing

#endif  // USE_REMOTE_RECEIVER_TIMER_SAMPLING
