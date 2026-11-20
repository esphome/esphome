#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "esphome/components/audio_analyzer/speaker/audio_analyzer_speaker.h"
#include "../test_helpers.h"

namespace esphome::audio_analyzer::testing {

/// Output speaker the test drives, reports played audio when told and takes the state it is given. Like i2s_audio,
/// it accepts audio only while running, requests its own start from play(), and ignores start() while starting or
/// running.
class FakeOutput : public speaker::Speaker {
 public:
  size_t play(const uint8_t *data, size_t length) override {
    if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
      this->start();
    }
    if (this->state_ != speaker::STATE_RUNNING) {
      return 0;
    }
    const size_t accepted = std::min(length, this->accept);
    this->bytes += accepted;
    return accepted;
  }
  void start() override {
    if (this->ignore_starts || this->state_ == speaker::STATE_STARTING || this->state_ == speaker::STATE_RUNNING) {
      return;
    }
    this->start_requested = true;
  }
  void stop() override { ++this->stops; }
  void finish() override { ++this->finishes; }
  bool has_buffered_data() const override { return this->buffered; }
  void set_pause_state(bool pause_state) override { this->paused = pause_state; }
  bool get_pause_state() const override { return this->paused; }

  /// A state the output's loop() moved to; starting takes the start request
  void set_state(speaker::State state) {
    this->state_ = state;
    if (state == speaker::STATE_STARTING || state == speaker::STATE_RUNNING) {
      this->start_requested = false;
    }
  }
  void report_played(uint32_t frames) { this->audio_output_callback_(frames, 0); }

  size_t accept{SIZE_MAX};
  size_t bytes{0};
  int stops{0};
  int finishes{0};
  bool start_requested{false};
  /// Like an output with an error status or a refused stream format
  bool ignore_starts{false};
  bool buffered{false};
  bool paused{false};
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
  EXPECT_TRUE(this->output_->start_requested);
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
  EXPECT_TRUE(this->output_->start_requested);
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

TEST_F(AudioAnalyzerSpeakerTest, QuickRestartFollowsTheOutputThroughStopped) {
  // stop() then start() while the output is still stopping: it keeps the start and passes through STOPPED
  this->run_stream_();
  this->speaker_->stop();
  this->output_->set_state(speaker::STATE_STOPPING);
  this->loop_();
  this->speaker_->start();
  EXPECT_TRUE(this->output_->start_requested);
  for (speaker::State output : {speaker::STATE_STOPPING, speaker::STATE_STOPPED, speaker::STATE_STARTING}) {
    this->output_->set_state(output);
    this->loop_();
    EXPECT_FALSE(this->speaker_->is_stopped()) << "output state " << output;
    EXPECT_FALSE(this->speaker_->is_running()) << "output state " << output;
  }
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
}

TEST_F(AudioAnalyzerSpeakerTest, RestartBeforeTheOutputStoppedIgnoresTheOldStream) {
  // stop() and start() before any loop(): the output ignores the start while running, stops, then restarts when the
  // producer plays again
  this->run_stream_();
  this->speaker_->stop();
  this->speaker_->start();
  EXPECT_FALSE(this->output_->start_requested);
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_running());  // the output still runs the old stream
  this->output_->set_state(speaker::STATE_STOPPING);
  this->loop_();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  EXPECT_FALSE(this->speaker_->is_stopped());
  EXPECT_EQ(this->play_tone_(FFT_SIZE), 0u);
  EXPECT_TRUE(this->output_->start_requested);
  this->output_->set_state(speaker::STATE_STARTING);
  this->loop_();
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  EXPECT_TRUE(this->speaker_->is_running());
}

TEST_F(AudioAnalyzerSpeakerTest, RestartStopsWaitingForAnOutputThatNeverStops) {
  this->run_stream_();
  this->speaker_->stop();
  this->speaker_->start();
  this->loop_at_(2000);
  this->loop_at_(6900);
  EXPECT_FALSE(this->speaker_->is_running());
  this->loop_at_(7001);
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
  WarningLog log;
  this->output_->ignore_starts = true;
  this->speaker_->start();
  this->loop_at_(1000);
  this->loop_at_(1900);
  EXPECT_FALSE(this->speaker_->is_stopped());
  this->loop_at_(2001);
  EXPECT_TRUE(this->speaker_->is_stopped());
  EXPECT_EQ(this->output_->stops, 0);
  // Warned once, not on every retry, until the output runs again
  this->speaker_->start();
  this->loop_at_(2100);
  this->loop_at_(3200);
  EXPECT_TRUE(this->speaker_->is_stopped());
  EXPECT_EQ(log.count("did not start"), 1);
  this->output_->ignore_starts = false;
  this->run_stream_();
  this->speaker_->stop();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  this->output_->ignore_starts = true;
  this->speaker_->start();
  this->loop_();
  this->loop_at_(this->now_ + 1001);
  EXPECT_EQ(log.count("did not start"), 2);
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
  this->run_stream_();
  this->play_tone_(FFT_SIZE);  // only acknowledges the restart: counted, not copied
  this->loop_();
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(2 * FFT_SIZE);
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
  this->loop_();
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(this->frames_, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, OtherRateIsNotAnalyzedAndWarnsOncePerStream) {
  WarningLog log;
  this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, 1, 8000));
  this->run_stream_();
  for (int i = 0; i < 2; ++i) {
    this->play_tone_(FFT_SIZE);
    this->output_->report_played(FFT_SIZE);
    this->loop_();
  }
  EXPECT_EQ(this->frames_, 0);
  EXPECT_EQ(log.count("Not analyzing a 8000 Hz stream"), 1);
  this->speaker_->stop();
  this->output_->set_state(speaker::STATE_STOPPED);
  this->loop_();
  this->run_stream_();
  this->play_tone_(FFT_SIZE);
  this->loop_();
  EXPECT_EQ(log.count("Not analyzing a 8000 Hz stream"), 2);
}

TEST_F(AudioAnalyzerSpeakerTest, WarnsOnceWhenTheOutputNeverReportsPlayedAudio) {
  WarningLog log;
  this->run_stream_();
  this->play_tone_(FFT_SIZE);
  this->loop_at_(this->now_ + 1000);
  // Paused before its first report: the output plays silence and reports nothing, which is no reason to warn
  this->speaker_->set_pause_state(true);
  this->loop_at_(this->now_ + 3000);
  EXPECT_EQ(log.count("doesn't report"), 0);
  this->speaker_->set_pause_state(false);
  this->loop_();
  this->loop_at_(this->now_ + 1900);
  EXPECT_EQ(log.count("doesn't report"), 0);
  this->loop_at_(this->now_ + 200);
  this->loop_at_(this->now_ + 3000);
  EXPECT_EQ(log.count("doesn't report"), 1);
}

TEST_F(AudioAnalyzerSpeakerTest, NoWarningWhenTheOutputReports) {
  WarningLog log;
  this->run_stream_();
  this->play_tone_(FFT_SIZE);
  this->output_->report_played(16);
  this->loop_at_(this->now_ + 5000);
  EXPECT_EQ(log.count("doesn't report"), 0);
}

TEST_F(AudioAnalyzerSpeakerTest, ForwardsToTheOutput) {
  this->speaker_->set_pause_state(true);
  EXPECT_TRUE(this->output_->paused);
  EXPECT_TRUE(this->speaker_->get_pause_state());
  this->speaker_->set_volume(0.25f);
  EXPECT_FLOAT_EQ(this->output_->get_volume(), 0.25f);
  EXPECT_FLOAT_EQ(this->speaker_->get_volume(), 0.25f);
  this->speaker_->set_mute_state(true);
  EXPECT_TRUE(this->output_->get_mute_state());
  EXPECT_TRUE(this->speaker_->get_mute_state());
  EXPECT_FALSE(this->speaker_->has_buffered_data());
  this->output_->buffered = true;
  EXPECT_TRUE(this->speaker_->has_buffered_data());
  this->speaker_->finish();
  EXPECT_EQ(this->output_->finishes, 1);
}

TEST_F(AudioAnalyzerSpeakerTest, StreamFormatForwardedOnStartAndPlay) {
  const audio::AudioStreamInfo mono(16, 1, RATE);
  const audio::AudioStreamInfo stereo(16, 2, RATE);
  this->speaker_->start();
  EXPECT_EQ(this->output_->get_audio_stream_info(), mono);
  this->loop_();
  this->output_->set_state(speaker::STATE_RUNNING);
  this->loop_();
  // A producer may change the format of a running stream before its next play()
  this->speaker_->set_audio_stream_info(stereo);
  const std::vector<uint8_t> pcm = pcm_sine(16, 1000.0, RATE, 0.5f, 2);
  this->speaker_->play(pcm.data(), pcm.size());
  EXPECT_EQ(this->output_->get_audio_stream_info(), stereo);
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
