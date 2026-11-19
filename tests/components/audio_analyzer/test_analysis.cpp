#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

#include "esphome/components/audio_analyzer/analysis.h"
#include "test_helpers.h"

namespace esphome::audio_analyzer::testing {

// Feeds whole frames of a sine at the analysis rate
static void feed_sine(SpectrumAnalysis &analysis, float frequency, float amplitude, uint32_t frames) {
  const float rate = analysis.get_analysis_rate();
  for (uint32_t i = 0; i < frames * FFT_SIZE; ++i) {
    analysis.push(amplitude * static_cast<float>(std::sin(TWO_PI * frequency * i / rate)));
  }
}

// Default band plan at 16 kHz, no smoothing
static SpectrumAnalysis unsmoothed() {
  SpectrumAnalysis analysis;
  analysis.set_band_plan(16000.0f, DEFAULT_PLAN);
  analysis.set_smoothing(1.0f, 1.0f);
  return analysis;
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

TEST(AudioAnalyzerSpectrum, StartsSilent) {
  const SpectrumAnalysis analysis = unsmoothed();
  EXPECT_FLOAT_EQ(analysis.get_rms_db(), FLOOR_DB);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    EXPECT_FLOAT_EQ(analysis.get_band_db(b), FLOOR_DB);
    EXPECT_FLOAT_EQ(analysis.get_band_level(b), 0.0f);
  }
  EXPECT_TRUE(analysis.is_released());
}

TEST(AudioAnalyzerSpectrum, BandEdgesAndFrame) {
  const SpectrumAnalysis analysis = unsmoothed();
  EXPECT_NEAR(analysis.get_lower_edge(), 78.1f, 0.1f);
  EXPECT_NEAR(analysis.get_upper_edge(), 3984.4f, 0.1f);
  EXPECT_EQ(analysis.get_frame_ms(), 32u);
}

// Band of the default plan holding an FFT bin
static uint8_t band_of_bin(float bin) {
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    if (bin >= DEFAULT_PLAN[b] - 0.5f && bin < DEFAULT_PLAN[b + 1] - 0.5f) {
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
    const uint8_t band = band_of_bin(bin);
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
  // The Hann window's main lobe spreads a tone over 2 bins either side. Band 1 is bins 5-6: a tone centred in it keeps
  // its neighbours 15 dB down, a tone on its first bin shows in band 0 about 8 dB down, and a tone on the boundary is
  // split between the two bands. Nothing reaches two bands away.
  const float expected_db = mean_square_db(0.5f);

  SpectrumAnalysis analysis = unsmoothed();
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

TEST(AudioAnalyzerSpectrum, AttackAndReleaseSteps) {
  SpectrumAnalysis analysis = unsmoothed();
  analysis.set_smoothing(0.6f, 0.1f);
  feed_sine(analysis, 1000.0f, 0.5f, 1);
  const float target = mean_square_db(0.5f);
  EXPECT_NEAR(analysis.get_rms_db(), FLOOR_DB + (target - FLOOR_DB) * 0.6f, 0.1f);

  const float before = analysis.get_rms_db();
  analysis.release_step();
  EXPECT_NEAR(analysis.get_rms_db(), before + (FLOOR_DB - before) * 0.1f, 0.1f);
}

TEST(AudioAnalyzerSpectrum, ReleaseReachesSilence) {
  SpectrumAnalysis analysis = unsmoothed();
  analysis.set_smoothing(0.6f, 0.1f);
  feed_sine(analysis, 1000.0f, 0.5f, 4);
  EXPECT_FALSE(analysis.is_released());
  for (int i = 0; i < 200 && !analysis.is_released(); ++i) {
    analysis.release_step();
  }
  EXPECT_TRUE(analysis.is_released());
  EXPECT_NEAR(analysis.get_rms_db(), FLOOR_DB, 0.1f);
  EXPECT_FLOAT_EQ(analysis.get_band_level(5), 0.0f);
}

TEST(AudioAnalyzerDecimation, PassesUnchangedWithoutAFactor) {
  Decimator decimator;
  float out = 0.0f;
  EXPECT_TRUE(decimator.push(0.25f, out));
  EXPECT_FLOAT_EQ(out, 0.25f);
}

// Gain of the reference filter at a frequency (fraction of the input rate), from its coefficients
static double filter_gain(uint8_t factor, double fraction) {
  const std::complex<double> z1 = std::polar(1.0, -TWO_PI * fraction);
  const std::complex<double> z2 = z1 * z1;
  double gain = 1.0;
  for (const Decimator::Section &s : reference_sections(factor)) {
    gain *= std::abs(static_cast<double>(s.b0) * (1.0 + 2.0 * z1 + z2) /
                     (1.0 + static_cast<double>(s.a1) * z1 + static_cast<double>(s.a2) * z2));
  }
  return gain;
}

TEST(AudioAnalyzerDecimation, FilterRealizesItsTransferFunction) {
  // The float filter's output level matches the analytic response of its coefficients, in the passband and in the
  // stopband, and every third sample is kept
  for (double frequency : {1000.0, 4000.0, 7000.0, 12100.0, 19900.0}) {
    Decimator decimator;
    decimator.configure(3, reference_sections(3));
    double squares = 0.0;
    uint32_t kept = 0;
    for (uint32_t i = 0; i < 48000; ++i) {
      float out;
      if (decimator.push(0.5f * static_cast<float>(std::sin(TWO_PI * frequency * i / 48000.0)), out)) {
        if (i >= 24000) {  // settled
          squares += static_cast<double>(out) * out;
          ++kept;
        }
      }
    }
    EXPECT_EQ(kept, 8000u);
    const double measured_db = 10.0 * std::log10(squares / kept / 0.125);
    const double expected_db = 20.0 * std::log10(filter_gain(3, frequency / 48000.0));
    EXPECT_NEAR(measured_db, expected_db, std::max(0.05, -expected_db * 0.02)) << frequency;
  }
}

TEST(AudioAnalyzerDecimation, ResetClearsTheFilter) {
  Decimator decimator;
  decimator.configure(3, reference_sections(3));
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

// Decimates a 48 kHz sine to 16 kHz into an unsmoothed analysis with the default plan
static SpectrumAnalysis analyze_48k(double frequency) {
  Decimator decimator;
  decimator.configure(3, reference_sections(3));
  SpectrumAnalysis analysis = unsmoothed();
  for (uint32_t i = 0; i < 4 * FFT_SIZE * 3; ++i) {
    float out;
    if (decimator.push(0.5f * static_cast<float>(std::sin(TWO_PI * frequency * i / 48000.0)), out)) {
      analysis.push(out);
    }
  }
  return analysis;
}

TEST(AudioAnalyzerDecimation, TopBandMatchesUndecimatedStream) {
  SpectrumAnalysis direct = unsmoothed();
  feed_sine(direct, 3900.0f, 0.5f, 4);
  const SpectrumAnalysis decimated = analyze_48k(3900.0);
  EXPECT_NEAR(decimated.get_band_db(NUM_BANDS - 1), direct.get_band_db(NUM_BANDS - 1), 0.1f);
  EXPECT_NEAR(decimated.get_rms_db(), direct.get_rms_db(), 0.1f);
}

TEST(AudioAnalyzerDecimation, AliasesOfTheTopBandAreAttenuated) {
  // 12.1 and 19.9 kHz fold onto 3.9 kHz at 16 kHz; test_config.py sweeps the whole folding range of every rate
  for (double frequency : {12100.0, 19900.0}) {
    const SpectrumAnalysis analysis = analyze_48k(frequency);
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
      EXPECT_LT(analysis.get_band_db(b), mean_square_db(0.5f) - 45.0f) << frequency << " band " << int(b);
    }
  }
}

}  // namespace esphome::audio_analyzer::testing
