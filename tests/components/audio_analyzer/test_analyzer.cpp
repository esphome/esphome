#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "test_helpers.h"

namespace esphome::audio_analyzer::testing {

// Exposes the protected glue a platform calls from loop()
class TestAnalyzer : public AudioAnalyzer {
 public:
  explicit TestAnalyzer(uint32_t buffer_samples = 4 * FFT_SIZE) {
    this->set_band_plan(16000.0f, DEFAULT_PLAN);
    this->set_smoothing(1.0f, 0.12f);
    this->tap_.allocate(buffer_samples);
  }
  /// Producer side: a 1 kHz tone at 16 kHz
  void write_tone(uint32_t frames, float amplitude = 0.5f) {
    const std::vector<uint8_t> pcm = pcm_sine(frames, 1000.0, 16000, amplitude, 1, this->written_);
    this->written_ += frames;
    this->tap_.write(pcm.data(), frames, 2, 1);
  }
  /// What a platform's loop() does with no limit on what is due
  void analyze_all(uint32_t now) {
    this->analyze_(this->buffered_samples_(), false);
    this->overflow_raised = this->update_overflow_warning_(now);
    this->release_if_idle_(now);
  }
  void start_analysis() override {}
  void stop_analysis() override {}
  uint32_t buffered() { return this->buffered_samples_(); }
  void restart() { this->tap_.restart(); }
  void analyze(uint32_t due, bool silent) { this->analyze_(due, silent); }
  void release_if_idle(uint32_t now) { this->release_if_idle_(now); }
  bool overflow_raised{false};

 protected:
  uint32_t written_{0};
};

TEST(AudioAnalyzerGlue, AnalyzesWhatTheProducerCopied) {
  TestAnalyzer analyzer;
  int frames = 0;
  analyzer.add_on_frame_callback([&frames]() { ++frames; });
  at(1000);
  analyzer.analyze_all(1000);  // first drain lets the producer start
  analyzer.write_tone(2 * FFT_SIZE);
  analyzer.analyze_all(1000);
  EXPECT_EQ(frames, 2);
  EXPECT_TRUE(analyzer.is_active());
  EXPECT_NEAR(analyzer.get_rms_db(), mean_square_db(0.5f), 0.1f);
}

TEST(AudioAnalyzerGlue, SilentAnalyzesSilence) {
  TestAnalyzer analyzer;
  at(1000);
  analyzer.buffered();
  analyzer.write_tone(FFT_SIZE);
  analyzer.analyze(analyzer.buffered(), true);
  EXPECT_TRUE(analyzer.is_active());
  EXPECT_FLOAT_EQ(analyzer.get_rms_db(), FLOOR_DB);
}

TEST(AudioAnalyzerGlue, SkipsToTheNewestFrameWhenBehind) {
  TestAnalyzer analyzer(8 * FFT_SIZE);
  int frames = 0;
  analyzer.add_on_frame_callback([&frames]() { ++frames; });
  at(1000);
  analyzer.buffered();
  analyzer.write_tone(FFT_SIZE / 2);
  analyzer.analyze(analyzer.buffered(), false);  // a partial frame, dropped by the skip
  analyzer.write_tone(5 * FFT_SIZE);
  analyzer.analyze(analyzer.buffered(), false);
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(analyzer.buffered(), 0u);
}

TEST(AudioAnalyzerGlue, OverflowWarningRaisedOnceAndClearedAfterASecond) {
  TestAnalyzer analyzer(FFT_SIZE);
  at(1000);
  analyzer.analyze_all(1000);
  analyzer.write_tone(2 * FFT_SIZE);  // twice the buffer
  analyzer.analyze_all(1000);
  EXPECT_TRUE(analyzer.overflow_raised);
  EXPECT_TRUE(analyzer.status_has_warning());
  // The copy restarts after the drain; another overflow keeps the warning without raising it again
  analyzer.write_tone(2 * FFT_SIZE);
  at(1500);
  analyzer.analyze_all(1500);
  EXPECT_FALSE(analyzer.overflow_raised);
  EXPECT_TRUE(analyzer.status_has_warning());
  at(2400);
  analyzer.analyze_all(2400);
  EXPECT_TRUE(analyzer.status_has_warning());
  at(2501);
  analyzer.analyze_all(2501);
  EXPECT_FALSE(analyzer.status_has_warning());
}

TEST(AudioAnalyzerGlue, RestartDropsTheBufferAndThePartialFrame) {
  TestAnalyzer analyzer;
  int frames = 0;
  analyzer.add_on_frame_callback([&frames]() { ++frames; });
  at(1000);
  analyzer.buffered();
  analyzer.write_tone(FFT_SIZE - 10);
  analyzer.analyze(analyzer.buffered(), false);
  analyzer.write_tone(20);
  analyzer.restart();
  EXPECT_EQ(analyzer.buffered(), 0u);
  // Without the drop, 10 more samples would complete the old frame
  analyzer.write_tone(10);
  analyzer.analyze(analyzer.buffered(), false);
  EXPECT_EQ(frames, 0);
}

TEST(AudioAnalyzerGlue, ReleasesOneFrameAtATimeAfterTwoIdleFrames) {
  TestAnalyzer analyzer;
  at(1000);
  analyzer.buffered();
  analyzer.write_tone(FFT_SIZE);
  analyzer.analyze(analyzer.buffered(), false);
  const float level = analyzer.get_rms_db();

  // A frame is 32 ms: nothing happens before two frame periods without audio
  analyzer.release_if_idle(1000 + 63);
  EXPECT_TRUE(analyzer.is_active());
  EXPECT_FLOAT_EQ(analyzer.get_rms_db(), level);

  analyzer.release_if_idle(1000 + 64);
  EXPECT_FALSE(analyzer.is_active());
  const float one_step = analyzer.get_rms_db();
  EXPECT_LT(one_step, level);

  // The next step is one frame later, not on every loop
  analyzer.release_if_idle(1000 + 64 + 16);
  EXPECT_FLOAT_EQ(analyzer.get_rms_db(), one_step);
  analyzer.release_if_idle(1000 + 64 + 32);
  EXPECT_LT(analyzer.get_rms_db(), one_step);
}

TEST(AudioAnalyzerGlue, ALateLoopCatchesUpTheRelease) {
  // Two analyzers with the same frame; one is released on time, the other after a 100 ms stall
  TestAnalyzer on_time;
  TestAnalyzer late;
  at(1000);
  for (TestAnalyzer *a : {&on_time, &late}) {
    a->buffered();
    a->write_tone(FFT_SIZE);
    a->analyze(a->buffered(), false);
  }
  for (uint32_t now = 1000; now <= 1000 + 5 * 32; now += 16) {
    on_time.release_if_idle(now);
  }
  late.release_if_idle(1000 + 5 * 32);
  EXPECT_FLOAT_EQ(late.get_rms_db(), on_time.get_rms_db());
}

TEST(AudioAnalyzerGlue, ReleasesToSilence) {
  TestAnalyzer analyzer;
  at(1000);
  analyzer.buffered();
  analyzer.write_tone(FFT_SIZE);
  analyzer.analyze(analyzer.buffered(), false);
  for (uint32_t now = 1000; now < 1000 + 500 * 16; now += 16) {
    analyzer.release_if_idle(now);
  }
  EXPECT_NEAR(analyzer.get_rms_db(), FLOOR_DB, 0.1f);
  EXPECT_FLOAT_EQ(analyzer.get_rms_level(), 0.0f);
  EXPECT_FALSE(analyzer.is_active());
}

}  // namespace esphome::audio_analyzer::testing
