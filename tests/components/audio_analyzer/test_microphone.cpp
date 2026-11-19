#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "esphome/components/audio_analyzer/microphone_audio_analyzer.h"
#include "fake_sample_buffer.h"
#include "test_helpers.h"

// MicrophoneSource's own .cpp needs esp-audio-libs, which only builds for the ESP32 (tests/components/microphone
// leaves it out). These stand in for it with what the analyzer relies on: start/stop the microphone unless passive,
// and pass 16-bit mono audio through unchanged.
namespace esphome::microphone {
void MicrophoneSource::start() {
  if (!this->enabled_ && !this->passive_) {
    this->enabled_ = true;
    this->mic_->start();
  }
}
void MicrophoneSource::stop() {
  if (this->enabled_ && !this->passive_) {
    this->enabled_ = false;
    this->mic_->stop();
  }
}
void MicrophoneSource::process_audio_(std::span<const uint8_t> data, const audio::AudioStreamInfo &source_info,
                                      std::vector<uint8_t> &filtered_data) {
  filtered_data.assign(data.begin(), data.end());
}
}  // namespace esphome::microphone

namespace esphome::audio_analyzer::testing {

/// A microphone that runs as soon as it is started and delivers what the test captures.
class FakeMicrophone : public microphone::Microphone {
 public:
  void start() override {
    ++this->starts;
    this->state_ = microphone::STATE_RUNNING;
  }
  void stop() override {
    ++this->stops;
    this->state_ = microphone::STATE_STOPPED;
  }
  /// Delivers a 1 kHz tone, continuing the previous one
  void capture(uint32_t frames, uint32_t rate = 16000) {
    std::vector<uint8_t> pcm = pcm_sine(frames, 1000.0, rate, 0.5f, 1, this->captured_);
    this->captured_ += frames;
    this->publish_audio_(pcm, audio::AudioStreamInfo(16, 1, rate));
  }

  int starts{0};
  int stops{0};

 protected:
  uint32_t captured_{0};
};

class AudioAnalyzerMicrophoneTest : public ::testing::Test {
 protected:
  void make_(bool passive, uint32_t buffer_samples = 4 * FFT_SIZE) {
    // Fresh ones: the old microphone holds the old analyzer's data callback
    this->analyzer_.reset();
    this->source_.reset();
    this->mic_ = std::make_unique<FakeMicrophone>();
    this->source_ = std::make_unique<microphone::MicrophoneSource>(this->mic_.get(), 16, 1, passive);
    this->source_->add_channel(0);
    this->analyzer_ = std::make_unique<MicrophoneAudioAnalyzer>(this->source_.get(), 16000);
    this->analyzer_->set_band_plan(16000.0f, DEFAULT_PLAN);
    this->analyzer_->set_smoothing(1.0f, 1.0f);
    this->analyzer_->set_buffer_samples(buffer_samples);
    this->analyzer_->add_on_frame_callback([this]() { ++this->frames_; });
    this->analyzer_->setup();
  }
  void loop_at_(uint32_t now) {
    this->now_ = now;
    at(now);
    this->analyzer_->loop();
  }
  void loop_() { this->loop_at_(this->now_ + 16); }

  std::unique_ptr<FakeMicrophone> mic_;
  std::unique_ptr<microphone::MicrophoneSource> source_;
  std::unique_ptr<MicrophoneAudioAnalyzer> analyzer_;
  uint32_t now_{1000};
  int frames_{0};
};

TEST_F(AudioAnalyzerMicrophoneTest, StartsTheMicrophoneUnlessPassive) {
  this->make_(false);
  EXPECT_EQ(this->mic_->starts, 1);
  this->make_(true);
  EXPECT_EQ(this->mic_->starts, 0);
}

TEST_F(AudioAnalyzerMicrophoneTest, FailsWithoutItsBuffer) {
  FakeSampleBuffer::fail_next_create = true;
  this->make_(false);
  EXPECT_TRUE(this->analyzer_->is_failed());
  EXPECT_EQ(this->mic_->starts, 0);
}

TEST_F(AudioAnalyzerMicrophoneTest, AnalyzesAudioOfTheCurrentRunOnly) {
  this->make_(false);
  this->loop_();  // new run
  this->mic_->capture(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
  EXPECT_TRUE(this->analyzer_->is_active());
  EXPECT_NEAR(this->analyzer_->get_rms_db(), mean_square_db(0.5f), 0.1f);

  // Audio captured just before the microphone stopped is left over; the next run doesn't analyze it
  this->mic_->capture(FFT_SIZE / 2);
  this->analyzer_->stop_analysis();
  this->loop_();
  this->analyzer_->start_analysis();
  this->loop_();
  this->mic_->capture(FFT_SIZE / 2);  // the copy restarts after this capture
  this->loop_();
  this->mic_->capture(FFT_SIZE / 2);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
  this->mic_->capture(FFT_SIZE / 2);
  this->loop_();
  EXPECT_EQ(this->frames_, 2);
}

TEST_F(AudioAnalyzerMicrophoneTest, PassiveAnalyzesWhileTheMicrophoneRuns) {
  this->make_(true);
  this->mic_->capture(FFT_SIZE);  // not running: ignored
  this->loop_();
  this->mic_->start();  // started by another component
  this->loop_();
  this->mic_->capture(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerMicrophoneTest, StartAndStopControlTheMicrophoneUnlessPassive) {
  this->make_(false);
  this->analyzer_->stop_analysis();
  EXPECT_EQ(this->mic_->stops, 1);
  this->analyzer_->start_analysis();
  EXPECT_EQ(this->mic_->starts, 2);

  WarningLog log;
  this->make_(true);
  this->mic_->start();
  this->analyzer_->stop_analysis();
  this->analyzer_->start_analysis();
  EXPECT_EQ(this->mic_->stops, 0);
  EXPECT_EQ(this->mic_->starts, 1);
  EXPECT_EQ(log.count("passive mode"), 2);
}

TEST_F(AudioAnalyzerMicrophoneTest, OtherFormatIsNotAnalyzedAndWarnsOncePerRun) {
  WarningLog log;
  this->make_(false);
  this->loop_();
  this->mic_->capture(FFT_SIZE, 8000);
  this->loop_();
  this->mic_->capture(FFT_SIZE, 8000);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);
  EXPECT_EQ(log.count("isn't 16-bit mono at 16000 Hz"), 1);
  this->analyzer_->stop_analysis();
  this->loop_();
  this->analyzer_->start_analysis();
  this->mic_->capture(FFT_SIZE, 8000);
  this->loop_();
  EXPECT_EQ(log.count("isn't 16-bit mono at 16000 Hz"), 2);
}

TEST_F(AudioAnalyzerMicrophoneTest, OverflowWarnsUntilASecondWithoutOne) {
  WarningLog log;
  this->make_(false, FFT_SIZE);
  this->loop_at_(1000);
  this->mic_->capture(FFT_SIZE);
  this->loop_at_(1016);
  EXPECT_FALSE(this->analyzer_->status_has_warning());
  this->mic_->capture(3 * FFT_SIZE);  // the main loop stalled
  this->loop_at_(1300);
  EXPECT_TRUE(this->analyzer_->status_has_warning());
  EXPECT_EQ(log.count("fell behind the microphone"), 1);
  this->mic_->capture(FFT_SIZE / 2);
  this->loop_at_(2000);
  EXPECT_TRUE(this->analyzer_->status_has_warning());
  this->loop_at_(2301);
  EXPECT_FALSE(this->analyzer_->status_has_warning());
}

TEST_F(AudioAnalyzerMicrophoneTest, StoppingClearsTheWarning) {
  this->make_(false, FFT_SIZE);
  this->loop_();
  this->mic_->capture(3 * FFT_SIZE);
  this->loop_();
  ASSERT_TRUE(this->analyzer_->status_has_warning());
  this->analyzer_->stop_analysis();
  this->loop_();
  EXPECT_FALSE(this->analyzer_->status_has_warning());
}

TEST_F(AudioAnalyzerMicrophoneTest, SleepsOnceStoppedAndReleased) {
  this->make_(false);
  this->analyzer_->set_smoothing(1.0f, 0.5f);
  this->loop_at_(1000);
  this->mic_->capture(FFT_SIZE);
  this->loop_at_(1000);
  // Stopped but still active, then releasing: the loop keeps running
  this->analyzer_->stop_analysis();
  this->loop_at_(1016);
  EXPECT_FALSE(this->analyzer_->is_idle());
  for (int i = 0; i < 4; ++i) {
    this->loop_();
  }
  EXPECT_FALSE(this->analyzer_->is_idle());
  for (int i = 0; i < 100 && !this->analyzer_->is_idle(); ++i) {
    this->loop_();
  }
  EXPECT_TRUE(this->analyzer_->is_idle());
  EXPECT_FLOAT_EQ(this->analyzer_->get_rms_level(), 0.0f);
}

}  // namespace esphome::audio_analyzer::testing
