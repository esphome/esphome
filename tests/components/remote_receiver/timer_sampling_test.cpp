#include <gtest/gtest.h>
#include <array>
#include <vector>
#include "esphome/components/remote_receiver/remote_receiver.h"

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
      if (this->store_.sample(level, SAMPLE_US * TIMER_TICKS_PER_US) && this->store_.ready != nullptr) {
        this->frames.emplace_back(this->store_.ready, this->store_.ready + this->store_.ready_count);
        this->store_.ready = nullptr;
      }
    }
  }
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

}  // namespace esphome::remote_receiver::testing
