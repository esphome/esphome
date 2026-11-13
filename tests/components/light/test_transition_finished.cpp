#include <gtest/gtest.h>

#include <cstdint>
#include <random>

#include "esphome/components/light/light_transformer.h"

namespace esphome::light::testing {

// Exposes the protected progress helpers; never instantiated
class ProgressProbe : public LightTransformer {
 public:
  using LightTransformer::progress;
  using LightTransformer::reached_end;
};

static void expect_same(uint32_t elapsed, uint32_t length) {
  ASSERT_EQ(ProgressProbe::reached_end(elapsed, length), ProgressProbe::progress(elapsed, length) >= 1.0f)
      << "elapsed=" << elapsed << " length=" << length;
}

TEST(LightTransformerFinished, MatchesProgressAtTheEdges) {
  for (uint32_t length : {0u, 1u, 2u, 1000u}) {
    for (uint32_t elapsed = 0; elapsed <= length + 2; elapsed++)
      expect_same(elapsed, length);
  }
}

TEST(LightTransformerFinished, MatchesProgressWhereFloatRounds) {
  // Lengths above 2^24 no longer fit a float exactly, so elapsed < length can still convert to the same float
  for (uint32_t shift = 23; shift < 32; shift++) {
    for (int32_t offset = -300; offset <= 300; offset++) {
      const uint32_t length = (1u << shift) + static_cast<uint32_t>(offset);
      for (uint32_t back = 0; back <= 300 && back <= length; back++)
        expect_same(length - back, length);
    }
  }
  for (uint32_t back = 0; back <= 4096; back++)
    expect_same(UINT32_MAX - back, UINT32_MAX);
}

TEST(LightTransformerFinished, MatchesProgressForRandomPairs) {
  std::mt19937 rng(12345);
  for (int i = 0; i < 100000; i++) {
    const uint32_t length = rng() >> (rng() % 32);
    const uint32_t elapsed = (i & 1) && length != 0 ? rng() % length : rng();
    expect_same(elapsed, length);
  }
}

}  // namespace esphome::light::testing
