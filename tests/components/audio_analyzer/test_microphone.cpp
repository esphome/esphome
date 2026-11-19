#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/microphone_audio_analyzer.h"
#include "test_helpers.h"

namespace esphome::audio_analyzer::testing {

// The microphone analyzer's own logic; on the device MicrophoneAudioAnalyzer only adds the microphone calls
class TestMicrophone : public MicrophoneAnalyzerBase {
 public:
  explicit TestMicrophone(uint32_t buffer_samples = 4 * FFT_SIZE) {
    this->set_band_plan(16000.0f, DEFAULT_PLAN);
    this->set_smoothing(1.0f, 1.0f);
    this->tap_.allocate(buffer_samples);
  }
  /// What the microphone's data callback delivers
  void capture(uint32_t frames) {
    const std::vector<uint8_t> pcm = pcm_sine(frames, 1000.0, 16000, 0.5f, 1, this->captured_);
    this->captured_ += frames;
    this->on_audio_(pcm.data(), pcm.size());
  }
  void loop_at(uint32_t now, bool microphone_running) {
    at(now);
    this->run_(microphone_running);
  }
  bool allows_control(bool passive) const { return this->allows_control_(passive); }

 protected:
  uint32_t captured_{0};
};

TEST(AudioAnalyzerMicrophone, AnalyzesAudioOfTheCurrentRunOnly) {
  TestMicrophone mic;
  int frames = 0;
  mic.add_on_frame_callback([&frames]() { ++frames; });
  mic.loop_at(1000, true);  // new run
  mic.capture(FFT_SIZE);
  mic.loop_at(1016, true);
  EXPECT_EQ(frames, 1);
  EXPECT_TRUE(mic.is_active());
  EXPECT_NEAR(mic.get_rms_db(), mean_square_db(0.5f), 0.1f);

  // Audio captured just before the microphone stopped is left over; the next run doesn't analyze it
  mic.capture(FFT_SIZE / 2);
  mic.loop_at(1032, false);
  mic.loop_at(2000, true);
  mic.capture(FFT_SIZE / 2);
  mic.loop_at(2016, true);
  EXPECT_EQ(frames, 1);
  mic.capture(FFT_SIZE / 2);
  mic.loop_at(2032, true);
  EXPECT_EQ(frames, 2);
}

TEST(AudioAnalyzerMicrophone, OverflowWarnsUntilASecondWithoutOne) {
  TestMicrophone mic(FFT_SIZE);
  mic.loop_at(1000, true);
  mic.capture(FFT_SIZE);
  mic.loop_at(1016, true);
  EXPECT_FALSE(mic.status_has_warning());
  mic.capture(3 * FFT_SIZE);  // the main loop stalled
  mic.loop_at(1300, true);
  EXPECT_TRUE(mic.status_has_warning());
  mic.capture(FFT_SIZE / 2);
  mic.loop_at(2000, true);
  EXPECT_TRUE(mic.status_has_warning());
  mic.loop_at(2301, true);
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(AudioAnalyzerMicrophone, StoppingClearsTheWarning) {
  TestMicrophone mic(FFT_SIZE);
  mic.loop_at(1000, true);
  mic.capture(3 * FFT_SIZE);
  mic.loop_at(1016, true);
  ASSERT_TRUE(mic.status_has_warning());
  mic.loop_at(1032, false);
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(AudioAnalyzerMicrophone, SleepsOnceStoppedAndReleased) {
  TestMicrophone mic;
  mic.set_smoothing(1.0f, 0.5f);
  mic.loop_at(1000, true);
  mic.capture(FFT_SIZE);
  mic.loop_at(1000, true);
  // Stopped but still active, then releasing: the loop keeps running
  mic.loop_at(1016, false);
  EXPECT_FALSE(mic.is_idle());
  uint32_t now = 1016;
  while (now < 1016 + 64 && !mic.is_idle()) {
    now += 16;
    mic.loop_at(now, false);
  }
  EXPECT_FALSE(mic.is_idle());
  for (int i = 0; i < 100 && !mic.is_idle(); ++i) {
    now += 16;
    mic.loop_at(now, false);
  }
  EXPECT_TRUE(mic.is_idle());
  EXPECT_FLOAT_EQ(mic.get_rms_level(), 0.0f);
}

TEST(AudioAnalyzerMicrophone, NoControlInPassiveMode) {
  TestMicrophone mic;
  EXPECT_FALSE(mic.allows_control(true));
  EXPECT_TRUE(mic.allows_control(false));
}

}  // namespace esphome::audio_analyzer::testing
