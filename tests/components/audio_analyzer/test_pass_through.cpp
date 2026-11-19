#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/pass_through.h"

namespace esphome::audio_analyzer::testing {

using State = PassThroughState;
static constexpr State STOPPED = State::PASS_THROUGH_STATE_STOPPED;
static constexpr State STARTING = State::PASS_THROUGH_STATE_STARTING;
static constexpr State RUNNING = State::PASS_THROUGH_STATE_RUNNING;
static constexpr State STOPPING = State::PASS_THROUGH_STATE_STOPPING;

// Drives the state functions the way AudioAnalyzerSpeaker calls them
struct PassThrough {
  State state{STOPPED};
  bool new_stream{false};
  void start() {
    this->new_stream = starts_new_stream(this->state);
    if (this->new_stream) {
      this->state = STARTING;
    }
  }
  void play() {
    if (starts_new_stream(this->state)) {
      this->start();
    }
  }
  void stop() { this->state = state_after_stop(this->state); }
  void loop(bool output_running, bool output_stopped, bool timed_out = false) {
    this->state = follow_output(this->state, output_running, output_stopped, timed_out);
  }
};

TEST(AudioAnalyzerPassThrough, StopWhileIdleThenPlay) {
  // A cancelled announcement stops a speaker that never played: it stays stopped, so the next play() starts it
  PassThrough speaker;
  speaker.stop();
  EXPECT_EQ(speaker.state, STOPPED);
  speaker.loop(false, true);
  EXPECT_EQ(speaker.state, STOPPED);
  speaker.play();
  EXPECT_EQ(speaker.state, STARTING);
  EXPECT_TRUE(speaker.new_stream);
  speaker.loop(true, false);
  EXPECT_EQ(speaker.state, RUNNING);
}

TEST(AudioAnalyzerPassThrough, FinishRunsUntilTheOutputDrained) {
  PassThrough speaker;
  speaker.start();
  speaker.loop(true, false);
  ASSERT_EQ(speaker.state, RUNNING);
  // finish() leaves the state alone: the output still plays its buffered audio
  speaker.loop(true, false);
  EXPECT_EQ(speaker.state, RUNNING);
  speaker.loop(false, false);  // output stopping
  EXPECT_EQ(speaker.state, RUNNING);
  speaker.loop(false, true);
  EXPECT_EQ(speaker.state, STOPPED);
}

TEST(AudioAnalyzerPassThrough, StopFollowsTheOutputToStopped) {
  PassThrough speaker;
  speaker.start();
  speaker.loop(true, false);
  speaker.stop();
  EXPECT_EQ(speaker.state, STOPPING);
  speaker.loop(false, false);
  EXPECT_EQ(speaker.state, STOPPING);
  speaker.loop(false, true);
  EXPECT_EQ(speaker.state, STOPPED);
}

TEST(AudioAnalyzerPassThrough, RestartWhileStoppingIsANewStream) {
  PassThrough speaker;
  speaker.start();
  speaker.loop(true, false);
  speaker.stop();
  speaker.play();
  EXPECT_EQ(speaker.state, STARTING);
  EXPECT_TRUE(speaker.new_stream);
}

TEST(AudioAnalyzerPassThrough, StartWhileRunningContinuesTheStream) {
  PassThrough speaker;
  speaker.start();
  speaker.loop(true, false);
  speaker.start();
  EXPECT_EQ(speaker.state, RUNNING);
  EXPECT_FALSE(speaker.new_stream);
}

TEST(AudioAnalyzerPassThrough, FailedStartTimesOut) {
  PassThrough speaker;
  speaker.start();
  speaker.loop(false, true);
  EXPECT_EQ(speaker.state, STARTING);  // the output hasn't processed its start yet
  speaker.loop(false, true, true);
  EXPECT_EQ(speaker.state, STOPPING);  // the speaker stops the output
  speaker.loop(false, true);
  EXPECT_EQ(speaker.state, STOPPED);
  speaker.play();
  EXPECT_EQ(speaker.state, STARTING);
}

TEST(AudioAnalyzerPassThrough, StoppedIgnoresARunningOutput) {
  // Only start() or play() start a stream; an output started by someone else doesn't
  EXPECT_EQ(follow_output(STOPPED, true, false, false), STOPPED);
}

TEST(AudioAnalyzerSync, NothingHeardWhileUncopiedFramesPlay) {
  // 3000 frames went to the output before the copy started, then 300 samples (900 frames at /3) were copied: the
  // copied audio is only heard once the uncopied frames have played
  EXPECT_EQ(samples_heard(300, 3, 3000 + 900), 0u);
  // 2000 of the uncopied frames played: still nothing
  EXPECT_EQ(samples_heard(300, 3, 1000 + 900), 0u);
  // All uncopied frames and 300 copied frames played: 100 samples heard
  EXPECT_EQ(samples_heard(300, 3, 600), 100u);
  // Everything played
  EXPECT_EQ(samples_heard(300, 3, 0), 300u);
}

TEST(AudioAnalyzerSync, PartialDecimationGroupIsNotHeard) {
  // 2 of a sample's 3 frames played: not yet
  EXPECT_EQ(samples_heard(10, 3, 30 - 2), 0u);
  EXPECT_EQ(samples_heard(10, 3, 30 - 3), 1u);
}

// Collects what FrameAssembler hands on
struct Collector {
  std::vector<uint8_t> bytes;
  uint32_t frames{0};
  uint32_t add(FrameAssembler &assembler, const std::vector<uint8_t> &data, size_t length, size_t frame_bytes) {
    return assembler.add(data.data(), length, frame_bytes, [this, frame_bytes](const uint8_t *f, uint32_t count) {
      this->bytes.insert(this->bytes.end(), f, f + count * frame_bytes);
      this->frames += count;
    });
  }
};

TEST(AudioAnalyzerFrames, WritesEndingMidFrameKeepTheStreamAligned) {
  // Stereo 16-bit (4-byte frames) written as 7, 6 and 3 bytes: 4 whole frames, in order
  std::vector<uint8_t> stream(16);
  for (uint8_t i = 0; i < stream.size(); ++i) {
    stream[i] = i;
  }
  FrameAssembler assembler;
  Collector out;
  EXPECT_EQ(out.add(assembler, {stream.begin(), stream.begin() + 7}, 7, 4), 1u);
  EXPECT_EQ(out.add(assembler, {stream.begin() + 7, stream.begin() + 13}, 6, 4), 2u);
  EXPECT_EQ(out.add(assembler, {stream.begin() + 13, stream.end()}, 3, 4), 1u);
  EXPECT_EQ(out.frames, 4u);
  EXPECT_EQ(out.bytes, stream);
}

TEST(AudioAnalyzerFrames, ByteAtATime) {
  FrameAssembler assembler;
  Collector out;
  uint32_t frames = 0;
  for (uint8_t i = 0; i < 12; ++i) {
    frames += out.add(assembler, {i}, 1, 3);
  }
  EXPECT_EQ(frames, 4u);
  EXPECT_EQ(out.bytes.size(), 12u);
  EXPECT_EQ(out.bytes[11], 11);
}

TEST(AudioAnalyzerFrames, ResetDropsThePartialFrame) {
  FrameAssembler assembler;
  Collector out;
  out.add(assembler, {1, 2, 3}, 3, 2);
  assembler.reset();
  EXPECT_EQ(out.add(assembler, {7, 8}, 2, 2), 1u);
  EXPECT_EQ(out.bytes, (std::vector<uint8_t>{1, 2, 7, 8}));
}

}  // namespace esphome::audio_analyzer::testing
