#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "esphome/components/audio_analyzer/speaker/audio_analyzer_speaker.h"
#include "../test_helpers.h"

namespace esphome::audio_analyzer::testing {

/// Output speaker the test drives: accepts bytes, reports them played when told, and takes the state it is given.
class FakeOutput : public speaker::Speaker {
 public:
  size_t play(const uint8_t *data, size_t length) override {
    const size_t accepted = std::min(length, this->accept);
    this->bytes += accepted;
    return accepted;
  }
  void start() override { ++this->starts; }
  void stop() override { ++this->stops; }
  void finish() override { ++this->finishes; }
  bool has_buffered_data() const override { return false; }

  void set_state(speaker::State state) { this->state_ = state; }
  void report_played(uint32_t frames) { this->audio_output_callback_(frames, 0); }

  size_t accept{SIZE_MAX};
  size_t bytes{0};
  int starts{0};
  int stops{0};
  int finishes{0};
};

static constexpr uint32_t RATE = 16000;

class AudioAnalyzerSpeakerTest : public ::testing::Test {
 protected:
  void SetUp() override { this->make_(RATE, 4 * FFT_SIZE); }

  void make_(uint32_t rate, uint32_t buffer_samples) {
    // A fresh output too: the old one holds the old speaker's played-audio callback
    this->output_ = std::make_unique<FakeOutput>();
    this->speaker_ = std::make_unique<AudioAnalyzerSpeaker>(this->output_.get(), rate);
    this->speaker_->set_band_plan(16000.0f, DEFAULT_PLAN);
    this->speaker_->set_smoothing(1.0f, 1.0f);
    this->speaker_->set_buffer_samples(buffer_samples);
    if (rate > 16000) {
      this->speaker_->set_decimation(rate / 16000, reference_sections(rate / 16000));
    }
    this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, 1, rate));
    this->speaker_->setup();
    this->speaker_->add_on_frame_callback([this]() { ++this->frames_; });
    this->rate_ = rate;
  }
  void loop_at_(uint32_t now) {
    this->now_ = now;
    at(now);
    this->speaker_->loop();
  }
  void loop_() { this->loop_at_(this->now_ + 16); }
  /// Plays a 1 kHz tone, continuing the previous one
  size_t play_tone_(uint32_t frames) {
    const std::vector<uint8_t> pcm = pcm_sine(frames, 1000.0, this->rate_, 0.5f, 1, this->played_);
    const size_t written = this->speaker_->play(pcm.data(), pcm.size());
    this->played_ += written / 2;
    return written;
  }
  /// Starts a stream and runs it until the speaker follows a running output
  void run_stream_() {
    this->speaker_->start();
    this->loop_();
    this->output_->set_state(speaker::STATE_RUNNING);
    this->loop_();
    ASSERT_TRUE(this->speaker_->is_running());
  }

  std::unique_ptr<FakeOutput> output_;
  std::unique_ptr<AudioAnalyzerSpeaker> speaker_;
  uint32_t rate_{RATE};
  uint32_t now_{1000};
  uint32_t played_{0};
  int frames_{0};
};

TEST_F(AudioAnalyzerSpeakerTest, StartIsForwardedAtOnceAndTakenByLoop) {
  this->speaker_->start();
  EXPECT_EQ(this->output_->starts, 1);
  EXPECT_TRUE(this->speaker_->is_stopped());  // only loop() moves the state
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_stopped());
  EXPECT_FALSE(this->speaker_->is_running());
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
}

TEST_F(AudioAnalyzerSpeakerTest, StopWhileIdleThenPlay) {
  // A cancelled announcement stops a speaker that never played: it stays stopped, so the next play() starts it
  this->speaker_->stop();
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_stopped());
  this->play_tone_(100);
  EXPECT_EQ(this->output_->starts, 1);
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_stopped());
}

TEST_F(AudioAnalyzerSpeakerTest, FinishRunsUntilTheOutputStopped) {
  this->run_stream_();
  this->speaker_->finish();
  EXPECT_EQ(this->output_->finishes, 1);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
  this->output_->set_state(speaker::STATE_STOPPING);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_stopped());
}

TEST_F(AudioAnalyzerSpeakerTest, StopFollowsTheOutputToStopped) {
  this->run_stream_();
  this->speaker_->stop();
  EXPECT_EQ(this->output_->stops, 1);
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_running());
  EXPECT_FALSE(this->speaker_->is_stopped());
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_stopped());
}

TEST_F(AudioAnalyzerSpeakerTest, StopThenStartBeforeLoopStartsANewStream) {
  this->run_stream_();
  this->speaker_->stop();
  this->speaker_->start();
  this->output_->set_state(speaker::STATE_STOPPING);
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_running());
  EXPECT_FALSE(this->speaker_->is_stopped());
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
}

TEST_F(AudioAnalyzerSpeakerTest, StartThenStopBeforeLoopStaysStopped) {
  this->speaker_->start();
  this->speaker_->stop();
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_stopped());
}

TEST_F(AudioAnalyzerSpeakerTest, ShortStreamEndsTheStart) {
  // The output started and stopped again before reporting running (a short clip, or a refused format)
  this->speaker_->start();
  this->loop_();
  this->output_->set_state(speaker::STATE_STARTING);
  this->loop_();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_stopped());
  EXPECT_EQ(this->output_->stops, 0);
}

TEST_F(AudioAnalyzerSpeakerTest, OutputThatNeverStartsEndsTheStartWithoutAStop) {
  this->speaker_->start();
  this->loop_at_(1000);
  this->loop_at_(1900);
  EXPECT_FALSE(this->speaker_->is_stopped());
  this->loop_at_(2001);
  EXPECT_TRUE(this->speaker_->is_stopped());
  EXPECT_EQ(this->output_->stops, 0);
}

TEST_F(AudioAnalyzerSpeakerTest, SlowOutputStartIsWaitedFor) {
  // For example an I2S speaker waiting for its bus: no timeout while the output is starting
  this->speaker_->start();
  this->loop_at_(1000);
  this->output_->set_state(speaker::STATE_STARTING);
  this->loop_at_(9000);
  EXPECT_FALSE(this->speaker_->is_stopped());
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_at_(9016);
  EXPECT_TRUE(this->speaker_->is_running());
}

TEST_F(AudioAnalyzerSpeakerTest, AnalyzesOnlyWhatWasPlayed) {
  this->run_stream_();
  ASSERT_EQ(this->play_tone_(2 * FFT_SIZE), 2 * FFT_SIZE * 2u);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);  // still in the output
  EXPECT_FALSE(this->speaker_->is_active());
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
  EXPECT_NEAR(this->speaker_->get_rms_db(), mean_square_db(0.5f), 0.1f);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 2);
}

TEST_F(AudioAnalyzerSpeakerTest, PlayedFramesAreForwarded) {
  uint32_t forwarded = 0;
  this->speaker_->add_audio_output_callback([&forwarded](uint32_t frames, int64_t) { forwarded += frames; });
  this->output_->report_played(123);
  EXPECT_EQ(forwarded, 123u);
}

TEST_F(AudioAnalyzerSpeakerTest, DecimatedStreamCountsInputFrames) {
  this->make_(32000, 4 * FFT_SIZE);
  this->run_stream_();
  this->play_tone_(4 * FFT_SIZE);  // two analysis frames at 16 kHz
  this->output_->report_played(2 * FFT_SIZE - 2);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);  // the last sample of the first frame is not heard yet
  this->output_->report_played(2);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
  EXPECT_NEAR(this->speaker_->get_rms_db(), mean_square_db(0.5f), 0.2f);
}

TEST_F(AudioAnalyzerSpeakerTest, WritesEndingMidFrameKeepTheStreamAligned) {
  this->output_->accept = 301;  // odd byte counts: every write ends mid-frame
  this->run_stream_();
  const std::vector<uint8_t> stream = pcm_sine(2 * FFT_SIZE, 1000.0, RATE, 0.5f);
  size_t total = 0;
  while (total < stream.size()) {
    // Continue from the byte the output stopped at, like a producer does
    total += this->speaker_->play(stream.data() + total, stream.size() - total);
  }
  this->output_->report_played(2 * FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 2);
  EXPECT_NEAR(this->speaker_->get_rms_db(), mean_square_db(0.5f), 0.1f);
}

TEST_F(AudioAnalyzerSpeakerTest, NewStreamDropsTheUnheardAudio) {
  this->run_stream_();
  this->play_tone_(FFT_SIZE);
  this->speaker_->stop();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  ASSERT_TRUE(this->speaker_->is_stopped());
  // The output dropped the unplayed audio and never reports it; the new stream doesn't wait for it
  this->play_tone_(FFT_SIZE);
  this->loop_();
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);  // the first play() of the stream came before loop() drained the old audio
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, OverflowWarnsThenClearsWhenStopped) {
  this->make_(RATE, FFT_SIZE);
  this->run_stream_();
  this->play_tone_(3 * FFT_SIZE);  // more than the buffer, nothing played yet
  this->loop_();
  EXPECT_TRUE(this->speaker_->status_has_warning());
  this->speaker_->stop();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  EXPECT_FALSE(this->speaker_->status_has_warning());
}

TEST_F(AudioAnalyzerSpeakerTest, OverflowHoldsTheAnalysisUntilTheUncopiedAudioPlayed) {
  this->make_(RATE, FFT_SIZE);
  this->run_stream_();
  this->play_tone_(3 * FFT_SIZE);
  this->loop_();  // drains after the overflow
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(3 * FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, SkipsAheadAfterAStall) {
  this->make_(RATE, 8 * FFT_SIZE);
  this->run_stream_();
  this->play_tone_(5 * FFT_SIZE);
  this->output_->report_played(5 * FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, MutedOutputAnalyzesSilence) {
  this->run_stream_();
  this->output_->set_mute_state(true);
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
  EXPECT_FLOAT_EQ(this->speaker_->get_rms_db(), FLOOR_DB);
}

TEST_F(AudioAnalyzerSpeakerTest, StopAnalysisKeepsAudioPassingThrough) {
  this->run_stream_();
  this->speaker_->stop_analysis();
  EXPECT_EQ(this->play_tone_(FFT_SIZE), FFT_SIZE * 2u);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);
  this->speaker_->start_analysis();
  this->loop_();  // drops what was buffered before
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, OtherRateIsNotAnalyzed) {
  this->run_stream_();
  this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, 1, 8000));
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 0);
}

TEST_F(AudioAnalyzerSpeakerTest, SleepsOnceStoppedAndReleased) {
  this->run_stream_();
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  ASSERT_TRUE(this->speaker_->is_stopped());
  EXPECT_FALSE(this->speaker_->is_idle());  // values not released yet
  for (int i = 0; i < 10; ++i) {
    this->loop_();
  }
  EXPECT_TRUE(this->speaker_->is_idle());
  EXPECT_FALSE(this->speaker_->is_active());
}

}  // namespace esphome::audio_analyzer::testing
