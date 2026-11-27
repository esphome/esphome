#include <gtest/gtest.h>

#include <limits>

#include "esphome/components/tflite_waveform/tflite_waveform.h"

namespace esphome::tflite_waveform::testing {

TEST(WaveformTest, FullScaleMapsToPlusMinusOne) {
  EXPECT_FLOAT_EQ(pcm_to_float(-32768), -1.0f);
  EXPECT_FLOAT_EQ(pcm_to_float(0), 0.0f);
  EXPECT_FLOAT_EQ(pcm_to_float(16384), 0.5f);
}

TEST(WaveformTest, EverySampleSurvivesARoundTrip) {
  for (int sample = -32768; sample <= 32767; sample++)
    ASSERT_EQ(float_to_pcm(pcm_to_float(static_cast<int16_t>(sample))), sample);
}

TEST(WaveformTest, OutputsBeyondFullScaleSaturate) {
  EXPECT_EQ(float_to_pcm(1.0f), 32767);
  EXPECT_EQ(float_to_pcm(8.0f), 32767);
  EXPECT_EQ(float_to_pcm(-8.0f), -32768);
  EXPECT_EQ(float_to_pcm(std::numeric_limits<float>::infinity()), 32767);
  EXPECT_EQ(float_to_pcm(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(WaveformTest, QuantizationAppliesTheZeroPointAndClamps) {
  EXPECT_EQ(quantize(0.5f, 1.0f / 128.0f, 0, -128, 127), 64);
  EXPECT_EQ(quantize(0.0f, 1.0f / 128.0f, 128, 0, 255), 128);
  EXPECT_EQ(quantize(1.0f, 1.0f / 128.0f, 0, -128, 127), 127);
  EXPECT_EQ(quantize(-2.0f, 1.0f / 128.0f, 0, -128, 127), -128);
}

TEST(WaveformTest, HalfStepsRoundBeforeTheZeroPoint) {
  const float scale = 1.0f / 128.0f;
  const float half_step = pcm_to_float(-128);  // half a quantization step below zero
  EXPECT_EQ(quantize(half_step, scale, 0, -128, 127), -1);
  EXPECT_EQ(quantize(half_step, scale, 128, 0, 255), 127);
}

}  // namespace esphome::tflite_waveform::testing
