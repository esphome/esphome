#include <gtest/gtest.h>

#include "esphome/components/i2s_aec_microphone/reference_probe.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace esphome::i2s_aec_microphone::testing {

TEST(ReferenceProbe, FindsTheRawEchoLagAndMeasuresCoherentSuppression) {
  constexpr int samples = ReferenceProbe::WINDOW_FRAMES * 512;
  constexpr int history = 64;
  std::array<int16_t, samples + history> reference{};
  uint32_t state = 17;
  for (auto &sample : reference) {
    state = state * 1664525U + 1013904223U;
    sample = static_cast<int16_t>((state >> 20) & 0x1FFF) - 4096;
  }

  for (const int true_lag : {1, 5, 8}) {
    ReferenceProbe probe;
    for (int frame = 0; frame < ReferenceProbe::WINDOW_FRAMES; ++frame) {
      for (int i = frame % ReferenceProbe::SAMPLE_STRIDE; i < 512; i += ReferenceProbe::SAMPLE_STRIDE) {
        const int position = history + frame * 512 + i;
        const int16_t raw = reference[position - true_lag] / 4;
        const int16_t clean = raw / 10;
        for (int lag = 0; lag < ReferenceProbe::LAG_COUNT; ++lag) {
          const int offset = ReferenceProbe::LAG_OFFSETS[lag];
          probe.add_before(lag, reference[position - offset], raw);
          probe.add_after(lag, reference[position - offset], clean);
        }
      }
    }
    const auto result = probe.result();
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(ReferenceProbe::LAG_OFFSETS[result.lag_index], true_lag);
    EXPECT_GT(result.raw_correlation_squared, 0.99f);
    EXPECT_NEAR(result.remaining, 0.1f, 0.01f);
  }
}

TEST(ReferenceProbe, RejectsSilentReference) {
  ReferenceProbe probe;
  for (int i = 0; i < 64; ++i) {
    probe.add_before(2, 0, 1000);
    probe.add_after(2, 0, 100);
  }
  EXPECT_FALSE(probe.result().valid);
}

TEST(ReferenceProbe, ReportsNoSuppressionForUnrelatedUnchangedMicrophoneSignal) {
  ReferenceProbe probe;
  for (int i = 0; i < 64; ++i) {
    const int16_t reference = (i & 1) != 0 ? 1000 : -1000;
    const int16_t raw = (i & 2) != 0 ? 1000 : -1000;
    probe.add_before(2, reference, raw);
    probe.add_after(2, reference, raw);
  }
  const auto result = probe.result();
  EXPECT_TRUE(result.valid);
  EXPECT_NEAR(result.remaining, 1.0f, 0.001f);
}

TEST(ReferenceProbe, StabilizesVanishingRawProjection) {
  ReferenceProbe probe;
  for (int i = 0; i < 64; ++i) {
    const int16_t reference = (i & 1) != 0 ? 1000 : -1000;
    const int16_t raw = (i & 2) != 0 ? 1000 : -1000;
    probe.add_before(3, reference, raw);
    probe.add_after(3, reference, reference / 4);
  }
  const auto result = probe.result();
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(std::isfinite(result.remaining));
  EXPECT_GT(result.remaining, 1.0f);
}

TEST(ReferenceProbe, ReportsAmplificationInsteadOfClampingToOne) {
  ReferenceProbe probe;
  for (int i = 0; i < 64; ++i) {
    const int16_t reference = (i & 1) != 0 ? 1000 : -1000;
    probe.add_before(3, reference, reference / 2);
    probe.add_after(3, reference, reference);
  }
  const auto result = probe.result();
  EXPECT_TRUE(result.valid);
  EXPECT_NEAR(result.remaining, 2.0f, 0.001f);
}

}  // namespace esphome::i2s_aec_microphone::testing
