#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/gree/gree.h"

namespace esphome::gree::testing {

TEST(GreeClimateTest, HeatCoolHiddenWithoutHeatByDefault) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(GreeClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

// The unit displays trunc(celsiusToFahrenheit(GREE_TEMP_MIN + Temp + 0.5 * TempExtraDegreeF)).
// These expectations come from frames captured off a real YX1FF remote and from the panel of a
// Blueridge mini-split, not from a conversion formula.
TEST(GreeClimateTest, FahrenheitFieldsMatchCapturedRemoteFrames) {
  struct Expectation {
    float celsius;
    uint8_t temp_field;
    bool extra_degree;
  };
  // 18.33 C is how Home Assistant delivers a 65 F request, 26.67 C a 78 F request, and so on.
  const Expectation expectations[] = {
      {18.33f, 2, true},    // 65 F -- captured remote frame b1=02 b3=5C
      {18.89f, 3, false},   // 66 F -- captured remote frame b1=03 b3=58
      {21.67f, 6, false},   // 71 F
      {22.22f, 6, true},    // 72 F
      {25.0f, 9, false},    // 77 F
      {25.56f, 10, false},  // 78 F
  };
  for (const auto &expectation : expectations) {
    uint8_t temp_field = 0xFF;
    bool extra_degree = false;
    fahrenheit_fields(expectation.celsius, temp_field, extra_degree);
    EXPECT_EQ(expectation.temp_field, temp_field) << "celsius " << expectation.celsius;
    EXPECT_EQ(expectation.extra_degree, extra_degree) << "celsius " << expectation.celsius;
  }
}

TEST(GreeClimateTest, FahrenheitFieldsCoverEveryWholeDegreeInRange) {
  // 61..86 F is the range a YX1FF remote offers; each should be exactly reachable.
  for (int fahrenheit = 61; fahrenheit <= 86; fahrenheit++) {
    const float celsius = (static_cast<float>(fahrenheit) - 32.0f) * 5.0f / 9.0f;
    uint8_t temp_field = 0xFF;
    bool extra_degree = false;
    fahrenheit_fields(celsius, temp_field, extra_degree);
    const float encoded = static_cast<float>(GREE_TEMP_MIN + temp_field) + (extra_degree ? 0.5f : 0.0f);
    EXPECT_EQ(fahrenheit, static_cast<int>(encoded * 9.0f / 5.0f + 32.0f)) << "fahrenheit " << fahrenheit;
  }
}

TEST(GreeClimateTest, FahrenheitFieldsHandleNonFiniteTarget) {
  // transmit_state() can run from setup() via restore_state_(), when the target may still be NaN.
  uint8_t temp_field = 0xFF;
  bool extra_degree = true;
  fahrenheit_fields(NAN, temp_field, extra_degree);
  EXPECT_EQ(0, temp_field);
  EXPECT_FALSE(extra_degree);
}

// Checksums of the two frames captured from a real YX1FF remote and verified on the unit's display.
// Byte 7 is zeroed here because frame_checksum() computes it.
TEST(GreeClimateTest, FrameChecksumMatchesCapturedFrames) {
  const uint8_t frame_65f[GREE_STATE_FRAME_SIZE] = {0x09, 0x02, 0x60, 0x5C, 0x00, 0x20, 0x00, 0x00};
  EXPECT_EQ(0x30, frame_checksum(frame_65f));

  const uint8_t frame_78f[GREE_STATE_FRAME_SIZE] = {0x09, 0x0A, 0x60, 0x58, 0x00, 0x20, 0x00, 0x00};
  EXPECT_EQ(0x70, frame_checksum(frame_78f));
}

// The yan/yx1ff checksum that used to live in transmit_state(),
// `(b0 << 4) + (b1 << 4) + 0xC0`, is this same formula with the constants folded in for every frame
// those models emit: bytes 2 and 3 contribute nothing in their low nibbles, bytes 4 and 6 nothing in
// their high nibbles, and byte 5 adds 2, giving 0 + 2 + 0xA = 0xC. Removing it was therefore
// bit-for-bit identical for existing installations; this pins that so it cannot regress silently.
TEST(GreeClimateTest, FrameChecksumMatchesTheRemovedFoldedFormula) {
  int mismatches = 0;
  for (int byte0 = 0; byte0 < 256; byte0++) {
    for (int byte1 = 0; byte1 < 256; byte1++) {
      // byte 2 is 0x00/0x20/0x60 plus an optional turbo bit -- all high-nibble, low nibble always 0
      static const uint8_t BYTE2_VALUES[] = {0x00, 0x20, 0x60, 0x70};
      for (const uint8_t byte2 : BYTE2_VALUES) {
        for (uint8_t byte4 = 0; byte4 <= GREE_VDIR_DOWN; byte4++) {
          const uint8_t frame[GREE_STATE_FRAME_SIZE] = {
              static_cast<uint8_t>(byte0), static_cast<uint8_t>(byte1), byte2, 0x50, byte4, 0x20, 0x00, 0x00};
          const uint8_t folded = static_cast<uint8_t>((byte0 << 4) + (byte1 << 4) + 0xC0);
          if (frame_checksum(frame) != folded) {
            mismatches++;
          }
        }
      }
    }
  }
  EXPECT_EQ(0, mismatches);
}

}  // namespace esphome::gree::testing
