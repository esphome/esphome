#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "esphome/core/application.h"

namespace esphome::audio_analyzer::testing {

static constexpr double TWO_PI = 6.283185307179586;

// Exposes the protected glue a platform calls from loop()
class TestAnalyzer : public AudioAnalyzer {
 public:
  TestAnalyzer() {
    this->set_attack(0);
    this->set_release(250);
    this->configure_analysis_(16000.0f);
  }
  uint32_t feed_frames(uint32_t frames) {
    uint32_t completed = 0;
    for (uint32_t i = 0; i < frames * FFT_SIZE; ++i) {
      completed += this->push_sample_(0.5f * static_cast<float>(std::sin(TWO_PI * 1000.0 * i / 16000.0)));
    }
    return completed;
  }
  void release_if_idle() { this->release_if_idle_(); }
};

static void at(uint32_t now_ms) { LoopBlockingGuard clock{nullptr, nullptr, now_ms}; }

TEST(AudioAnalyzerGlue, FramesCallBackAndActivate) {
  TestAnalyzer analyzer;
  int calls = 0;
  analyzer.add_on_frame_callback([&calls]() { ++calls; });
  at(1000);
  EXPECT_EQ(analyzer.feed_frames(2), 2u);
  EXPECT_EQ(calls, 2);
  EXPECT_TRUE(analyzer.is_active());
  EXPECT_NEAR(analyzer.get_rms_db(), -9.03f, 0.1f);
}

TEST(AudioAnalyzerGlue, ReleasesOneFrameAtATimeAfterTwoIdleFrames) {
  TestAnalyzer analyzer;
  at(1000);
  analyzer.feed_frames(1);
  const float level = analyzer.get_rms_db();

  // A frame is 32 ms: nothing happens before two frame periods without audio
  at(1000 + 63);
  analyzer.release_if_idle();
  EXPECT_TRUE(analyzer.is_active());
  EXPECT_FLOAT_EQ(analyzer.get_rms_db(), level);

  at(1000 + 64);
  analyzer.release_if_idle();
  EXPECT_FALSE(analyzer.is_active());
  const float one_step = analyzer.get_rms_db();
  EXPECT_LT(one_step, level);

  // The next step is one frame later, not on every loop
  at(1000 + 64 + 16);
  analyzer.release_if_idle();
  EXPECT_FLOAT_EQ(analyzer.get_rms_db(), one_step);
  at(1000 + 64 + 32);
  analyzer.release_if_idle();
  EXPECT_LT(analyzer.get_rms_db(), one_step);
}

TEST(AudioAnalyzerGlue, ReleasesToSilence) {
  TestAnalyzer analyzer;
  at(1000);
  analyzer.feed_frames(1);
  uint32_t now = 1000;
  for (int i = 0; i < 500; ++i) {
    now += 16;
    at(now);
    analyzer.release_if_idle();
  }
  EXPECT_NEAR(analyzer.get_rms_db(), FLOOR_DB, 0.1f);
  EXPECT_FLOAT_EQ(analyzer.get_rms_level(), 0.0f);
  EXPECT_FALSE(analyzer.is_active());
}

}  // namespace esphome::audio_analyzer::testing
