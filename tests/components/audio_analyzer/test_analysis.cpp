#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/analysis.h"

namespace esphome::audio_analyzer::testing {

static constexpr double TWO_PI = 6.283185307179586;

static float mean_square_db(float amplitude) { return 10.0f * log10f(amplitude * amplitude / 2.0f); }

// Feeds whole frames of a sine at the analysis rate
static void feed_sine(SpectrumAnalysis &analysis, float frequency, float amplitude, uint32_t frames) {
  const float rate = analysis.get_analysis_rate();
  for (uint32_t i = 0; i < frames * FFT_SIZE; ++i) {
    analysis.push(amplitude * static_cast<float>(std::sin(TWO_PI * frequency * i / rate)));
  }
}

static SpectrumAnalysis unsmoothed(float rate = 16000.0f) {
  SpectrumAnalysis analysis;
  analysis.set_attack(0);
  analysis.set_release(0);
  analysis.configure(rate);
  return analysis;
}

TEST(AudioAnalyzerDecimation, FactorBringsRateToAtMost16k) {
  struct Case {
    uint32_t rate;
    uint8_t factor;
  };
  for (const Case &c : {Case{8000, 1}, Case{16000, 1}, Case{22050, 2}, Case{24000, 2}, Case{32000, 2}, Case{44100, 3},
                        Case{48000, 3}, Case{96000, 6}}) {
    EXPECT_EQ(decimation_for(c.rate), c.factor) << c.rate;
    Decimator decimator;
    decimator.configure(c.rate);
    EXPECT_LE(decimator.get_output_rate(), 16000.0f) << c.rate;
  }
}

TEST(AudioAnalyzerSpectrum, StartsSilent) {
  SpectrumAnalysis analysis;
  EXPECT_FLOAT_EQ(analysis.get_power_db(), FLOOR_DB);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    EXPECT_FLOAT_EQ(analysis.get_band_db(b), FLOOR_DB);
    EXPECT_FLOAT_EQ(analysis.get_band_level(b), 0.0f);
  }
  EXPECT_TRUE(analysis.is_released());
}

TEST(AudioAnalyzerSpectrum, SineLandsInItsBand) {
  SpectrumAnalysis analysis = unsmoothed();
  // Band 9 of the default 100-4000 Hz plan spans about 1590-2160 Hz; 59 * 31.25 Hz is a bin centre inside it
  const auto &bins = analysis.get_band_bins();
  ASSERT_LE(bins[9], 59);
  ASSERT_GT(bins[10], 59);
  feed_sine(analysis, 59 * 31.25f, 0.5f, 2);

  const float expected_db = mean_square_db(0.5f);  // -9.03 dBFS
  EXPECT_NEAR(analysis.get_band_db(9), expected_db, 0.5f);
  EXPECT_LT(analysis.get_band_db(8), expected_db - 20.0f);
  EXPECT_LT(analysis.get_band_db(10), expected_db - 20.0f);
}

TEST(AudioAnalyzerSpectrum, PowerIsMeanSquareInDb) {
  // Same value as sound_level's RMS sensor for the same samples
  SpectrumAnalysis analysis = unsmoothed();
  feed_sine(analysis, 1000.0f, 0.25f, 1);
  EXPECT_NEAR(analysis.get_power_db(), mean_square_db(0.25f), 0.05f);
}

TEST(AudioAnalyzerSpectrum, BandsAddUpToBroadbandPower) {
  // Parseval: the bands of a sine sum to its power wherever it falls
  SpectrumAnalysis analysis = unsmoothed();
  feed_sine(analysis, 1000.0f, 0.5f, 1);
  float total = 0.0f;
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    total += powf(10.0f, analysis.get_band_db(b) / 10.0f);
  }
  EXPECT_NEAR(10.0f * log10f(total), mean_square_db(0.5f), 0.5f);
}

TEST(AudioAnalyzerSpectrum, LevelsMapTheirRange) {
  SpectrumAnalysis analysis = unsmoothed();
  analysis.set_power_range(-20.0f, 0.0f);
  feed_sine(analysis, 1000.0f, 0.5f, 1);  // -9.03 dBFS
  EXPECT_NEAR(analysis.get_power_level(), (-9.03f + 20.0f) / 20.0f, 0.01f);
  EXPECT_FLOAT_EQ(SpectrumAnalysis::normalize(-120.0f, -80.0f, -30.0f), 0.0f);
  EXPECT_FLOAT_EQ(SpectrumAnalysis::normalize(0.0f, -80.0f, -30.0f), 1.0f);
}

TEST(AudioAnalyzerSpectrum, BandEdgesMonotonicAndNonEmpty) {
  for (float rate : {16000.0f, 14700.0f, 12000.0f, 11025.0f, 8000.0f}) {
    SpectrumAnalysis analysis;
    analysis.configure(rate);
    const auto &bins = analysis.get_band_bins();
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
      EXPECT_LT(bins[b], bins[b + 1]) << "rate " << rate << " band " << int(b);
    }
    EXPECT_LE(bins[NUM_BANDS], FFT_SIZE / 2);
  }
}

TEST(AudioAnalyzerSpectrum, MaxFrequencyClampedBelowNyquist) {
  SpectrumAnalysis analysis;
  EXPECT_TRUE(analysis.configure(16000.0f));
  EXPECT_FLOAT_EQ(analysis.get_upper_frequency(), 4000.0f);
  // 8 kHz: bands end at 0.4 * 8000 Hz
  EXPECT_FALSE(analysis.configure(8000.0f));
  EXPECT_FLOAT_EQ(analysis.get_upper_frequency(), 3200.0f);
  EXPECT_LE(analysis.get_band_bins()[NUM_BANDS] * 8000.0f / FFT_SIZE, 3200.0f + 8000.0f / FFT_SIZE);
}

TEST(AudioAnalyzerSpectrum, AttackAndReleaseSteps) {
  SpectrumAnalysis analysis;
  analysis.set_attack(30);
  analysis.set_release(250);
  analysis.configure(16000.0f);
  const float frame = analysis.get_frame_seconds();  // 32 ms

  feed_sine(analysis, 1000.0f, 0.5f, 1);
  const float target = mean_square_db(0.5f);
  const float attack = 1.0f - expf(-frame * 1000.0f / 30.0f);
  EXPECT_NEAR(analysis.get_power_db(), FLOOR_DB + (target - FLOOR_DB) * attack, 0.1f);

  const float before = analysis.get_power_db();
  analysis.release(frame);
  const float release = 1.0f - expf(-frame * 1000.0f / 250.0f);
  EXPECT_NEAR(analysis.get_power_db(), before + (FLOOR_DB - before) * release, 0.1f);
}

TEST(AudioAnalyzerSpectrum, ReleaseReachesSilence) {
  SpectrumAnalysis analysis;
  analysis.configure(16000.0f);
  feed_sine(analysis, 1000.0f, 0.5f, 4);
  EXPECT_FALSE(analysis.is_released());
  for (int i = 0; i < 200 && !analysis.is_released(); ++i) {
    analysis.release(analysis.get_frame_seconds());
  }
  EXPECT_TRUE(analysis.is_released());
  EXPECT_NEAR(analysis.get_power_db(), FLOOR_DB, 0.1f);
  EXPECT_FLOAT_EQ(analysis.get_band_level(5), 0.0f);
}

// Decimates a 48 kHz sine into the analysis
static SpectrumAnalysis analyze_48k_sine(float frequency) {
  Decimator decimator;
  decimator.configure(48000);
  SpectrumAnalysis analysis = unsmoothed(decimator.get_output_rate());
  for (uint32_t i = 0; i < 3 * 3 * FFT_SIZE; ++i) {
    float out;
    if (decimator.push(0.5f * static_cast<float>(std::sin(TWO_PI * frequency * i / 48000.0)), out)) {
      analysis.push(out);
    }
  }
  return analysis;
}

TEST(AudioAnalyzerDecimation, PassbandKeepsLevel) {
  // 2 kHz at 48 kHz, decimated by 3: within 1 dB of its power
  SpectrumAnalysis analysis = analyze_48k_sine(2000.0f);
  EXPECT_NEAR(analysis.get_power_db(), mean_square_db(0.5f), 1.0f);
}

TEST(AudioAnalyzerDecimation, AliasesAreAttenuated) {
  // 12 kHz folds to 4 kHz at the 16 kHz analysis rate; the top band (about 2.9-4 kHz) must stay 30 dB down
  SpectrumAnalysis analysis = analyze_48k_sine(12000.0f);
  EXPECT_LT(analysis.get_band_db(NUM_BANDS - 1), mean_square_db(0.5f) - 30.0f);
  EXPECT_LT(analysis.get_power_db(), mean_square_db(0.5f) - 30.0f);
}

TEST(AudioAnalyzerSamples, ReadsEachSampleWidth) {
  const uint8_t s8[] = {0x40};
  EXPECT_FLOAT_EQ(read_mono_sample(s8, 1, 1), 0.5f);
  const uint8_t s16[] = {0x00, 0xC0};  // -16384
  EXPECT_FLOAT_EQ(read_mono_sample(s16, 2, 1), -0.5f);
  const uint8_t s24[] = {0x00, 0x00, 0x40};
  EXPECT_FLOAT_EQ(read_mono_sample(s24, 3, 1), 0.5f);
  const uint8_t s32[] = {0x00, 0x00, 0x00, 0xC0};
  EXPECT_FLOAT_EQ(read_mono_sample(s32, 4, 1), -0.5f);
}

TEST(AudioAnalyzerSamples, StereoIsDownmixed) {
  // Left silent, right at 0.5: a right-only signal is not lost
  const uint8_t frame[] = {0x00, 0x00, 0x00, 0x40};
  EXPECT_FLOAT_EQ(read_mono_sample(frame, 2, 2), 0.25f);
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

}  // namespace esphome::audio_analyzer::testing
