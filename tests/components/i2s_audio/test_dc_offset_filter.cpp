#include <gtest/gtest.h>

#include "esphome/components/i2s_audio/microphone/dc_offset_filter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace esphome::i2s_audio::testing {

static constexpr int32_t AMPLITUDE = 1 << 28;

static std::vector<uint8_t> constant_pcm(const audio::AudioStreamInfo &info, size_t frames, int32_t value) {
  std::vector<uint8_t> data(info.frames_to_bytes(frames));
  const size_t width = info.samples_to_bytes(1);
  for (size_t offset = 0; offset < data.size(); offset += width) {
    audio::pack_q31_as_audio_sample(value, data.data() + offset, width);
  }
  return data;
}

TEST(DCOffsetFilter, SettlingTimeDoesNotDependOnSampleRate) {
  for (uint32_t rate : {1600, 8000, 16000, 44100, 48000, 96000}) {
    SCOPED_TRACE(rate);
    audio::AudioStreamInfo info(32, 1, rate);
    auto data = constant_pcm(info, rate / 20, AMPLITUDE);
    DCOffsetFilter filter;
    filter.reset(rate);
    filter.process(data.data(), data.size(), info);
    const double residual = audio::unpack_audio_sample_to_q31(data.data() + data.size() - 4, 4) / double(AMPLITUDE);
    EXPECT_NEAR(residual, std::exp(-2.0 * M_PI * 20.0 * 0.05), 0.0001);
  }
}

TEST(DCOffsetFilter, PreservesPassbandGain) {
  for (uint32_t rate : {1600, 16000, 48000, 96000}) {
    SCOPED_TRACE(rate);
    audio::AudioStreamInfo info(32, 1, rate);
    auto data = constant_pcm(info, rate, 0);
    const double frequency = rate / 4.0;
    for (size_t frame = 0; frame < rate; ++frame) {
      const int32_t input = static_cast<int32_t>(AMPLITUDE * std::sin(2.0 * M_PI * frequency * frame / rate));
      audio::pack_q31_as_audio_sample(input, data.data() + 4 * frame, 4);
    }
    DCOffsetFilter filter;
    filter.reset(rate);
    filter.process(data.data(), data.size(), info);
    double energy = 0;
    for (size_t frame = rate / 2; frame < rate; ++frame) {
      const double output = audio::unpack_audio_sample_to_q31(data.data() + 4 * frame, 4) / double(AMPLITUDE);
      energy += output * output;
    }
    EXPECT_NEAR(std::sqrt(energy / (rate / 2) * 2), 1.0, 0.001);
  }
}

TEST(DCOffsetFilter, StereoChannelsMatchIndependentMonoFilters) {
  audio::AudioStreamInfo stereo_info(32, 2, 16000);
  audio::AudioStreamInfo mono_info(32, 1, 16000);
  auto left = constant_pcm(mono_info, 1600, AMPLITUDE);
  auto right = constant_pcm(mono_info, 1600, -AMPLITUDE / 2);
  auto stereo = constant_pcm(stereo_info, 1600, 0);
  for (size_t frame = 0; frame < 1600; ++frame) {
    std::copy_n(left.data() + 4 * frame, 4, stereo.data() + 8 * frame);
    std::copy_n(right.data() + 4 * frame, 4, stereo.data() + 8 * frame + 4);
  }
  DCOffsetFilter stereo_filter, left_filter, right_filter;
  for (auto *filter : {&stereo_filter, &left_filter, &right_filter}) {
    filter->reset(16000);
  }
  stereo_filter.process(stereo.data(), stereo.size(), stereo_info);
  left_filter.process(left.data(), left.size(), mono_info);
  right_filter.process(right.data(), right.size(), mono_info);
  for (size_t frame = 0; frame < 1600; ++frame) {
    EXPECT_EQ(audio::unpack_audio_sample_to_q31(stereo.data() + 8 * frame, 4),
              audio::unpack_audio_sample_to_q31(left.data() + 4 * frame, 4));
    EXPECT_EQ(audio::unpack_audio_sample_to_q31(stereo.data() + 8 * frame + 4, 4),
              audio::unpack_audio_sample_to_q31(right.data() + 4 * frame, 4));
  }
}

TEST(DCOffsetFilter, NyquistGainIsUnity) {
  audio::AudioStreamInfo info(32, 1, 16000);
  auto data = constant_pcm(info, 16000, AMPLITUDE);
  for (size_t frame = 1; frame < 16000; frame += 2) {
    audio::pack_q31_as_audio_sample(-AMPLITUDE, data.data() + 4 * frame, 4);
  }
  DCOffsetFilter filter;
  filter.reset(16000);
  filter.process(data.data(), data.size(), info);
  EXPECT_NEAR(audio::unpack_audio_sample_to_q31(data.data() + data.size() - 4, 4) / double(-AMPLITUDE), 1.0, 0.000001);
}

TEST(DCOffsetFilter, BufferBoundariesDoNotChangeTheResult) {
  for (uint8_t bits : {8, 16, 24, 32}) {
    SCOPED_TRACE(bits);
    audio::AudioStreamInfo info(bits, 2, 16000);
    auto whole = constant_pcm(info, 1600, AMPLITUDE);
    auto split = whole;
    DCOffsetFilter whole_filter, split_filter;
    whole_filter.reset(16000);
    split_filter.reset(16000);
    whole_filter.process(whole.data(), whole.size(), info);
    const size_t boundary = info.frames_to_bytes(137);
    split_filter.process(split.data(), boundary, info);
    split_filter.process(nullptr, 0, info);
    split_filter.process(split.data() + boundary, split.size() - boundary, info);
    EXPECT_EQ(whole, split);
    EXPECT_LE(std::abs(audio::unpack_audio_sample_to_q31(whole.data() + whole.size() - bits / 8, bits / 8)),
              std::max<int64_t>(2000, int64_t{1} << (32 - bits)));
  }
}

TEST(DCOffsetFilter, ClipsFullScaleStepsWithoutOverflowOrWrapping) {
  audio::AudioStreamInfo info(32, 1, 16000);
  DCOffsetFilter filter;
  for (int32_t initial : {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}) {
    filter.reset(16000);
    auto data = constant_pcm(info, 1600, initial);
    filter.process(data.data(), data.size(), info);
    const int32_t opposite = initial < 0 ? std::numeric_limits<int32_t>::max() : std::numeric_limits<int32_t>::min();
    data = constant_pcm(info, 1600, opposite);
    filter.process(data.data(), data.size(), info);
    EXPECT_EQ(audio::unpack_audio_sample_to_q31(data.data(), 4), opposite);
    EXPECT_LT(std::abs(int64_t{audio::unpack_audio_sample_to_q31(data.data() + data.size() - 4, 4)}), 20000);
  }
}

TEST(DCOffsetFilter, ResetDiscardsPreviousCaptureHistory) {
  audio::AudioStreamInfo info(32, 2, 16000);
  auto original = constant_pcm(info, 100, AMPLITUDE);
  DCOffsetFilter filter;
  filter.reset(96000);
  auto previous = original;
  filter.process(previous.data(), previous.size(), info);
  filter.reset(16000);
  auto restarted = original;
  filter.process(restarted.data(), restarted.size(), info);
  DCOffsetFilter fresh;
  fresh.reset(16000);
  fresh.process(original.data(), original.size(), info);
  EXPECT_EQ(restarted, original);
}

}  // namespace esphome::i2s_audio::testing
