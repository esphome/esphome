#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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
  EXPECT_FLOAT_EQ(analysis.get_rms_db(), FLOOR_DB);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    EXPECT_FLOAT_EQ(analysis.get_band_db(b), FLOOR_DB);
    EXPECT_FLOAT_EQ(analysis.get_band_level(b), 0.0f);
  }
  EXPECT_TRUE(analysis.is_released());
}

// Band of the default 100-4000 Hz plan at 16 kHz holding an FFT bin
static uint8_t band_of_bin(const SpectrumAnalysis &analysis, float bin) {
  const auto &bins = analysis.get_band_bins();
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    if (bin >= bins[b] - 0.5f && bin < bins[b + 1] - 0.5f) {
      return b;
    }
  }
  return NUM_BANDS;
}

TEST(AudioAnalyzerSpectrum, SineLandsInItsBand) {
  // Tones on a bin centre and halfway between two bins (the worst scalloping), each at least 2 bins from its band's
  // edges, in middle bands and the top band
  for (float bin : {30.0f, 30.5f, 59.0f, 59.5f, 110.0f, 110.5f}) {
    SpectrumAnalysis analysis = unsmoothed();
    const uint8_t band = band_of_bin(analysis, bin);
    ASSERT_LT(band, NUM_BANDS) << bin;
    feed_sine(analysis, bin * 31.25f, 0.5f, 2);

    const float expected_db = mean_square_db(0.5f);  // -9.03 dBFS
    EXPECT_NEAR(analysis.get_band_db(band), expected_db, 0.5f) << bin;
    if (band > 0) {
      EXPECT_LT(analysis.get_band_db(band - 1), expected_db - 20.0f) << bin;
    }
    if (band < NUM_BANDS - 1) {
      EXPECT_LT(analysis.get_band_db(band + 1), expected_db - 20.0f) << bin;
    }
  }
}

TEST(AudioAnalyzerSpectrum, ToneNearABandEdgeLeaksIntoItsNeighbour) {
  // The Hann window's main lobe spreads a tone over 2 bins either side. With the default plan at 16 kHz, band 1 is
  // bins 5-6: a tone centred in it keeps its neighbours 15 dB down, a tone on its first bin shows in band 0 about
  // 8 dB down, and a tone on the boundary is split between the two bands. Nothing reaches two bands away.
  SpectrumAnalysis analysis = unsmoothed();
  ASSERT_EQ(analysis.get_band_bins()[1], 5);
  ASSERT_EQ(analysis.get_band_bins()[2], 7);
  const float expected_db = mean_square_db(0.5f);

  feed_sine(analysis, 5.5f * 31.25f, 0.5f, 2);
  EXPECT_NEAR(analysis.get_band_db(1), expected_db, 0.5f);
  EXPECT_LT(analysis.get_band_db(0), expected_db - 15.0f);
  EXPECT_LT(analysis.get_band_db(2), expected_db - 15.0f);

  analysis = unsmoothed();
  feed_sine(analysis, 5.0f * 31.25f, 0.5f, 2);
  EXPECT_NEAR(analysis.get_band_db(1), expected_db, 1.0f);
  EXPECT_NEAR(analysis.get_band_db(0), expected_db - 7.0f, 1.0f);
  EXPECT_LT(analysis.get_band_db(2), expected_db - 40.0f);

  analysis = unsmoothed();
  feed_sine(analysis, 4.5f * 31.25f, 0.5f, 2);
  EXPECT_NEAR(analysis.get_band_db(0), analysis.get_band_db(1), 0.1f);
  EXPECT_NEAR(analysis.get_band_db(0), expected_db - 3.0f, 0.5f);
}

TEST(AudioAnalyzerSpectrum, RmsIsMeanSquareInDb) {
  // Same value as sound_level's RMS sensor for the same samples
  SpectrumAnalysis analysis = unsmoothed();
  feed_sine(analysis, 1000.0f, 0.25f, 1);
  EXPECT_NEAR(analysis.get_rms_db(), mean_square_db(0.25f), 0.05f);
}

TEST(AudioAnalyzerSpectrum, DcCountsInRmsButNotInBands) {
  SpectrumAnalysis analysis = unsmoothed();
  for (uint32_t i = 0; i < FFT_SIZE; ++i) {
    analysis.push(0.01f);
  }
  EXPECT_NEAR(analysis.get_rms_db(), -40.0f, 0.05f);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    EXPECT_LT(analysis.get_band_db(b), -90.0f) << int(b);
  }
}

TEST(AudioAnalyzerSpectrum, BandsAddUpToBroadbandMeanSquare) {
  // Parseval: the bands of a sine inside the range sum to its mean square, on and between bins
  for (float frequency : {200.0f, 437.5f, 1000.0f, 1015.625f, 2500.0f, 3700.0f}) {
    SpectrumAnalysis analysis = unsmoothed();
    feed_sine(analysis, frequency, 0.5f, 1);
    float total = 0.0f;
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
      total += powf(10.0f, analysis.get_band_db(b) / 10.0f);
    }
    EXPECT_NEAR(10.0f * log10f(total), mean_square_db(0.5f), 0.1f) << frequency;
  }
}

TEST(AudioAnalyzerSpectrum, LevelsMapTheirRange) {
  SpectrumAnalysis analysis = unsmoothed();
  analysis.set_rms_range(-20.0f, 0.0f);
  feed_sine(analysis, 1000.0f, 0.5f, 1);  // -9.03 dBFS
  EXPECT_NEAR(analysis.get_rms_level(), (-9.03f + 20.0f) / 20.0f, 0.01f);
  EXPECT_FLOAT_EQ(SpectrumAnalysis::normalize(-120.0f, -80.0f, -30.0f), 0.0f);
  EXPECT_FLOAT_EQ(SpectrumAnalysis::normalize(0.0f, -80.0f, -30.0f), 1.0f);
}

TEST(AudioAnalyzerSpectrum, DefaultBandPlan) {
  // 100-4000 Hz at 16 kHz: log-spaced, but the lowest bands are widened to 2 bins
  SpectrumAnalysis analysis;
  EXPECT_TRUE(analysis.configure(16000.0f));
  const std::array<uint16_t, NUM_BANDS + 1> expected{3, 5, 7, 9, 11, 14, 20, 27, 37, 50, 68, 94, 128};
  EXPECT_EQ(analysis.get_band_bins(), expected);
  EXPECT_NEAR(analysis.get_lower_edge(), 78.1f, 0.1f);
  EXPECT_NEAR(analysis.get_upper_edge(), 3984.4f, 0.1f);
}

TEST(AudioAnalyzerSpectrum, BandsAtLeastTwoBinsBelowTheLimit) {
  struct Case {
    float min_hz, max_hz;
  };
  for (const Case &c : {Case{100.0f, 4000.0f}, Case{20.0f, 800.0f}, Case{4000.0f, 4800.0f}, Case{100.0f, 4800.0f}}) {
    for (float rate : {16000.0f, 14700.0f, 12000.0f, 11025.0f, 8000.0f}) {
      SpectrumAnalysis analysis;
      analysis.set_frequency_range(c.min_hz, c.max_hz);
      analysis.configure(rate);
      const auto &bins = analysis.get_band_bins();
      EXPECT_GE(bins[0], 1) << c.min_hz << " " << rate;
      for (uint8_t b = 0; b < NUM_BANDS; ++b) {
        EXPECT_GE(bins[b + 1] - bins[b], MIN_BAND_BINS) << c.min_hz << " " << rate << " band " << int(b);
      }
      EXPECT_LE(analysis.get_upper_edge(), MAX_BAND_FRACTION * rate + 0.01f) << c.min_hz << " " << rate;
    }
  }
}

TEST(AudioAnalyzerSpectrum, ClampBelowMinFrequencyMovesTheBottomDown) {
  // 4000-4800 Hz is valid at 16 kHz; an 8 kHz stream only allows bands up to 2400 Hz, below min_frequency
  SpectrumAnalysis analysis;
  analysis.set_frequency_range(4000.0f, 4800.0f);
  EXPECT_TRUE(analysis.configure(16000.0f));
  EXPECT_FALSE(analysis.configure(8000.0f));
  EXPECT_LE(analysis.get_upper_edge(), 2400.0f);
  EXPECT_LT(analysis.get_lower_edge(), analysis.get_upper_edge());
  EXPECT_EQ(analysis.get_band_bins()[NUM_BANDS] - analysis.get_band_bins()[0], NUM_BANDS * MIN_BAND_BINS);
}

TEST(AudioAnalyzerSpectrum, MaxFrequencyClampedForLowRates) {
  SpectrumAnalysis analysis;
  EXPECT_TRUE(analysis.configure(16000.0f));
  // 8 kHz: bands end at 0.3 * 8000 Hz, the bottom stays
  EXPECT_FALSE(analysis.configure(8000.0f));
  EXPECT_LE(analysis.get_upper_edge(), 2400.0f);
  EXPECT_GT(analysis.get_upper_edge(), 2400.0f - 8000.0f / FFT_SIZE);
  EXPECT_NEAR(analysis.get_lower_edge(), 100.0f, 8000.0f / FFT_SIZE);
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
  EXPECT_NEAR(analysis.get_rms_db(), FLOOR_DB + (target - FLOOR_DB) * attack, 0.1f);

  const float before = analysis.get_rms_db();
  analysis.release(frame);
  const float release = 1.0f - expf(-frame * 1000.0f / 250.0f);
  EXPECT_NEAR(analysis.get_rms_db(), before + (FLOOR_DB - before) * release, 0.1f);
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
  EXPECT_NEAR(analysis.get_rms_db(), FLOOR_DB, 0.1f);
  EXPECT_FLOAT_EQ(analysis.get_band_level(5), 0.0f);
}

// Decimates a sine into an unsmoothed analysis with the highest band edge the stream allows
static SpectrumAnalysis analyze_sine(uint32_t rate, double frequency, float min_hz = 100.0f) {
  Decimator decimator;
  decimator.configure(rate);
  SpectrumAnalysis analysis;
  analysis.set_attack(0);
  analysis.set_release(0);
  analysis.set_frequency_range(min_hz, MAX_BAND_FRACTION * decimator.get_output_rate());
  analysis.configure(decimator.get_output_rate());
  for (uint32_t i = 0; i < 4 * FFT_SIZE * decimator.get_factor(); ++i) {
    float out;
    if (decimator.push(0.5f * static_cast<float>(std::sin(TWO_PI * frequency * i / rate)), out)) {
      analysis.push(out);
    }
  }
  return analysis;
}

TEST(AudioAnalyzerDecimation, TopBandMatchesUndecimatedStream) {
  // A tone near the highest allowed band edge reads the same from a 16 kHz stream (no filter) and from 32 and 48 kHz
  // streams decimated to 16 kHz; 44.1 kHz streams are analyzed at 14.7 kHz, with their own limit
  for (double fraction : {0.9, 0.97}) {
    const double frequency = fraction * MAX_BAND_FRACTION * 16000.0;
    const SpectrumAnalysis direct = analyze_sine(16000, frequency);
    for (uint32_t rate : {32000u, 48000u}) {
      const SpectrumAnalysis decimated = analyze_sine(rate, frequency);
      EXPECT_NEAR(decimated.get_band_db(NUM_BANDS - 1), direct.get_band_db(NUM_BANDS - 1), 0.1f) << rate;
      EXPECT_NEAR(decimated.get_rms_db(), direct.get_rms_db(), 0.1f) << rate;
    }
    const SpectrumAnalysis cd = analyze_sine(44100, fraction * MAX_BAND_FRACTION * 14700.0);
    EXPECT_NEAR(cd.get_band_db(NUM_BANDS - 1), mean_square_db(0.5f), 0.1f);
  }
}

TEST(AudioAnalyzerDecimation, AliasesAreAttenuatedEverywhere) {
  // Every input tone that folds into 0 Hz to the highest allowed band edge (the worst case lands on that edge) stays
  // more than 45 dB down, in the RMS and in every band
  const float limit_db = mean_square_db(0.5f) - 45.0f;
  for (uint32_t rate : {22050u, 32000u, 44100u, 48000u}) {
    const uint8_t factor = decimation_for(rate);
    const double out = static_cast<double>(rate) / factor;
    const double top = MAX_BAND_FRACTION * out;
    for (double frequency = out - top; frequency < rate / 2.0; frequency += 37.0) {
      double folded = std::fmod(frequency, out);
      folded = std::min(folded, out - folded);
      if (folded > top) {
        continue;
      }
      const SpectrumAnalysis analysis = analyze_sine(rate, frequency);
      EXPECT_LT(analysis.get_rms_db(), limit_db) << rate << " " << frequency;
      for (uint8_t b = 0; b < NUM_BANDS; ++b) {
        EXPECT_LT(analysis.get_band_db(b), limit_db) << rate << " " << frequency << " band " << int(b);
      }
    }
  }
}

TEST(AudioAnalyzerDecimation, ResetClearsTheFilter) {
  Decimator decimator;
  decimator.configure(48000);
  float out = 0.0f;
  for (int i = 0; i < 30; ++i) {
    decimator.push(1.0f, out);
  }
  decimator.reset();
  float first = 1.0f;
  for (int i = 0; i < 3; ++i) {
    decimator.push(0.0f, first);
  }
  EXPECT_FLOAT_EQ(first, 0.0f);
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

}  // namespace esphome::audio_analyzer::testing
